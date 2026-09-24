/*
 * trace-client 0.1.0 (C++) -- https://github.com/Stephenson-Software/trace-client-cpp
 *
 * One call to report that a program was used. Copy this header into a project
 * as is; there is nothing else to add. C++11 or later, no library to link
 * beyond the threads library (-pthread), as for any std::thread.
 *
 * MIT licensed. Keep this header when vendoring so the file can be found again.
 */
#ifndef TRACE_CLIENT_HPP
#define TRACE_CLIENT_HPP

#define TRACE_CLIENT_VERSION "0.1.0"

// The executable that carries a report over HTTPS: the system's own curl
// (shipped with macOS, with Windows 10 1803 and later, and with nearly every
// Linux distribution). Override before including to point somewhere else.
// Define TRACE_CLIENT_USE_LIBCURL instead to link against libcurl.
#ifndef TRACE_CLIENT_CURL
#define TRACE_CLIENT_CURL "curl"
#endif

#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#if defined(TRACE_CLIENT_USE_LIBCURL)
#include <curl/curl.h>
#elif defined(__EMSCRIPTEN__)
// No processes and no threads worth the name in a browser: the client is
// always disabled there (reason "unavailable").
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define TRACE_CLIENT_UNDEF_LEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#define TRACE_CLIENT_UNDEF_NOMINMAX
#endif
#include <windows.h>
#ifdef TRACE_CLIENT_UNDEF_LEAN
#undef WIN32_LEAN_AND_MEAN
#undef TRACE_CLIENT_UNDEF_LEAN
#endif
#ifdef TRACE_CLIENT_UNDEF_NOMINMAX
#undef NOMINMAX
#undef TRACE_CLIENT_UNDEF_NOMINMAX
#endif
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char **environ;
#endif
#define TRACE_CLIENT_POSIX 1
#endif

namespace trace_client {

/** String tags on a report, such as {{"version", "1.4.0"}}. */
typedef std::map<std::string, std::string> Tags;

/**
 * Where dropped reports are mentioned. Called on the client's own thread for
 * failed deliveries and on the reporting thread for a full queue.
 */
typedef std::function<void(const std::string &)> Logger;

/** How many reports may wait to be sent before new ones are dropped. */
static const std::size_t QUEUE_CAPACITY = 256;
/** Per-request timeout, and the most close() waits for queued reports. */
static const double TIMEOUT_SECONDS = 5.0;
/** The server's limits: tags per report, and length of a name or tag key/value. */
static const std::size_t MAX_TAGS = 32;
static const std::size_t MAX_LENGTH = 255;

/** Environment variables that turn reporting off for every trace-reporting program. */
static const char *const ENV_TRACE_USAGE_REPORTING = "TRACE_USAGE_REPORTING";
static const char *const ENV_DO_NOT_TRACK = "DO_NOT_TRACK";

/** The values disabledReason() can take. First match wins, in this order. */
static const char *const REASON_ENVIRONMENT = "environment";
static const char *const REASON_CONFIG = "config";
static const char *const REASON_NO_KEY = "no key";
/** No way to send on this platform, or the client could not start its thread. */
static const char *const REASON_UNAVAILABLE = "unavailable";

namespace detail {

inline std::string getEnvironment(const char *name) {
#if defined(_MSC_VER)
    char *value = NULL;
    std::size_t length = 0;
    std::string result;
    if (_dupenv_s(&value, &length, name) == 0 && value != NULL) {
        result = value;
    }
    std::free(value);
    return result;
#else
    const char *value = std::getenv(name);
    return value == NULL ? std::string() : std::string(value);
#endif
}

inline std::string trimLower(const std::string &text) {
    std::size_t begin = 0, end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    std::string out = text.substr(begin, end - begin);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

inline bool isBlank(const std::string &text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!std::isspace(static_cast<unsigned char>(text[i]))) return false;
    }
    return true;
}

/**
 * Valid UTF-8, at most maxBytes long: invalid sequences become U+FFFD and a
 * code point is never cut in half. The server stores what it is sent, and a
 * string it cannot decode would cost the whole report.
 */
inline std::string cleanUtf8(const std::string &text, std::size_t maxBytes) {
    static const char replacement[] = "\xEF\xBF\xBD";
    std::string out;
    out.reserve(text.size() < maxBytes ? text.size() : maxBytes);
    std::size_t i = 0;
    const std::size_t n = text.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t length = 0;
        unsigned long cp = 0;
        if (c < 0x80) { length = 1; cp = c; }
        else if (c >= 0xC2 && c <= 0xDF) { length = 2; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { length = 3; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { length = 4; cp = c & 0x07; }
        bool valid = length > 0 && i + length <= n;
        for (std::size_t k = 1; valid && k < length; ++k) {
            unsigned char next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0) != 0x80) valid = false;
            else cp = (cp << 6) | (next & 0x3F);
        }
        if (valid && ((length == 3 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)))
                      || (length == 4 && (cp < 0x10000 || cp > 0x10FFFF)))) {
            valid = false;
        }
        if (valid) {
            if (out.size() + length > maxBytes) break;
            out.append(text, i, length);
            i += length;
        } else {
            if (out.size() + 3 > maxBytes) break;
            out.append(replacement, 3);
            i += 1;
        }
    }
    return out;
}

