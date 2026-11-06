// Usage reporting (src/header/usageReporting.h): the first-run notice, the
// settings file, the installation ID and the opt-outs. Every test points
// HOME/XDG_CONFIG_HOME/XDG_DATA_HOME at a
// fresh temporary directory and the endpoint at a closed loopback port or a
// StubTrace on loopback, so nothing here touches the real config directory or
// the real trace server.
#include "lib/catch2.hpp"

#include "../src/header/usageReporting.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <atomic>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct Sandbox {
    std::string home;
    Sandbox() {
        char pattern[] = "/tmp/microbiome-usage-XXXXXX";
        home = mkdtemp(pattern);
        setenv("HOME", home.c_str(), 1);
        setenv("XDG_CONFIG_HOME", (home + "/.config").c_str(), 1);
        setenv("XDG_DATA_HOME", (home + "/.local/share").c_str(), 1);
        unsetenv("TRACE_INSTALL_ID");
        unsetenv("TRACE_USAGE_REPORTING");
        unsetenv("DO_NOT_TRACK");
        setenv(usage_reporting::ENV_ENDPOINT, "http://127.0.0.1:9", 1);
    }
    ~Sandbox() {
        std::string command = "rm -rf '" + home + "'";
        if (std::system(command.c_str()) != 0) {
        }
        unsetenv("TRACE_USAGE_REPORTING");
        unsetenv("TRACE_INSTALL_ID");
    }
    std::string settings() const { return home + "/.config/microbiome/usage-reporting.conf"; }
    std::string installIdFile() const { return home + "/.local/share/microbiome/trace-install-id"; }
};

bool exists(const std::string &path) {
    std::ifstream in(path.c_str());
    return in.good();
}

std::string firstLine(const std::string &path) {
    std::ifstream in(path.c_str());
    std::string line;
    std::getline(in, line);
    return usage_reporting::trim(line);
}

std::string contents(const std::string &path) {
    std::ifstream in(path.c_str());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

/**
 * Stands in for the trace server on a loopback port: it answers every request
 * with 201 and keeps what it was sent. A reporter delivers through the system
 * curl, so a test that needs one to arrive starts with curlAvailable().
 */
class StubTrace {
public:
    StubTrace() {
        // curl would otherwise send a loopback request to a configured proxy.
        setenv("no_proxy", "127.0.0.1", 1);
        setenv("NO_PROXY", "127.0.0.1", 1);
        listener_ = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0; // any free port, so parallel runs cannot collide
        // Unchecked, a stub that is not listening would make "nothing arrived" prove nothing.
        REQUIRE(bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof address) == 0);
        REQUIRE(listen(listener_, 8) == 0);
        socklen_t length = sizeof address;
        getsockname(listener_, reinterpret_cast<sockaddr *>(&address), &length);
        port_ = ntohs(address.sin_port);
        thread_ = std::thread([this] { serve(); });
    }

    ~StubTrace() {
        stop();
        close(listener_);
    }

    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    /** Stops listening and returns every request, so close the reporter first. */
    std::vector<std::string> received() {
        stop();
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

private:
    void stop() {
        stopping_ = true;
        if (thread_.joinable()) thread_.join();
    }

    void serve() {
        while (!stopping_) {
            pollfd waiting{listener_, POLLIN, 0};
            if (poll(&waiting, 1, 20) <= 0) continue;
            int connection = accept(listener_, NULL, NULL);
            if (connection < 0) continue;
            std::string request = readRequest(connection);
            static const char response[] =
                "HTTP/1.1 201 Created\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            if (send(connection, response, sizeof response - 1, MSG_NOSIGNAL) < 0) {
            }
            close(connection);
            std::lock_guard<std::mutex> lock(mutex_);
            requests_.push_back(request);
        }
    }

    static std::string readRequest(int connection) {
        timeval timeout{5, 0}; // a stalled client must not hang the suite
        setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        std::string request;
        char buffer[1024];
        for (;;) {
            std::size_t headersEnd = request.find("\r\n\r\n");
            if (headersEnd != std::string::npos) {
                std::size_t lengthAt = request.find("Content-Length: ");
                std::size_t length = lengthAt == std::string::npos || lengthAt > headersEnd
                    ? 0 : std::stoul(request.substr(lengthAt + 16));
                if (request.size() >= headersEnd + 4 + length) return request;
            }
            ssize_t count = recv(connection, buffer, sizeof buffer, 0);
            if (count <= 0) return request;
            request.append(buffer, static_cast<std::size_t>(count));
        }
    }

    int listener_ = -1;
    int port_ = 0;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::vector<std::string> requests_;
};

