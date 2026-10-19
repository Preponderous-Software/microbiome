// Catch2 tests for recording a run as a usd-replay/1 replay (strata/src/replay.*).
#include "catch2.hpp"

#include <cstdio>
#include <string>

#include "micro_environment.h"
#include "replay.h"

using namespace mbstrata;

namespace {

Config smallChamber() {
    Config c;
    c.width = 64;
    c.height = 64;
    c.initialPerSpecies = 80;
    c.grainCount = 10;
    c.seed = 7;
    return c;
}

size_t count(const std::string& text, const std::string& what) {
    size_t n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++n;
    return n;
}

std::string hex(uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

}  // namespace

TEST_CASE("Recording a replay does not change the run: same state hash every tick", "[strata][replay][determinism]") {
    const Config c = smallChamber();
    MicroEnvironment recorded(c), plain(c);
    ReplayOptions o;
    o.every = 3;
    o.fieldColumns = 16;
    ReplayRecorder rec(recorded, o);
    rec.observe(recorded);
    REQUIRE(recorded.stateHash() == plain.stateHash());
    for (int t = 0; t < 200; ++t) {
        recorded.step();
        rec.observe(recorded);
        plain.step();
        REQUIRE(recorded.stateHash() == plain.stateHash());
    }
    rec.finish(recorded);
    REQUIRE(recorded.stateHash() == plain.stateHash());
    CHECK(rec.samples() == 68);  // ticks 0, 3, ..., 198 and the last one, 200
}

TEST_CASE("A replay carries the contract's metadata, sets, field, grains and the state hash per sample",
          "[strata][replay]") {
    const Config c = smallChamber();
    MicroEnvironment env(c);
    ReplayOptions o;
    o.every = 10;
    o.fieldColumns = 16;
    o.simVersion = "9.9.9+test";
    ReplayRecorder rec(env, o);
    rec.observe(env);
    for (int t = 0; t < 50; ++t) {
        env.step();
        rec.observe(env);
    }
    rec.finish(env);  // 50 was already recorded: no second sample
    CHECK(rec.samples() == 6);
    CHECK(rec.fieldColumns() == 16);
    CHECK(rec.fieldRows() == 16);
    const std::string usda = rec.usda();
    CHECK(usda.rfind("#usda 1.0", 0) == 0);
    CHECK(usda.find("string schema = \"usd-replay/1\"") != std::string::npos);
    CHECK(usda.find("string sim = \"microbiome\"") != std::string::npos);
    CHECK(usda.find("string simVersion = \"9.9.9+test\"") != std::string::npos);
    CHECK(usda.find("string seed = \"7\"") != std::string::npos);
    CHECK(usda.find("def Points \"aerobes\"") != std::string::npos);
    CHECK(usda.find("def Points \"fermenters\"") != std::string::npos);
    CHECK(usda.find("def Points \"cross_feeders\"") != std::string::npos);
    CHECK(usda.find("custom int2 replay:field:size = (16, 16)") != std::string::npos);
    CHECK(usda.find("def Points \"grains\"") != std::string::npos);
    CHECK(usda.find("startTimeCode = 0") != std::string::npos);
    CHECK(usda.find("endTimeCode = 50") != std::string::npos);
    // The final state hash is the simulation's own, at tick 50.
    CHECK(usda.find("50: \"" + hex(env.stateHash()) + "\"") != std::string::npos);
    // One ids sample per set per sample.
    CHECK(count(usda, "int64[] ids.timeSamples") == 3);
    CHECK(count(usda, "\n                50: [") >= 3);
}

TEST_CASE("A replay is written to disk and its path must be writable", "[strata][replay]") {
    MicroEnvironment env(smallChamber());
    ReplayOptions o;
    o.fieldColumns = 8;
    ReplayRecorder rec(env, o);
    rec.observe(env);
    const std::string path = "mb_strata_test_replay.usda";
    rec.write(path);
    std::FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE(f != nullptr);
    char head[10] = {};
    CHECK(std::fread(head, 1, 9, f) == 9);
    std::fclose(f);
    std::remove(path.c_str());
    CHECK(std::string(head) == "#usda 1.0");
    CHECK_THROWS(rec.write("no/such/directory/replay.usda"));
}