/** A JSON string literal. Input is expected to be valid UTF-8 already. */
inline std::string quote(const std::string &text) {
    std::string out;
    out.reserve(text.size() + 2);
    out += '"';
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

/** The shortest decimal that reads back as value, whatever the C locale. */
inline std::string number(double value) {
    std::string text;
    for (int precision = 1; precision <= 17; ++precision) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(precision);
        out << value;
        text = out.str();
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        double back = 0;
        in >> back;
        if (back == value) break;
    }
    return text;
}

/**
 * The report body. JSON is written by hand so this file has no dependencies;
 * the shape is fixed and small, three scalars and a flat string map.
 */
inline std::string json(const std::string &application, const std::string &name,
                        bool hasValue, double value, const Tags &tags) {
    std::string out = "{\"application\":" + quote(cleanUtf8(application, std::string::npos));
    out += ",\"name\":" + quote(cleanUtf8(name, MAX_LENGTH));
    if (hasValue && !std::isnan(value) && !std::isinf(value)) {
        out += ",\"value\":" + number(value);
    }
    std::string pairs;
    std::size_t count = 0;
    for (Tags::const_iterator tag = tags.begin(); tag != tags.end() && count < MAX_TAGS; ++tag) {
        std::string key = cleanUtf8(tag->first, MAX_LENGTH);
        if (isBlank(key)) continue;
        if (count > 0) pairs += ',';
        pairs += quote(key) + ':' + quote(cleanUtf8(tag->second, MAX_LENGTH));
        ++count;
    }
    if (count > 0) out += ",\"tags\":{" + pairs + '}';
    out += '}';
    return out;
}

/** Drops control characters: nothing typed into a header may end the line. */
inline std::string headerSafe(const std::string &text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0x20 && c != 0x7F) out += static_cast<char>(c);
    }
    return out;
}

/**
 * A quoted value in curl's config-file syntax. curl unescapes \\ \" \t \n \r
 * \v inside quotes; every backslash and quote is escaped and every control
 * character is written as an escape, so a value can never end its line and
 * start another option.
 */
inline std::string curlQuote(const std::string &text) {
    std::string out = "\"";
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\v': out += "\\v"; break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) out += c;
        }
    }
    out += '"';
    return out;
}

struct Shared {
    std::string endpoint;
    std::string key;
    std::string application;
    Logger logger;

    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::string> queue;
    bool closing;   // close() was called: send what is queued, then stop
    bool abandoned; // close() gave up waiting: stop now, drop the rest
    bool done;      // the sending thread has stopped
#if defined(TRACE_CLIENT_POSIX)
    pid_t child; // the curl process in flight, or 0
#elif defined(_WIN32) && !defined(TRACE_CLIENT_USE_LIBCURL)
    HANDLE child; // the curl process in flight, or NULL
#endif

    Shared() : closing(false), abandoned(false), done(false)
#if defined(TRACE_CLIENT_POSIX)
        , child(0)
#elif defined(_WIN32) && !defined(TRACE_CLIENT_USE_LIBCURL)
        , child(NULL)
#endif
    {}