bool curlAvailable() {
    return std::system("command -v curl >/dev/null 2>&1") == 0;
}

std::string bodyOf(const std::string &request) {
    std::size_t headersEnd = request.find("\r\n\r\n");
    return headersEnd == std::string::npos ? std::string() : request.substr(headersEnd + 4);
}

/** Reports startup as main() does, and returns once the event has been sent. */
void reportStartup() {
    std::ostringstream quiet;
    usage_reporting::UsageReporter reporter(quiet);
    reporter.reportStartup();
    reporter.close();
}

/** Runs a test in a directory of its own; version() reads the working directory. */
class InDirectory {
public:
    explicit InDirectory(const std::string &directory) {
        REQUIRE(getcwd(previous_, sizeof previous_) != NULL);
        REQUIRE(chdir(directory.c_str()) == 0);
    }
    ~InDirectory() {
        if (chdir(previous_) != 0) {
        }
    }

private:
    char previous_[PATH_MAX];
};

/** Empties PATH for a test, so posix_spawnp cannot find curl. */
class WithoutPath {
public:
    explicit WithoutPath(const std::string &emptyDirectory) : previous_(usage_reporting::environment("PATH")) {
        setenv("PATH", emptyDirectory.c_str(), 1);
    }
    ~WithoutPath() { setenv("PATH", previous_.c_str(), 1); }

private:
    std::string previous_;
};

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
    REQUIRE(first.str().find("https://danielstephenson.dev/usage-reporting") != std::string::npos);
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
    REQUIRE_FALSE(exists(sandbox.installIdFile()));
    REQUIRE(reporter.installId().empty());
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

TEST_CASE("The installation ID is kept in the data directory and reused", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream out;
    std::string first;
    {
        usage_reporting::UsageReporter reporter(out);
        first = reporter.installId();
    }
    REQUIRE(usage_reporting::installIdPath() == sandbox.installIdFile());
    REQUIRE(first.size() == 36);
    REQUIRE(firstLine(sandbox.installIdFile()) == first);
    REQUIRE(out.str().find(sandbox.installIdFile()) != std::string::npos);
    usage_reporting::UsageReporter again(out);
    REQUIRE(again.installId() == first);
}

TEST_CASE("TRACE_INSTALL_ID is sent instead and the file is left alone", "[usage-reporting]") {
    Sandbox sandbox;
    setenv("TRACE_INSTALL_ID", " pinned-id ", 1);
    std::ostringstream out;
    usage_reporting::UsageReporter reporter(out);
    REQUIRE(reporter.installId() == "pinned-id");
    REQUIRE_FALSE(exists(sandbox.installIdFile()));
}

TEST_CASE("No installation ID is made when the settings file turns reporting off", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream quiet;
    { usage_reporting::UsageReporter reporter(quiet); }
    std::remove(sandbox.installIdFile().c_str());
    std::ofstream(sandbox.settings().c_str()) << "enabled=false\n";
    usage_reporting::UsageReporter reporter(quiet);
    REQUIRE(reporter.installId().empty());
    REQUIRE_FALSE(exists(sandbox.installIdFile()));
}

