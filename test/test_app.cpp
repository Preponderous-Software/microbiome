// Tests for the console app (mb_app): MicrobiomeApplication's run loop and
// printed results, and runConsoleApp's options. Every run uses a small config
// with output off; runConsoleApp is given no onStarted, so nothing is reported.
#include "lib/catch2.hpp"

#include "../src/header/appConfig.h"
#include "../src/header/mbapp.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

AppConfig smallConfig(int numSimulations) {
    AppConfig config;
    config.setEnvironmentSize(4);
    config.setEntityFactor(2);
    config.setNumSimulations(numSimulations);
    config.setSimulationOutputEnabled(false);
    return config;
}

// Runs runConsoleApp with these arguments after the program name.
int runWith(std::vector<std::string> arguments, std::ostream& out, std::ostream& err, AppConfig config) {
    arguments.insert(arguments.begin(), "mb_app");
    std::vector<char*> argv;
    for (std::string& argument : arguments) {
        argv.push_back(&argument[0]);
    }
    argv.push_back(nullptr);
    return runConsoleApp(static_cast<int>(arguments.size()), argv.data(), out, err, config);
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

std::string temporaryPath() {
    char pattern[] = "/tmp/microbiome-app-XXXXXX";
    int fd = mkstemp(pattern);
    REQUIRE(fd != -1);
    close(fd);
    std::remove(pattern);
    return std::string(pattern) + ".usda";
}

}

TEST_CASE("One simulation prints its own result", "[app]") {
    std::ostringstream out;
    MicrobiomeApplication app(smallConfig(1), out);

    REQUIRE(app.run());

    REQUIRE(app.getResults().size() == 1);
    Result result = app.getResults()[0];
    std::ostringstream expected;
    expected << "Running simulation 1 of 1\n"
             << "=== Simulation Result ===\n"
             << "Surviving Microorganisms: " << result.getSurvivingMicroorganisms() << "\n"
             << "Dead Microorganisms: " << result.getDeadMicroorganisms() << "\n"
             << "Total Energy: " << result.getEnergy() << "\n"
             << "Ticks elapsed: " << result.getTicksElapsed() << "\n";
    REQUIRE(out.str() == expected.str());
}

TEST_CASE("Several simulations print their averages, rounded down", "[app]") {
    std::ostringstream out;
    MicrobiomeApplication app(smallConfig(3), out);

    REQUIRE(app.run());

    std::vector<Result> results = app.getResults();
    REQUIRE(results.size() == 3);
    size_t ticks = 0, dead = 0, surviving = 0, energy = 0;
    for (Result& result : results) {
        ticks += result.getTicksElapsed();
        dead += result.getDeadMicroorganisms();
        surviving += result.getSurvivingMicroorganisms();
        energy += result.getEnergy();
    }
    std::ostringstream expected;
    expected << "Running simulation 1 of 3\n"
             << "Running simulation 2 of 3\n"
             << "Running simulation 3 of 3\n"
             << "=== Average Results of 3 Simulations ===\n"
             << "Ticks elapsed: " << ticks / 3 << "\n"
             << "Dead Microorganisms: " << dead / 3 << "\n"
             << "Surviving Microorganisms: " << surviving / 3 << "\n"
             << "Total Energy: " << energy / 3 << "\n";
    REQUIRE(out.str() == expected.str());
    REQUIRE_FALSE(contains(out.str(), "=== Simulation Result ==="));
}

TEST_CASE("No simulations print that there is nothing to report", "[app]") {
    std::ostringstream out;
    MicrobiomeApplication app(smallConfig(0), out);

    REQUIRE(app.run());

    REQUIRE(app.getResults().empty());
    REQUIRE(out.str() == "No results to print.\n");
}