    // Guards logger alone, so a logger may itself call report(). close()
    // clears the logger under it: once close() returns, the program's logger
    // is never called again, even by a thread that close() gave up on.
    std::mutex logMutex;

    void log(const std::string &message) {
        try {
            std::lock_guard<std::mutex> lock(logMutex);
            if (logger) logger("[trace] " + message);
        } catch (...) {
            // a logger that throws must not stop the sending thread
        }
    }

    void forgetLogger() {
        try {
            std::lock_guard<std::mutex> lock(logMutex);
            logger = Logger();
        } catch (...) {
        }
    }

    /** Called by close() with the mutex held, once it has given up waiting. */
    void killChild() {
#if defined(TRACE_CLIENT_POSIX)
        if (child > 0) ::kill(child, SIGKILL);
#elif defined(_WIN32) && !defined(TRACE_CLIENT_USE_LIBCURL)
        if (child != NULL) ::TerminateProcess(child, 1);
#endif
    }
};

inline long timeoutWholeSeconds() {
    return static_cast<long>(TIMEOUT_SECONDS);
}

#if !defined(TRACE_CLIENT_USE_LIBCURL) && !defined(__EMSCRIPTEN__)
/** Everything curl is told, on its standard input rather than its command line. */
inline std::string curlConfig(const Shared &shared, const std::string &body) {
    std::ostringstream timeout;
    timeout.imbue(std::locale::classic());
    timeout << timeoutWholeSeconds();
    std::string config;
    config += "url = " + curlQuote(shared.endpoint) + "\n";
    config += "proto = \"=http,https\"\n";
    config += "header = \"Content-Type: application/json; charset=utf-8\"\n";
    config += "header = \"Expect:\"\n"; // no 100-continue round trip for a larger body
    config += "header = " + curlQuote("Authorization: Bearer " + headerSafe(shared.key)) + "\n";
    config += "user-agent = " + curlQuote("trace-client-cpp/" TRACE_CLIENT_VERSION " (" + headerSafe(shared.application) + ")") + "\n";
    config += "data-binary = " + curlQuote(body) + "\n";
    config += "max-time = " + timeout.str() + "\n";
    config += "connect-timeout = " + timeout.str() + "\n";
#if defined(_WIN32)
    config += "output = \"NUL\"\n";
#else
    config += "output = \"/dev/null\"\n";
#endif
    config += "write-out = \"%{http_code}\"\n";
    return config;
}

inline int parseStatus(const std::string &out) {
    int status = 0;
    bool any = false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] >= '0' && out[i] <= '9') {
            status = status * 10 + (out[i] - '0');
            any = true;
            if (status > 999) return -1;
        } else if (!std::isspace(static_cast<unsigned char>(out[i]))) {
            return -1;
        }
    }
    return any && status > 0 ? status : -1;
}
#endif

#if defined(TRACE_CLIENT_USE_LIBCURL)

inline std::size_t discardBody(char *, std::size_t size, std::size_t count, void *) {
    return size * count;
}

#if LIBCURL_VERSION_NUM >= 0x072000
inline int abortWhenAbandoned(void *data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    Shared *shared = static_cast<Shared *>(data);
    std::lock_guard<std::mutex> lock(shared->mutex);
    return shared->abandoned ? 1 : 0;
}
#endif

/** Returns the HTTP status, or -1 with error set. */
inline int send(Shared &shared, const std::string &body, std::string &error) {
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        error = "curl_easy_init failed";
        return -1;
    }
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + headerSafe(shared.key)).c_str());
    std::string agent = "trace-client-cpp/" TRACE_CLIENT_VERSION " (" + headerSafe(shared.application) + ")";
    curl_easy_setopt(curl, CURLOPT_URL, shared.endpoint.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutWholeSeconds());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeoutWholeSeconds());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardBody);
#if LIBCURL_VERSION_NUM >= 0x072000
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortWhenAbandoned);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &shared);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
#endif
    CURLcode result = curl_easy_perform(curl);
    long status = -1;
    if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    } else {
        error = curl_easy_strerror(result);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return static_cast<int>(status);
}

#elif defined(TRACE_CLIENT_POSIX)

inline void closeQuietly(int fd) {
    if (fd >= 0) ::close(fd);
}