TEST_CASE("The settings and data directories follow XDG, then HOME", "[usage-reporting]") {
    Sandbox sandbox;

    SECTION("the XDG variables win over HOME") {
        setenv("XDG_CONFIG_HOME", "/xdg/config", 1);
        setenv("XDG_DATA_HOME", "/xdg/data", 1);

        REQUIRE(usage_reporting::settingsPath() == "/xdg/config/microbiome/usage-reporting.conf");
        REQUIRE(usage_reporting::installIdPath() == "/xdg/data/microbiome/trace-install-id");
    }

    SECTION("unset XDG variables fall back to the defaults under HOME") {
        unsetenv("XDG_CONFIG_HOME");
        unsetenv("XDG_DATA_HOME");

        REQUIRE(usage_reporting::settingsPath() == sandbox.settings());
        REQUIRE(usage_reporting::installIdPath() == sandbox.installIdFile());
    }

    SECTION("empty XDG variables count as unset") {
        setenv("XDG_CONFIG_HOME", "", 1);
        setenv("XDG_DATA_HOME", "", 1);

        REQUIRE(usage_reporting::settingsPath() == sandbox.settings());
        REQUIRE(usage_reporting::installIdPath() == sandbox.installIdFile());
    }

    SECTION("with no HOME either there is nowhere to keep a file") {
        unsetenv("XDG_CONFIG_HOME");
        unsetenv("XDG_DATA_HOME");
        unsetenv("HOME");

        REQUIRE(usage_reporting::settingsPath().empty());
        REQUIRE(usage_reporting::installIdPath().empty());
    }
}

// Nothing can record that the notice was shown, so it repeats on every run.
TEST_CASE("With no home reporting stays on, without an installation ID or a file", "[usage-reporting]") {
    Sandbox sandbox;
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("HOME");

    for (int run = 0; run < 2; run++) {
        std::ostringstream out;
        usage_reporting::UsageReporter reporter(out);

        REQUIRE(reporter.isEnabled());
        REQUIRE(reporter.installId().empty());
        REQUIRE(out.str().find("enabled=false in the settings file") != std::string::npos);
        REQUIRE(out.str().find("kept in trace-install-id") != std::string::npos);
    }
}

TEST_CASE("Every spelling of the environment opt-outs turns reporting off", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream out;

    SECTION("TRACE_USAGE_REPORTING") {
        std::string value = GENERATE("off", "OFF", "false", "0", "no", " Off ");
        setenv("TRACE_USAGE_REPORTING", value.c_str(), 1);

        usage_reporting::UsageReporter reporter(out);

        REQUIRE(reporter.disabledReason() == "environment");
    }

    SECTION("DO_NOT_TRACK") {
        std::string value = GENERATE("1", "true", "yes", "TRUE", " yes ");
        setenv("DO_NOT_TRACK", value.c_str(), 1);

        usage_reporting::UsageReporter reporter(out);

        REQUIRE(reporter.disabledReason() == "environment");
    }

    REQUIRE(out.str().empty());
    REQUIRE_FALSE(exists(sandbox.settings()));
    REQUIRE_FALSE(exists(sandbox.installIdFile()));
    unsetenv("DO_NOT_TRACK");
}

TEST_CASE("Any other value of the environment variables leaves reporting on", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream out;

    SECTION("TRACE_USAGE_REPORTING") {
        std::string value = GENERATE("", "on", "1", "true", "disabled");
        setenv("TRACE_USAGE_REPORTING", value.c_str(), 1);

        usage_reporting::UsageReporter reporter(out);

        REQUIRE(reporter.isEnabled());
    }

    SECTION("DO_NOT_TRACK") {
        std::string value = GENERATE("", "0", "false", "no");
        setenv("DO_NOT_TRACK", value.c_str(), 1);

        usage_reporting::UsageReporter reporter(out);

        REQUIRE(reporter.isEnabled());
    }

    unsetenv("DO_NOT_TRACK");
}

