// Usage reporting: tells trace (https://trace.danielstephenson.dev) that
// microbiome was started, and nothing else.
//
// One event, `startup`, tagged with the version; it carries the program name
// and nothing about the person running it, their machine or the simulation. It is
// sent through the vendored single-header client (trace_client.hpp) on a
// thread of its own, so it never delays startup and never stops the program:
// a machine that is offline, or has no `curl`, simply sends nothing.
//
// Reporting is on by default and turned off by any one of:
//   - TRACE_USAGE_REPORTING=off or DO_NOT_TRACK=1 in the environment (every
//     trace-reporting program honours these; checked first);
//   - enabled=false in the settings file this writes on first run, in the
//     user's config directory (see settingsPath()).
// The first run prints one notice saying so, on stderr; the settings file it
// leaves behind is what keeps the notice from repeating. When the environment
// has already opted out, nothing is printed and no file is written.
// Details: https://github.com/Stephenson-Software/trace#usage-reporting
#ifndef MICROBIOME_USAGE_REPORTING_H
#define MICROBIOME_USAGE_REPORTING_H

#include "trace_client.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace usage_reporting {

// The name trace issued the key for. It is the `application` on every event
// and must not change without a new key.
static const char *const PROGRAM_NAME = "microbiome";
static const char *const DEFAULT_ENDPOINT = "https://trace.danielstephenson.dev";
// A program key identifies the program to trace and lets the operator revoke
// it; it is scoped to reporting only and is not a secret (it ships in every
// copy of the program).
static const char *const KEY = "sU5Evd_4GTujpQ5NVUW_CSxG80lJepzDjymOg69OhBc";
static const char *const DETAILS_URL = "https://github.com/Stephenson-Software/trace#usage-reporting";
// Points reporting at another server, e.g. a local stub while testing.
static const char *const ENV_ENDPOINT = "MICROBIOME_USAGE_REPORTING_ENDPOINT";
static const char *const SETTINGS_FILENAME = "usage-reporting.conf";

inline std::string environment(const char *name) {
    const char *value = std::getenv(name);
    return value == NULL ? std::string() : std::string(value);
}

/**
 * Where the settings file lives: %APPDATA%\microbiome on Windows,
 * ~/Library/Application Support/microbiome on macOS, and
 * $XDG_CONFIG_HOME/microbiome (default ~/.config/microbiome) elsewhere.
 * Empty when there is no home to put it in.
 */
inline std::string configDirectory() {
#if defined(_WIN32)
    std::string base = environment("APPDATA");
    return base.empty() ? std::string() : base + "\\" + PROGRAM_NAME;
#elif defined(__APPLE__)
    std::string home = environment("HOME");
    return home.empty() ? std::string() : home + "/Library/Application Support/" + PROGRAM_NAME;
#else
    std::string base = environment("XDG_CONFIG_HOME");
    if (base.empty()) {
        std::string home = environment("HOME");
        if (home.empty()) return std::string();
        base = home + "/.config";
    }
    return base + "/" + PROGRAM_NAME;
#endif
}

inline std::string settingsPath() {
    std::string directory = configDirectory();
    if (directory.empty()) return std::string();
#if defined(_WIN32)
    return directory + "\\" + SETTINGS_FILENAME;
#else
    return directory + "/" + SETTINGS_FILENAME;
#endif
}

inline bool makeDirectories(const std::string &path) {
    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            std::string prefix = path.substr(0, i);
#if defined(_WIN32)
            if (prefix.size() == 2 && prefix[1] == ':') continue; // a drive, "C:"
            _mkdir(prefix.c_str());
#else
            mkdir(prefix.c_str(), 0755);
#endif
        }
    }
    std::ofstream probe((path + "/.probe").c_str());
    bool writable = probe.good();
    probe.close();
    std::remove((path + "/.probe").c_str());
    return writable;
}

inline std::string trim(const std::string &text) {
    std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

inline std::string lowered(std::string text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] >= 'A' && text[i] <= 'Z') text[i] = static_cast<char>(text[i] - 'A' + 'a');
    }
    return text;
}

/** What the settings file says. Anything unreadable leaves reporting on. */
struct Settings {
    bool exists;
    bool enabled;
    std::string endpoint;
    Settings() : exists(false), enabled(true) {}
};