inline bool closeOnExec(int fd) {
    int flags = ::fcntl(fd, F_GETFD);
    return flags != -1 && ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != -1;
}

/**
 * Runs `curl -q --silent --config -` with the request on its standard input.
 * The argument vector is fixed -- nothing from the program or the report is
 * ever an argument, and no shell is involved. Returns the HTTP status, or -1
 * with error set (curl missing, unreachable server, timeout).
 */
inline int send(Shared &shared, const std::string &body, std::string &error) {
    int in[2] = {-1, -1};
    int out[2] = {-1, -1};
    if (::pipe(in) != 0 || ::pipe(out) != 0 || !closeOnExec(in[0]) || !closeOnExec(in[1])
        || !closeOnExec(out[0]) || !closeOnExec(out[1])) {
        error = "could not create a pipe";
        closeQuietly(in[0]); closeQuietly(in[1]); closeQuietly(out[0]); closeQuietly(out[1]);
        return -1;
    }
#if defined(F_SETNOSIGPIPE)
    ::fcntl(in[1], F_SETNOSIGPIPE, 1);
#endif
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attributes);
    posix_spawn_file_actions_adddup2(&actions, in[0], 0);
    posix_spawn_file_actions_adddup2(&actions, out[1], 1);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    // curl starts with an ordinary signal mask and SIGPIPE at its default,
    // whatever the sending thread (which blocks SIGPIPE) or the host set.
    sigset_t none, pipe;
    sigemptyset(&none);
    sigemptyset(&pipe);
    sigaddset(&pipe, SIGPIPE);
    posix_spawnattr_setsigmask(&attributes, &none);
    posix_spawnattr_setsigdefault(&attributes, &pipe);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

    char program[] = TRACE_CLIENT_CURL;
    char arg0[] = "curl";
    char arg1[] = "-q"; // first, so no ~/.curlrc is read
    char arg2[] = "--silent";
    char arg3[] = "--config";
    char arg4[] = "-";
    char *argv[] = {arg0, arg1, arg2, arg3, arg4, NULL};
#if defined(__APPLE__)
    char **env = *_NSGetEnviron();
#else
    char **env = environ;
#endif
    pid_t pid = 0;
    int spawned = posix_spawnp(&pid, program, &actions, &attributes, argv, env);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    closeQuietly(in[0]);
    closeQuietly(out[1]);
    if (spawned != 0) {
        closeQuietly(in[1]);
        closeQuietly(out[0]);
        error = std::string("could not run " TRACE_CLIENT_CURL ": ") + std::strerror(spawned);
        return -1;
    }
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.child = pid;
        if (shared.abandoned) shared.killChild();
    }

    std::string config = curlConfig(shared, body);
    std::size_t written = 0;
    while (written < config.size()) {
        ssize_t n = ::write(in[1], config.data() + written, config.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break; // EPIPE: curl is gone; its exit status says why
        written += static_cast<std::size_t>(n);
    }
    closeQuietly(in[1]);

    std::string answer;
    char buffer[64];
    for (;;) {
        ssize_t n = ::read(out[0], buffer, sizeof buffer);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (answer.size() < 64) answer.append(buffer, static_cast<std::size_t>(n));
    }
    closeQuietly(out[0]);

    // Wait without reaping first, so close() can never signal a pid that has
    // already been reused: the zombie holds the pid until waitpid below.
    siginfo_t info;
    std::memset(&info, 0, sizeof info);
    while (::waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOWAIT) != 0 && errno == EINTR) {
    }
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.child = 0;
    }
    int status = 0;
    bool exited = false;
    for (;;) {
        pid_t reaped = ::waitpid(pid, &status, 0);
        if (reaped == pid) { exited = true; break; }
        if (reaped < 0 && errno == EINTR) continue;
        break; // ECHILD: the host reaps its own children; judge by the output
    }
    int http = parseStatus(answer);
    if (http > 0) return http;
    if (exited && WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        error = "could not run " TRACE_CLIENT_CURL;
    } else if (exited && WIFEXITED(status)) {
        std::ostringstream message;
        message << "curl exited with " << WEXITSTATUS(status);
        error = message.str();
    } else {
        error = "curl did not finish";
    }
    return -1;
}