TEST_CASE("The settings file parser accepts the enabled spellings it documents", "[usage-reporting]") {
    Sandbox sandbox;
    std::string path = sandbox.home + "/sample.conf";

    SECTION("these turn reporting off") {
        std::string value = GENERATE("false", "off", "no", "0", "FALSE", "Off");
        std::ofstream(path.c_str()) << "enabled=" << value << "\n";

        REQUIRE_FALSE(usage_reporting::readSettings(path).enabled);
    }

    SECTION("anything else leaves it on") {
        std::string value = GENERATE("true", "on", "1", "maybe", "");
        std::ofstream(path.c_str()) << "enabled=" << value << "\n";

        REQUIRE(usage_reporting::readSettings(path).enabled);
    }

    SECTION("comments, unknown names and lines with no = are skipped") {
        std::ofstream(path.c_str()) << "#enabled=false\n\nenabled\nother=false\n";

        usage_reporting::Settings settings = usage_reporting::readSettings(path);

        REQUIRE(settings.exists);
        REQUIRE(settings.enabled);
        REQUIRE(settings.endpoint.empty());
    }

    SECTION("the setting's name is not case sensitive") {
        std::ofstream(path.c_str()) << "ENABLED=false\nEndpoint=http://127.0.0.1:1\n";

        usage_reporting::Settings settings = usage_reporting::readSettings(path);

        REQUIRE_FALSE(settings.enabled);
        REQUIRE(settings.endpoint == "http://127.0.0.1:1");
    }

    SECTION("no path means nothing was read") {
        REQUIRE_FALSE(usage_reporting::readSettings("").exists);
    }
}

TEST_CASE("The settings file a first run writes keeps reporting on and explains itself", "[usage-reporting]") {
    Sandbox sandbox;
    std::ostringstream out;
    { usage_reporting::UsageReporter reporter(out); }

    usage_reporting::Settings settings = usage_reporting::readSettings(sandbox.settings());
    REQUIRE(settings.exists);
    REQUIRE(settings.enabled);
    REQUIRE(settings.endpoint.empty());

    std::string written = contents(sandbox.settings());
    REQUIRE(written == usage_reporting::settingsFileContent());
    REQUIRE(written.find("https://danielstephenson.dev/usage-reporting") != std::string::npos);
    REQUIRE(written.find(sandbox.installIdFile()) != std::string::npos);
    REQUIRE(written.find("TRACE_USAGE_REPORTING=off") != std::string::npos);
    REQUIRE(written.find("DO_NOT_TRACK=1") != std::string::npos);
}

TEST_CASE("A blank TRACE_INSTALL_ID falls back to the installation ID file", "[usage-reporting]") {
    Sandbox sandbox;
    setenv("TRACE_INSTALL_ID", "   ", 1);
    std::ostringstream out;

    usage_reporting::UsageReporter reporter(out);

    REQUIRE(reporter.installId().size() == 36);
    REQUIRE(firstLine(sandbox.installIdFile()) == reporter.installId());
}

// mb_tests is built without MICROBIOME_VERSION, so version() takes its version.txt path here.
TEST_CASE("The reported version comes from version.txt and is never blank", "[usage-reporting]") {
    Sandbox sandbox;
    InDirectory directory(sandbox.home);

    SECTION("the file's first line, trimmed") {
        std::ofstream("version.txt") << " 1.2.3 \nignored\n";
        REQUIRE(usage_reporting::version() == "1.2.3");
    }

    SECTION("no file") {
        REQUIRE(usage_reporting::version() == "unknown");
    }

    SECTION("a blank file, which would otherwise turn the client off") {
        std::ofstream("version.txt") << "   \n";
        REQUIRE(usage_reporting::version() == "unknown");
    }
}