TEST_CASE("--record writes one file holding only the first simulation", "[app][usd]") {
    std::string path = temporaryPath();
    std::ostringstream out;
    MicrobiomeApplication app(smallConfig(3), out);
    app.setRecordPath(path);

    REQUIRE(app.run());

    std::ifstream file(path);
    REQUIRE(file.good());
    std::stringstream usda;
    usda << file.rdbuf();
    std::remove(path.c_str());

    size_t firstTicks = app.getResults()[0].getTicksElapsed();
    REQUIRE(contains(usda.str(), "endTimeCode = " + std::to_string(firstTicks) + "\n"));
    REQUIRE(countOccurrences(out.str(), "Recorded ") == 1);
    REQUIRE(contains(out.str(), "Running simulation 1 of 3\nRecorded " + std::to_string(firstTicks + 1) +
                                " ticks of simulation 1 to " + path + "\nRunning simulation 2 of 3\n"));
}

TEST_CASE("runConsoleApp runs the configured simulations and prints the seed", "[app]") {
    std::ostringstream out, err;

    REQUIRE(runWith({}, out, err, smallConfig(2)) == 0);

    REQUIRE(err.str().empty());
    REQUIRE(out.str().rfind("Seed: ", 0) == 0);
    REQUIRE(contains(out.str(), "(repeat this run with --seed "));
    REQUIRE(contains(out.str(), "=== Average Results of 2 Simulations ==="));
}

TEST_CASE("The same --seed runs the same simulations", "[app]") {
    std::ostringstream first, second, err;

    REQUIRE(runWith({"--seed", "12345"}, first, err, smallConfig(3)) == 0);
    REQUIRE(runWith({"--seed", "12345"}, second, err, smallConfig(3)) == 0);

    REQUIRE(err.str().empty());
    REQUIRE(first.str().rfind("Seed: 12345 (repeat this run with --seed 12345)\n", 0) == 0);
    REQUIRE(first.str() == second.str());
}

TEST_CASE("--record and --seed can be given together", "[app][usd]") {
    std::string path = temporaryPath();
    std::ostringstream out, err;

    REQUIRE(runWith({"--seed", "7", "--record", path}, out, err, smallConfig(2)) == 0);

    std::ifstream file(path);
    REQUIRE(file.good());
    std::remove(path.c_str());
    REQUIRE(contains(out.str(), "Seed: 7 "));
    REQUIRE(countOccurrences(out.str(), "ticks of simulation 1 to " + path) == 1);
}

TEST_CASE("Any other arguments print the usage and exit with status 2", "[app]") {
    std::vector<std::vector<std::string>> invalid = {
        {"--bogus"},
        {"--record"},
        {"--seed"},
        {"--seed", "abc"},
        {"--seed", ""},
        {"--seed", "-1"},
        {"--seed", "+1"},
        {"--seed", " 1"},
        {"--seed", "12x"},
        {"--seed", "4294967296"},
        {"--seed", "99999999999999999999999"},
        {"--seed", "1", "extra"},
    };
    for (const std::vector<std::string>& arguments : invalid) {
        std::ostringstream out, err;
        INFO("arguments: " << arguments.size() << " starting with " << arguments[0]);
        REQUIRE(runWith(arguments, out, err, smallConfig(1)) == 2);
        REQUIRE(err.str() == "usage: mb_app [--record <path.usda>] [--seed <N>]\n");
        REQUIRE(out.str().empty());
    }
}

TEST_CASE("The largest seed is accepted", "[app]") {
    std::ostringstream out, err;

    REQUIRE(runWith({"--seed", "4294967295"}, out, err, smallConfig(1)) == 0);

    REQUIRE(contains(out.str(), "Seed: 4294967295 "));
}

TEST_CASE("onStarted runs only once the arguments are accepted", "[app]") {
    std::vector<std::string> good = {"mb_app", "--seed", "1"};
    std::vector<std::string> bad = {"mb_app", "--bogus"};
    std::vector<char*> goodArgv, badArgv;
    for (std::string& argument : good) goodArgv.push_back(&argument[0]);
    for (std::string& argument : bad) badArgv.push_back(&argument[0]);
    std::ostringstream out, err;
    int calls = 0;

    REQUIRE(runConsoleApp(2, badArgv.data(), out, err, smallConfig(1), [&calls]() { calls++; }) == 2);
    REQUIRE(calls == 0);
    REQUIRE(runConsoleApp(3, goodArgv.data(), out, err, smallConfig(1), [&calls]() { calls++; }) == 0);
    REQUIRE(calls == 1);
}