#elif defined(_WIN32)

inline bool findCurl(std::string &path) {
    char buffer[MAX_PATH];
    UINT length = ::GetSystemDirectoryA(buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        std::string candidate = std::string(buffer, length) + "\\curl.exe";
        DWORD attributes = ::GetFileAttributesA(candidate.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            path = candidate;
            return true;
        }
    }
    // Only the directories on PATH -- never the current directory, where a
    // stray curl.exe next to the program would otherwise be picked up.
    std::string searchPath = getEnvironment("PATH");
    if (searchPath.empty()) return false;
    std::string name = std::string(TRACE_CLIENT_CURL) + ".exe";
    DWORD found = ::SearchPathA(searchPath.c_str(), name.c_str(), NULL, MAX_PATH, buffer, NULL);
    if (found == 0 || found >= MAX_PATH) return false;
    path.assign(buffer, found);
    return true;
}

/** See the POSIX send(): same fixed command line, request on standard input. */
inline int send(Shared &shared, const std::string &body, std::string &error) {
    std::string curl;
    if (!findCurl(curl)) {
        error = "could not find curl.exe";
        return -1;
    }
    SECURITY_ATTRIBUTES inherit;
    inherit.nLength = sizeof inherit;
    inherit.lpSecurityDescriptor = NULL;
    inherit.bInheritHandle = TRUE;
    HANDLE inRead = NULL, inWrite = NULL, outRead = NULL, outWrite = NULL, nul = INVALID_HANDLE_VALUE;
    bool ready = ::CreatePipe(&inRead, &inWrite, &inherit, 1 << 16)
                 && ::SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0)
                 && ::CreatePipe(&outRead, &outWrite, &inherit, 0)
                 && ::SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    if (ready) {
        nul = ::CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                            OPEN_EXISTING, 0, NULL);
        ready = nul != INVALID_HANDLE_VALUE;
    }
    PROCESS_INFORMATION process;
    std::memset(&process, 0, sizeof process);
    if (ready) {
        STARTUPINFOA startup;
        std::memset(&startup, 0, sizeof startup);
        startup.cb = sizeof startup;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = inRead;
        startup.hStdOutput = outWrite;
        startup.hStdError = nul;
        char commandLine[] = "curl.exe -q --silent --config -";
        ready = ::CreateProcessA(curl.c_str(), commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                 NULL, NULL, &startup, &process) != 0;
        if (!ready) error = "could not run curl.exe";
    } else {
        error = "could not create a pipe";
    }
    if (inRead) ::CloseHandle(inRead);
    if (outWrite) ::CloseHandle(outWrite);
    if (nul != INVALID_HANDLE_VALUE) ::CloseHandle(nul);
    if (!ready) {
        if (inWrite) ::CloseHandle(inWrite);
        if (outRead) ::CloseHandle(outRead);
        return -1;
    }
    ::CloseHandle(process.hThread);
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.child = process.hProcess;
        if (shared.abandoned) shared.killChild();
    }
    std::string config = curlConfig(shared, body);
    std::size_t written = 0;
    while (written < config.size()) {
        DWORD n = 0;
        if (!::WriteFile(inWrite, config.data() + written, static_cast<DWORD>(config.size() - written), &n, NULL) || n == 0) break;
        written += n;
    }
    ::CloseHandle(inWrite);
    std::string answer;
    char buffer[64];
    for (;;) {
        DWORD n = 0;
        if (!::ReadFile(outRead, buffer, sizeof buffer, &n, NULL) || n == 0) break;
        if (answer.size() < 64) answer.append(buffer, n);
    }
    ::CloseHandle(outRead);
    ::WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    ::GetExitCodeProcess(process.hProcess, &exitCode);
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.child = NULL;
    }
    ::CloseHandle(process.hProcess);
    int http = parseStatus(answer);
    if (http > 0) return http;
    std::ostringstream message;
    message << "curl exited with " << exitCode;
    error = message.str();
    return -1;
}

#endif

#if !defined(__EMSCRIPTEN__) || defined(TRACE_CLIENT_USE_LIBCURL)
#define TRACE_CLIENT_CAN_SEND 1