// The README's promise, checked on the wire: one event, carrying the program name, its version
// and the installation ID, and nothing else.
TEST_CASE("Startup sends one event holding only the name, the version and the ID", "[usage-reporting]") {
    if (!curlAvailable()) {
        WARN("curl is not installed, so nothing can be delivered");
        return;
    }
    Sandbox sandbox;
    StubTrace trace;
    setenv(usage_reporting::ENV_ENDPOINT, trace.url().c_str(), 1);
    setenv("TRACE_INSTALL_ID", "pinned-id", 1);

    reportStartup();

    std::vector<std::string> requests = trace.received();
    REQUIRE(requests.size() == 1);
    REQUIRE(requests[0].rfind("POST /api/metrics HTTP/1.1\r\n", 0) == 0);
    REQUIRE(requests[0].find(std::string("Authorization: Bearer ") + usage_reporting::KEY + "\r\n") != std::string::npos);
    REQUIRE(bodyOf(requests[0]) ==
            "{\"application\":\"microbiome\",\"name\":\"startup\",\"tags\":"
            "{\"install\":\"pinned-id\",\"version\":\"" + usage_reporting::version() + "\"}}");
}

TEST_CASE("The endpoint comes from the environment, then the settings file", "[usage-reporting]") {
    if (!curlAvailable()) {
        WARN("curl is not installed, so nothing can be delivered");
        return;
    }
    Sandbox sandbox;
    StubTrace trace;
    std::ostringstream quiet;
    { usage_reporting::UsageReporter reporter(quiet); } // writes the settings file

    SECTION("the settings file's endpoint is used") {
        std::ofstream(sandbox.settings().c_str()) << "endpoint=" << trace.url() << "\n";
        unsetenv(usage_reporting::ENV_ENDPOINT);
    }

    SECTION("the environment wins over the settings file") {
        std::ofstream(sandbox.settings().c_str()) << "endpoint=http://127.0.0.1:9\n";
        setenv(usage_reporting::ENV_ENDPOINT, trace.url().c_str(), 1);
    }

    SECTION("a blank environment variable counts as unset") {
        std::ofstream(sandbox.settings().c_str()) << "endpoint=" << trace.url() << "\n";
        setenv(usage_reporting::ENV_ENDPOINT, "   ", 1);
    }

    reportStartup();

    REQUIRE(trace.received().size() == 1);
}

// The stub is listening and the endpoint points at it, so only the opt-out can explain silence.
TEST_CASE("Nothing is sent when reporting is off or no event was reported", "[usage-reporting]") {
    Sandbox sandbox;
    StubTrace trace;
    setenv(usage_reporting::ENV_ENDPOINT, trace.url().c_str(), 1);

    SECTION("TRACE_USAGE_REPORTING=off") {
        setenv("TRACE_USAGE_REPORTING", "off", 1);
        reportStartup();
    }

    SECTION("DO_NOT_TRACK=1") {
        setenv("DO_NOT_TRACK", "1", 1);
        reportStartup();
        unsetenv("DO_NOT_TRACK");
    }

    SECTION("enabled=false in the settings file") {
        std::ostringstream quiet;
        { usage_reporting::UsageReporter reporter(quiet); }
        std::ofstream(sandbox.settings().c_str()) << "enabled=false\n";
        reportStartup();
    }

    SECTION("constructing a reporter sends nothing by itself") {
        std::ostringstream quiet;
        usage_reporting::UsageReporter reporter(quiet);
        reporter.close();
    }

    REQUIRE(trace.received().empty());
}

TEST_CASE("A machine without curl sends nothing and the program carries on", "[usage-reporting]") {
    Sandbox sandbox;
    StubTrace trace;
    setenv(usage_reporting::ENV_ENDPOINT, trace.url().c_str(), 1);
    {
        WithoutPath withoutPath(sandbox.home);
        std::ostringstream quiet;
        usage_reporting::UsageReporter reporter(quiet);
        REQUIRE(reporter.isEnabled());

        reporter.reportStartup();
        reporter.close();

        REQUIRE_FALSE(reporter.isEnabled());
    }

    REQUIRE(trace.received().empty());
}
