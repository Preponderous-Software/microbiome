// Tests for WebServer: its HTTP endpoints (GET /api/state, the index page and GET /og.png), the
// simulation loop behind them and how run() fails. They link Ulfius, so they build as their own
// binary (`make catch2_web_tests`, ./mb_web_tests) and `make catch2_tests` stays free of that
// dependency. Each test uses its own 127.0.0.1 port and talks to the server over a plain socket.
#define CATCH_CONFIG_MAIN  // This binary's only test file, so it provides main()
#include "lib/catch2.hpp"

#include "../src/header/appConfig.h"
#include "../src/header/webServer.h"

#include <jansson.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace {

// The whole response, status line and headers included.
std::string httpGet(int port, std::string path) {
    for (int attempt = 0; attempt < 40; attempt++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == 0) {
            std::string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            send(sock, request.c_str(), request.size(), 0);

            std::string response;
            char buffer[4096];
            ssize_t bytesRead;
            while ((bytesRead = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
                response.append(buffer, bytesRead);
            }
            close(sock);
            return response;
        }

        // server may not have started listening yet
        close(sock);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return "";
}

std::string bodyOf(const std::string& response) {
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart == std::string::npos) {
        return "";
    }
    return response.substr(bodyStart + 4);
}

// Serves a 3x3 microbiome on the given port just long enough to answer one request.
std::string fetchFromServer(int port, std::string path) {
    AppConfig config;
    config.setEnvironmentSize(3);
    config.setEntityFactor(1);
    config.setSimulationOutputEnabled(false);

    WebServer server(&config, port);
    std::thread serverThread(&WebServer::run, &server);

    std::string response = httpGet(port, path);

    server.stop();
    serverThread.join();
    return response;
}

bool contains(const std::string& text, const std::string& fragment) {
    return text.find(fragment) != std::string::npos;
}

// A server left running across several requests. The destructor stops it, so a failed REQUIRE
// cannot destroy a joinable thread.
class RunningServer {
    public:
        RunningServer(AppConfig* config, int listenPort)
            : server(config, listenPort), serverThread(&WebServer::run, &server) {}

        ~RunningServer() {
            server.stop();
            serverThread.join();
        }

    private:
        WebServer server;
        std::thread serverThread;
};

// Polls GET /api/state until accept(state) holds and returns that state, which the caller must
// json_decref, or nullptr if it has not held after timeoutSeconds.
template <typename Predicate>
json_t* waitForState(int port, int timeoutSeconds, Predicate accept) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        json_error_t error;
        json_t* state = json_loads(bodyOf(httpGet(port, "/api/state")).c_str(), 0, &error);
        if (state != nullptr) {
            if (accept(state)) {
                return state;
            }
            json_decref(state);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

json_int_t integerField(json_t* object, const char* key) {
    return json_integer_value(json_object_get(object, key));
}

}

TEST_CASE("Web server serves the simulation state as JSON", "[webserver]") {
    std::string body = bodyOf(fetchFromServer(18123, "/api/state"));

    REQUIRE_FALSE(body.empty());
    REQUIRE(contains(body, "\"gridSize\":3"));
    REQUIRE(contains(body, "\"microorganisms\""));
    REQUIRE(contains(body, "\"biomatter\""));
}

// The stats and microorganism fields the index page's render() reads from /api/state, with the
// types it expects and values that agree with each other. The request is answered within a couple
// of seconds, and an organism starts with more than 100 energy and spends at most 4 per 200 ms
// tick, so nothing can have died yet: this is still the first generation, with no deaths and no
// biomatter (the next test covers a biomatter entry's fields).
TEST_CASE("Web server state carries the viewer's stats and microorganism fields", "[webserver]") {
    std::string body = bodyOf(fetchFromServer(18127, "/api/state"));

    json_error_t error;
    json_t* root = json_loads(body.c_str(), 0, &error);
    REQUIRE(root != nullptr);
    REQUIRE(json_is_object(root));

    for (const char* key : {"generation", "tick", "gridSize", "aliveCount", "deadCount", "totalEnergy"}) {
        INFO("field " << key);
        REQUIRE(json_is_integer(json_object_get(root, key)));
    }
    REQUIRE(json_integer_value(json_object_get(root, "generation")) == 1);
    REQUIRE(json_integer_value(json_object_get(root, "tick")) >= 0);
    REQUIRE(json_integer_value(json_object_get(root, "gridSize")) == 3);
    REQUIRE(json_integer_value(json_object_get(root, "deadCount")) == 0);

    json_t* organisms = json_object_get(root, "microorganisms");
    REQUIRE(json_is_array(organisms));
    // fission may already have added daughters to the size x entityFactor = 3 initial population
    REQUIRE(json_array_size(organisms) >= 3);
    REQUIRE(json_integer_value(json_object_get(root, "aliveCount")) == (json_int_t) json_array_size(organisms));

    json_int_t energySum = 0;
    size_t index;
    json_t* entry;
    json_array_foreach(organisms, index, entry) {
        for (const char* key : {"id", "x", "y", "energy", "timesEaten"}) {
            INFO("microorganism " << index << " field " << key);
            REQUIRE(json_is_integer(json_object_get(entry, key)));
        }
        json_int_t x = json_integer_value(json_object_get(entry, "x"));
        json_int_t y = json_integer_value(json_object_get(entry, "y"));
        REQUIRE((x >= 0 && x < 3));
        REQUIRE((y >= 0 && y < 3));
        REQUIRE(json_integer_value(json_object_get(entry, "energy")) > 0);
        energySum += json_integer_value(json_object_get(entry, "energy"));
    }

    json_t* biomatter = json_object_get(root, "biomatter");
    REQUIRE(json_is_array(biomatter));
    REQUIRE(json_array_size(biomatter) == 0);
    REQUIRE(json_integer_value(json_object_get(root, "totalEnergy")) == energySum);

    json_decref(root);
}