inline Settings readSettings(const std::string &path) {
    Settings settings;
    if (path.empty()) return settings;
    std::ifstream in(path.c_str());
    if (!in) return settings;
    settings.exists = true;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::string name = lowered(trim(line.substr(0, equals)));
        std::string value = trim(line.substr(equals + 1));
        if (name == "enabled") {
            std::string v = lowered(value);
            settings.enabled = !(v == "false" || v == "off" || v == "no" || v == "0");
        } else if (name == "endpoint") {
            settings.endpoint = value;
        }
    }
    return settings;
}

inline std::string settingsFileContent() {
    return std::string("# ") + PROGRAM_NAME + " usage reporting - "
        + DETAILS_URL + "\n"
        "#\n"
        "# " + PROGRAM_NAME + " sends one startup event (its name and version) to\n"
        "# https://trace.danielstephenson.dev - nothing about you, your machine or\n"
        "# the simulation. Set enabled to false to turn it off. TRACE_USAGE_REPORTING=off\n"
        "# or DO_NOT_TRACK=1 in the environment turns it off for every\n"
        "# trace-reporting program. This file also records that the notice was shown.\n"
        "enabled=true\n";
}

inline std::string notice(const std::string &path) {
    std::string where = path.empty() ? std::string("the settings file") : path;
    return std::string("Usage reporting is on: ") + PROGRAM_NAME
        + " sends a startup event (its name and version) to https://trace.danielstephenson.dev"
          " - nothing about you, your machine or the simulation. Turn it off with enabled=false in "
        + where + ", or for every trace-reporting program with the environment variable"
          " TRACE_USAGE_REPORTING=off. Details: " + DETAILS_URL;
}

/** The version the build was given, else version.txt in the working directory. */
inline std::string version() {
#if defined(MICROBIOME_VERSION)
    return MICROBIOME_VERSION;
#else
    std::ifstream in("version.txt");
    std::string line;
    if (in && std::getline(in, line)) return trim(line);
    return std::string();
#endif
}

/**
 * Decides whether to report, prints the first-run notice, and sends
 * `startup`. Nothing here throws or blocks for long: the destructor (or
 * close()) gives the event up to trace_client::TIMEOUT_SECONDS to leave.
 */
class UsageReporter {
public:
    explicit UsageReporter(std::ostream &notices = std::cerr) : client_(NULL) {
        try {
            bool enabled = true;
            std::string endpoint = DEFAULT_ENDPOINT;
            if (!trace_client::environmentOptsOut()) {
                std::string path = settingsPath();
                Settings settings = readSettings(path);
                if (!settings.exists) {
                    if (!path.empty() && makeDirectories(configDirectory())) {
                        std::ofstream out(path.c_str());
                        out << settingsFileContent();
                    }
                    notices << notice(path) << std::endl;
                }
                enabled = settings.enabled;
                if (!settings.endpoint.empty()) endpoint = settings.endpoint;
            }
            std::string fromEnvironment = trim(environment(ENV_ENDPOINT));
            if (!fromEnvironment.empty()) endpoint = fromEnvironment;
            // Always built through the client, even when off: it checks the
            // environment first and records why it is off.
            client_ = new trace_client::TraceClient(endpoint, PROGRAM_NAME, KEY, enabled);
        } catch (...) {
            client_ = NULL;
        }
    }

    ~UsageReporter() {
        close();
        delete client_;
    }

    bool isEnabled() const { return client_ != NULL && client_->isEnabled(); }

    std::string disabledReason() const {
        return client_ == NULL ? std::string(trace_client::REASON_UNAVAILABLE) : client_->disabledReason();
    }

    void reportStartup() {
        if (client_ == NULL) return;
        trace_client::Tags tags;
        std::string v = version();
        if (!v.empty()) tags["version"] = v;
        client_->report("startup", tags);
    }

    void close() {
        if (client_ != NULL) client_->close();
    }

private:
    UsageReporter(const UsageReporter &);
    UsageReporter &operator=(const UsageReporter &);
    trace_client::TraceClient *client_;
};

} // namespace usage_reporting

#endif
