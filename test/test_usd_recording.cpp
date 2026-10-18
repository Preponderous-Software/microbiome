// Tests for UsdRecording, the optional `mb_app --record <path>` history recorder.
// The recorder writes text .usda, so these tests check the stage text directly.
#include "lib/catch2.hpp"

#include "../src/header/appConfig.h"
#include "../src/header/microbiome.h"
#include "../src/header/simulation.h"
#include "../src/header/usdRecording.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

// The text of one entity's prim, from its `def Xform` line to the next entity's.
std::string entityBlock(const std::string& usda, int id) {
    std::string header = "def Xform \"e_" + std::to_string(id) + "\"";
    size_t start = usda.find(header);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = usda.find("def Xform \"e_", start + header.size());
    return usda.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

size_t countOccurrences(const std::string& text, const std::string& needle) {
    size_t count = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
        count++;
    }
    return count;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("A recorded tick holds every living microorganism at its grid coordinates", "[usd]") {
    Microbiome microbiome(0, "Test Microbiome", 3, 1);
    UsdRecording recording;

    recording.recordTick(microbiome, 0);
    std::string usda = recording.toUsda();

    REQUIRE(recording.getTicksRecorded() == 1);
    REQUIRE(countOccurrences(usda, "def Xform \"e_") == 3);
    for (Microorganism* microorganism : microbiome.getMicroorganisms()) {
        Location& location = microbiome.getGrid()->getLocation(microorganism->getLocationId());
        std::string block = entityBlock(usda, microorganism->getId());
        REQUIRE_FALSE(block.empty());
        REQUIRE(contains(block, "0: (" + std::to_string(location.getX()) + ", " + std::to_string(location.getY()) + ", 0)"));
        REQUIRE(contains(block, "0: \"microorganism\""));
        REQUIRE(contains(block, "int extras:energy.timeSamples = {\n                0: " + std::to_string(microorganism->getEnergy()) + ","));
        REQUIRE(contains(block, "0: \"inherited\""));
    }
    REQUIRE(contains(usda, "upAxis = \"Z\""));
    REQUIRE(contains(usda, "metersPerUnit = 1"));
    REQUIRE(contains(usda, "timeCodesPerSecond = 10"));
}

TEST_CASE("A death hides the microorganism and shows its biomatter from that tick", "[usd]") {
    Microbiome microbiome(0, "Test Microbiome", 3, 1);
    for (Microorganism* microorganism : microbiome.getMicroorganisms()) {
        // low enough that any metabolic rate (1-4) kills it this tick
        microorganism->setEnergy(1);
    }
    UsdRecording recording;

    recording.recordTick(microbiome, 0);
    microbiome.initiateMicroorganismMovement();
    recording.recordTick(microbiome, 1);
    std::string usda = recording.toUsda();

    // the three microorganisms (ids 0-2) are hidden at tick 1
    for (int id = 0; id < 3; id++) {
        std::string block = entityBlock(usda, id);
        REQUIRE(contains(block, "0: \"inherited\""));
        REQUIRE(contains(block, "1: \"invisible\""));
    }
    // their biomatter (ids 3-5) is hidden before it existed and shown at tick 1
    for (int id = 3; id < 6; id++) {
        std::string block = entityBlock(usda, id);
        REQUIRE(contains(block, "1: \"biomatter\""));
        REQUIRE(contains(block, "0: \"invisible\""));
        REQUIRE(contains(block, "1: \"inherited\""));
    }
    REQUIRE(contains(usda, "int extras:alive.timeSamples = {\n        0: 3,\n        1: 0,"));
    REQUIRE(contains(usda, "int extras:dead.timeSamples = {\n        0: 0,\n        1: 3,"));
}

TEST_CASE("A recording simulation records its initial state and every tick", "[usd][simulation]") {
    AppConfig config;
    config.setSimulationOutputEnabled(false);
    config.setMaxTicks(10);
    Simulation simulation(&config, 1, "Recorded Simulation");
    UsdRecording recording;
    simulation.setRecording(&recording);

    simulation.run();
    std::string usda = recording.toUsda();

    REQUIRE(recording.getTicksRecorded() == simulation.getTicksElapsed() + 1);
    REQUIRE(contains(usda, "startTimeCode = 0\n"));
    REQUIRE(contains(usda, "endTimeCode = " + std::to_string(simulation.getTicksElapsed()) + "\n"));
}

TEST_CASE("A simulation without a recording runs as before", "[usd][simulation]") {
    AppConfig config;
    config.setSimulationOutputEnabled(false);
    config.setMaxTicks(10);
    Simulation simulation(&config, 1, "Unrecorded Simulation");

    simulation.run();

    REQUIRE(simulation.getTicksElapsed() == 10);
}

TEST_CASE("write() puts the recorded stage at the given path", "[usd]") {
    Microbiome microbiome(0, "Test Microbiome", 2, 1);
    UsdRecording recording;
    recording.recordTick(microbiome, 0);
    std::string path = "mb_usd_test_" + std::to_string(getpid()) + ".usda";

    recording.write(path);

    std::ifstream file(path);
    std::stringstream written;
    written << file.rdbuf();
    REQUIRE(written.str() == recording.toUsda());
    REQUIRE(written.str().rfind("#usda 1.0\n", 0) == 0);
    std::ifstream leftover(path + ".tmp");
    REQUIRE_FALSE(leftover.good());
    std::remove(path.c_str());
}