// The server ticks on its own every 200 ms, and a death has to show up in /api/state. The first
// one can take a while: an organism starts with more than 100 energy and spends at most 4 per
// tick, so nothing dies before about 5 seconds. A larger population than the other tests' keeps
// that close to the minimum, since one of many is likely to start low on energy.
TEST_CASE("Web server state shows the simulation ticking, its deaths and their biomatter", "[webserver]") {
    AppConfig config;
    config.setEnvironmentSize(6);
    config.setEntityFactor(10);
    config.setSimulationOutputEnabled(false);
    RunningServer server(&config, 18130);

    json_t* state = waitForState(18130, 60, [](json_t* candidate) {
        return integerField(candidate, "deadCount") > 0;
    });
    REQUIRE(state != nullptr);

    REQUIRE(integerField(state, "tick") >= 25);
    REQUIRE(integerField(state, "generation") == 1);

    json_int_t energySum = 0;
    size_t index;
    json_t* entry;
    json_t* organisms = json_object_get(state, "microorganisms");
    REQUIRE(json_is_array(organisms));
    REQUIRE(integerField(state, "aliveCount") == (json_int_t) json_array_size(organisms));
    json_array_foreach(organisms, index, entry) {
        energySum += integerField(entry, "energy");
    }

    json_t* biomatter = json_object_get(state, "biomatter");
    REQUIRE(json_is_array(biomatter));
    REQUIRE(json_array_size(biomatter) >= 1);
    REQUIRE((json_int_t) json_array_size(biomatter) <= integerField(state, "deadCount"));
    json_array_foreach(biomatter, index, entry) {
        for (const char* key : {"id", "x", "y", "energy"}) {
            INFO("biomatter " << index << " field " << key);
            REQUIRE(json_is_integer(json_object_get(entry, key)));
        }
        REQUIRE((integerField(entry, "x") >= 0 && integerField(entry, "x") < 6));
        REQUIRE((integerField(entry, "y") >= 0 && integerField(entry, "y") < 6));
        // what biomatterYieldPerDeath leaves, less anything already foraged; empty biomatter is purged
        REQUIRE((integerField(entry, "energy") > 0 && integerField(entry, "energy") <= 60));
        energySum += integerField(entry, "energy");
    }
    REQUIRE(integerField(state, "totalEnergy") == energySum);

    json_decref(state);
}

// An empty population counts as extinct, so a server with no organisms rolls over to a new
// generation every tick. A real extinction takes minutes at 200 ms per tick, which also leaves
// the tick counter's reset uncovered here: with nothing alive the tick never leaves 0.
TEST_CASE("Web server starts a new generation once the population is extinct", "[webserver]") {
    AppConfig config;
    config.setEnvironmentSize(3);
    config.setEntityFactor(0);
    config.setSimulationOutputEnabled(false);
    RunningServer server(&config, 18129);

    json_t* state = waitForState(18129, 10, [](json_t* candidate) {
        return integerField(candidate, "generation") >= 3;
    });
    REQUIRE(state != nullptr);

    REQUIRE(integerField(state, "aliveCount") == 0);
    REQUIRE(integerField(state, "deadCount") == 0);
    REQUIRE(integerField(state, "tick") == 0);
    REQUIRE(json_array_size(json_object_get(state, "microorganisms")) == 0);

    json_decref(state);
}

TEST_CASE("Web server run() fails with the port in its message when the port is invalid", "[webserver]") {
    AppConfig config;
    config.setEnvironmentSize(3);
    config.setEntityFactor(1);

    SECTION("port 0") {
        WebServer server(&config, 0);
        REQUIRE_THROWS_WITH(server.run(), "Failed to initialize web server instance on port 0");
    }

    SECTION("a port above 65535") {
        WebServer server(&config, 70000);
        REQUIRE_THROWS_WITH(server.run(), "Failed to initialize web server instance on port 70000");
    }
}

