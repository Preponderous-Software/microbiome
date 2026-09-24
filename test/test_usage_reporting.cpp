// Usage reporting (src/header/usageReporting.h): the first-run notice, the
// settings file and the opt-outs. Every test points HOME/XDG_CONFIG_HOME at a
// fresh temporary directory and the endpoint at a closed loopback port, so
// nothing here touches the real config directory or the real trace server.
#include "lib/catch2.hpp"

#include "../src/header/usageReporting.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

struct Sandbox {
    std::string home;
    Sandbox() {
        char pattern[] = "/tmp/microbiome-usage-XXXXXX";
        home = mkdtemp(pattern);
        setenv("HOME", home.c_str(), 1);
        setenv("XDG_CONFIG_HOME", (home + "/.config").c_str(), 1);
        unsetenv("TRACE_USAGE_REPORTING");
        unsetenv("DO_NOT_TRACK");
        setenv(usage_reporting::ENV_ENDPOINT, "http://127.0.0.1:9", 1);
    }
    ~Sandbox() {
        std::string command = "rm -rf '" + home + "'";
        if (std::system(command.c_str()) != 0) {
        }
        unsetenv("TRACE_USAGE_REPORTING");
    }
    std::string settings() const { return home + "/.config/microbiome/usage-reporting.conf"; }
};

bool exists(const std::string &path) {
    std::ifstream in(path.c_str());
    return in.good();
}

}

TEST_CASE("First run prints the notice once and leaves a settings file", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream first, second;
    {
        usage_reporting::UsageReporter reporter(first);
        REQUIRE(reporter.isEnabled());
    }
    REQUIRE(first.str().find("Usage reporting is on: microbiome") != std::string::npos);
    REQUIRE(first.str().find("TRACE_USAGE_REPORTING=off") != std::string::npos);
    REQUIRE(first.str().find("https://github.com/Stephenson-Software/trace#usage-reporting") != std::string::npos);
    REQUIRE(first.str().find(sandbox.settings()) != std::string::npos);
    REQUIRE(exists(sandbox.settings()));
    {
        usage_reporting::UsageReporter reporter(second);
        REQUIRE(reporter.isEnabled());
    }
    REQUIRE(second.str().empty());
}

TEST_CASE("enabled=false in the settings file turns reporting off", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream quiet;
    { usage_reporting::UsageReporter reporter(quiet); }
    std::ofstream(sandbox.settings().c_str()) << "# edited\nenabled = false\n";
    std::ostringstream out;
    usage_reporting::UsageReporter reporter(out);
    REQUIRE_FALSE(reporter.isEnabled());
    REQUIRE(reporter.disabledReason() == "config");
    REQUIRE(out.str().empty());
}

TEST_CASE("The environment opt-out wins, prints nothing and writes nothing", "[usage-reporting]") {
    Sandbox sandbox;
    setenv("TRACE_USAGE_REPORTING", "off", 1);
    std::ostringstream out;
    usage_reporting::UsageReporter reporter(out);
    reporter.reportStartup();
    REQUIRE_FALSE(reporter.isEnabled());
    REQUIRE(reporter.disabledReason() == "environment");
    REQUIRE(out.str().empty());
    REQUIRE_FALSE(exists(sandbox.settings()));
}

TEST_CASE("DO_NOT_TRACK=1 is honoured too", "[usage-reporting]") {
    Sandbox sandbox;
    setenv("DO_NOT_TRACK", "1", 1);
    std::ostringstream out;
    usage_reporting::UsageReporter reporter(out);
    REQUIRE(reporter.disabledReason() == "environment");
    unsetenv("DO_NOT_TRACK");
}

TEST_CASE("The settings file parser reads enabled and endpoint", "[usage-reporting]") {
    Sandbox sandbox;
    std::string path = sandbox.home + "/sample.conf";
    std::ofstream(path.c_str()) << "# comment\n  enabled=NO \nendpoint = http://127.0.0.1:1\n";
    usage_reporting::Settings settings = usage_reporting::readSettings(path);
    REQUIRE(settings.exists);
    REQUIRE_FALSE(settings.enabled);
    REQUIRE(settings.endpoint == "http://127.0.0.1:1");
    REQUIRE(usage_reporting::readSettings(sandbox.home + "/missing.conf").enabled);
}

TEST_CASE("The key is the one issued for microbiome", "[usage-reporting]") {
    REQUIRE(std::string(usage_reporting::PROGRAM_NAME) == "microbiome");
    REQUIRE(std::string(usage_reporting::KEY).size() > 20);
}