inline void deliver(Shared &shared, const std::string &body) {
    std::string error;
    int status = -1;
    try {
        status = send(shared, body, error);
    } catch (...) {
        error = "unexpected failure";
    }
    if (status < 0) {
        shared.log("could not deliver " + body + ": " + error);
    } else if (status != 201) {
        std::ostringstream message;
        message << "trace server answered " << status << " for " << body;
        shared.log(message.str());
    }
}

inline void drain(std::shared_ptr<Shared> shared) {
#if defined(TRACE_CLIENT_POSIX)
    // A write to a curl that died early must be an EPIPE here, never a
    // SIGPIPE that kills the host program. Only this thread is affected.
    sigset_t pipe;
    sigemptyset(&pipe);
    sigaddset(&pipe, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &pipe, NULL);
#endif
    for (;;) {
        std::string body;
        {
            std::unique_lock<std::mutex> lock(shared->mutex);
            while (shared->queue.empty() && !shared->closing && !shared->abandoned) {
                shared->changed.wait(lock);
            }
            if (shared->abandoned || shared->queue.empty()) {
                shared->done = true;
                shared->queue.clear();
                shared->changed.notify_all();
                return;
            }
            body.swap(shared->queue.front());
            shared->queue.pop_front();
        }
        deliver(*shared, body);
    }
}
#endif

} // namespace detail

/**
 * Whether the environment asks for usage reporting to be off, via
 * TRACE_USAGE_REPORTING=off (also false, 0, no) or DO_NOT_TRACK=1 (also true,
 * yes), case-insensitive. Any other value, or none, leaves the program's own
 * setting in charge.
 */
inline bool environmentOptsOut() {
    std::string reporting = detail::trimLower(detail::getEnvironment(ENV_TRACE_USAGE_REPORTING));
    if (reporting == "off" || reporting == "false" || reporting == "0" || reporting == "no") return true;
    std::string dnt = detail::trimLower(detail::getEnvironment(ENV_DO_NOT_TRACK));
    return dnt == "1" || dnt == "true" || dnt == "yes";
}

/**
 * Reports usage events to a trace server, and never gets in the way of the
 * program doing the reporting.
 *
 * - report() returns immediately: the HTTP call happens on one thread the
 *   client owns, so a game loop can report without a frame waiting.
 * - Nothing here throws. A server that is down, slow or rejecting the key,
 *   or a machine without curl, is a dropped report, mentioned to the
 *   optional Logger and otherwise not at all.
 * - At most QUEUE_CAPACITY reports wait to be sent; later ones are dropped.
 * - close() (also run by the destructor) gives queued reports up to
 *   TIMEOUT_SECONDS in total, then stops -- a CLI that reports and exits at
 *   once still sends, and an unreachable server never hangs the exit.
 *
 * Reporting is opt-out. The constructor decides once, first match wins, and
 * disabledReason() says which: the environment (REASON_ENVIRONMENT), the
 * program's own setting (REASON_CONFIG), a blank key (REASON_NO_KEY), or no
 * way to send (REASON_UNAVAILABLE).
 *
 *     trace_client::TraceClient trace("https://trace.example.org", "MyGame",
 *                                     settings.key, settings.usageReporting);
 *     trace.report("startup", {{"version", "1.4.0"}});
 *     ...
 *     trace.close(); // or let the destructor do it
 */
class TraceClient {
public:
    /** A client that reports nothing (reason "config"). */
    TraceClient() : reason_(REASON_CONFIG) {}

    TraceClient(const std::string &baseUrl, const std::string &application, const std::string &key,
                bool enabled = true, Logger logger = Logger()) {
        try {
            if (environmentOptsOut()) {
                reason_ = REASON_ENVIRONMENT;
            } else if (!enabled) {
                reason_ = REASON_CONFIG;
            } else if (detail::isBlank(key)) {
                reason_ = REASON_NO_KEY;
            } else if (detail::isBlank(baseUrl) || detail::isBlank(application)) {
                reason_ = REASON_UNAVAILABLE;
            } else {
                start(baseUrl, application, key, logger);
            }
        } catch (...) {
            shared_.reset();
            reason_ = REASON_UNAVAILABLE;
        }
    }

    ~TraceClient() { close(); }