// run() has already started the simulation thread when the framework fails to start, so this also
// covers that it stops that thread again: the server is destroyed right after the throw.
TEST_CASE("Web server run() fails with the port in its message when the port is in use", "[webserver]") {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(listener >= 0);
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18128);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    REQUIRE(bind(listener, (struct sockaddr*) &addr, sizeof(addr)) == 0);
    REQUIRE(listen(listener, 1) == 0);

    AppConfig config;
    config.setEnvironmentSize(3);
    config.setEntityFactor(1);
    {
        WebServer server(&config, 18128);
        REQUIRE_THROWS_WITH(server.run(), "Failed to start web server on port 18128");
    }

    close(listener);
}

TEST_CASE("Web server index links to the portfolio in its footer", "[webserver]") {
    std::string body = bodyOf(fetchFromServer(18124, "/"));

    REQUIRE(contains(body, "<footer>"));
    REQUIRE(contains(body, "href=\"https://danielstephenson.dev\""));
}

TEST_CASE("Web server index carries share metadata for the public site", "[webserver]") {
    std::string body = bodyOf(fetchFromServer(18125, "/"));

    REQUIRE(contains(body, "<meta name=\"description\" content=\""));
    REQUIRE(contains(body, "<meta property=\"og:title\" content=\"Microbiome\">"));
    REQUIRE(contains(body, "<meta property=\"og:description\" content=\""));
    REQUIRE(contains(body, "<meta property=\"og:type\" content=\"website\">"));
    REQUIRE(contains(body, "<meta property=\"og:url\" content=\"https://microbiome.preponderous.org/\">"));
    REQUIRE(contains(body, "<link rel=\"canonical\" href=\"https://microbiome.preponderous.org/\">"));
    REQUIRE(contains(body, "<meta property=\"og:image\" content=\"https://microbiome.preponderous.org/og.png\">"));
    REQUIRE(contains(body, "<meta property=\"og:image:type\" content=\"image/png\">"));
    REQUIRE(contains(body, "<meta property=\"og:image:width\" content=\"1200\">"));
    REQUIRE(contains(body, "<meta property=\"og:image:height\" content=\"630\">"));
    REQUIRE(contains(body, "<meta name=\"twitter:card\" content=\"summary_large_image\">"));
    REQUIRE(contains(body, "<meta name=\"twitter:image\" content=\"https://microbiome.preponderous.org/og.png\">"));
    REQUIRE_FALSE(contains(body, "localhost"));

    // The alt text describes the picture assets/og.png shows, so it is the card
    // spec's own "alt" (og-card/card.json, which assets/og.png is drawn from),
    // word for word, on both tags. The spec's alt holds no '"', so the value
    // runs from its opening quote to the next one.
    std::ifstream specFile("og-card/card.json");
    REQUIRE(specFile.good());
    std::string spec((std::istreambuf_iterator<char>(specFile)), std::istreambuf_iterator<char>());
    const std::string key = "\"alt\": \"";
    size_t altStart = spec.find(key);
    REQUIRE(altStart != std::string::npos);
    altStart += key.size();
    size_t altEnd = spec.find('"', altStart);
    REQUIRE(altEnd != std::string::npos);
    std::string alt = spec.substr(altStart, altEnd - altStart);
    REQUIRE(alt.size() > 20);
    REQUIRE(contains(body, "<meta property=\"og:image:alt\" content=\"" + alt + "\">"));
    REQUIRE(contains(body, "<meta name=\"twitter:image:alt\" content=\"" + alt + "\">"));
}

// og:image names /og.png, so the server must answer it with the PNG. The bytes
// are compared with assets/og.png (the suite runs from the repository root), so
// a redrawn image whose header/ogImage.h was not regenerated fails here.
TEST_CASE("Web server serves the link-preview image at /og.png", "[webserver]") {
    std::string response = fetchFromServer(18126, "/og.png");

    std::ifstream file("assets/og.png", std::ios::binary);
    REQUIRE(file.good());
    std::string expected((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    size_t bodyStart = response.find("\r\n\r\n");
    REQUIRE(bodyStart != std::string::npos);
    std::string head = response.substr(0, bodyStart);
    std::string body = response.substr(bodyStart + 4);
    REQUIRE(contains(head, " 200 "));
    REQUIRE(contains(head, "Content-Type: image/png"));
    REQUIRE(body == expected);

    // PNG signature, then IHDR: big-endian width and height at bytes 16 and 20.
    REQUIRE(body.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0);
    const unsigned char* b = reinterpret_cast<const unsigned char*>(body.data());
    REQUIRE(((b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19]) == 1200);
    REQUIRE(((b[20] << 24) | (b[21] << 16) | (b[22] << 8) | b[23]) == 630);
}