    /** Whether report() will actually send anything. False after close(). */
    bool isEnabled() const { return static_cast<bool>(shared_); }

    /**
     * Why this client reports nothing: empty when it reports, otherwise one
     * of the REASON_* strings, verbatim. Unchanged by close().
     */
    const std::string &disabledReason() const { return reason_; }

    /** Reports that name happened, with optional tags. Returns immediately. */
    void report(const std::string &name, const Tags &tags = Tags()) { enqueue(name, false, 0.0, tags); }

    /** Reports that name happened, with a numeric value and optional tags. */
    void report(const std::string &name, double value, const Tags &tags = Tags()) {
        enqueue(name, true, value, tags);
    }

    /**
     * Stops the sending thread, giving reports already queued up to
     * timeoutSeconds in total to be sent first; whatever is still in flight
     * then is abandoned. Safe to call more than once, and on a disabled client.
     */
    void close(double timeoutSeconds = TIMEOUT_SECONDS) {
        if (!shared_) return;
        std::shared_ptr<detail::Shared> shared;
        shared.swap(shared_);
        try {
            std::unique_lock<std::mutex> lock(shared->mutex);
            shared->closing = true;
            shared->changed.notify_all();
            std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(static_cast<long long>(timeoutSeconds * 1000));
            while (!shared->done) {
                if (shared->changed.wait_until(lock, deadline) == std::cv_status::timeout) break;
            }
            if (shared->done) {
                lock.unlock();
                thread_.join();
            } else {
                shared->abandoned = true;
                shared->queue.clear();
                shared->killChild();
                shared->changed.notify_all();
                lock.unlock();
                shared->forgetLogger();
                // The thread holds its own reference to the shared state, so
                // it can finish on its own after this object is gone.
                thread_.detach();
            }
        } catch (...) {
            if (thread_.joinable()) thread_.detach();
        }
    }

private:
    TraceClient(const TraceClient &);
    TraceClient &operator=(const TraceClient &);

    void start(const std::string &baseUrl, const std::string &application, const std::string &key,
               const Logger &logger) {
#if defined(TRACE_CLIENT_CAN_SEND)
        std::shared_ptr<detail::Shared> shared = std::make_shared<detail::Shared>();
        std::string base = baseUrl;
        while (!base.empty() && (base[base.size() - 1] == '/' || std::isspace(static_cast<unsigned char>(base[base.size() - 1])))) {
            base.erase(base.size() - 1);
        }
        std::size_t first = base.find_first_not_of(" \t\r\n");
        shared->endpoint = (first == std::string::npos ? std::string() : base.substr(first)) + "/api/metrics";
        std::string app = application;
        std::size_t begin = app.find_first_not_of(" \t\r\n");
        std::size_t end = app.find_last_not_of(" \t\r\n");
        shared->application = app.substr(begin, end - begin + 1);
        std::string k = key;
        begin = k.find_first_not_of(" \t\r\n");
        end = k.find_last_not_of(" \t\r\n");
        shared->key = k.substr(begin, end - begin + 1);
        shared->logger = logger;
#if defined(TRACE_CLIENT_USE_LIBCURL)
        static std::once_flag initialised;
        std::call_once(initialised, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
#endif
        thread_ = std::thread(detail::drain, shared);
        shared_ = shared;
        reason_.clear();
#else
        (void) baseUrl; (void) application; (void) key; (void) logger;
        reason_ = REASON_UNAVAILABLE;
#endif
    }

    void enqueue(const std::string &name, bool hasValue, double value, const Tags &tags) {
        if (!shared_ || detail::isBlank(name)) return;
        try {
            std::string body = detail::json(shared_->application, name, hasValue, value, tags);
            bool full = false;
            {
                std::lock_guard<std::mutex> lock(shared_->mutex);
                full = shared_->queue.size() >= QUEUE_CAPACITY;
                if (!full) {
                    shared_->queue.push_back(body);
                    shared_->changed.notify_all();
                }
            }
            if (full) shared_->log("queue full, dropped " + name);
        } catch (...) {
            // a report must never be the reason a program stops
        }
    }

    std::shared_ptr<detail::Shared> shared_;
    std::thread thread_;
    std::string reason_;
};

} // namespace trace_client

#endif // TRACE_CLIENT_HPP
