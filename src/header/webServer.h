#ifndef WebServer_h
#define WebServer_h

#include <atomic>
#include <mutex>
#include <ostream>
#include <thread>

#include "appConfig.h"
#include "microbiome.h"

struct _u_instance;
struct _u_response;

/**
 * The WebServer class hosts a small REST API and web page (built on Ulfius)
 * that expose the state of a continuously-running Microbiome simulation, so
 * it can be viewed live in a browser instead of only in the console.
 * @author Daniel McCoy Stephenson
 */
class WebServer {
    public:
        WebServer(AppConfig* config, int port);
        ~WebServer();
        void run();
        void stop();
        void writeStateResponse(_u_response* response);
        void writeIndexResponse(_u_response* response);
        void writeOgImageResponse(_u_response* response);
    private:
        void runSimulationLoop();

        AppConfig* config;
        int port;
        _u_instance* instance;
        Microbiome* microbiome = nullptr;
        std::mutex stateMutex;
        std::atomic<bool> keepRunning{false};
        std::thread simulationThread;

        int tickIntervalMs = 200;
        int generation = 0;
        int tickCount = 0;
};

/**
 * Reads the port mb_webapp listens on from the value of MICROBIOME_WEB_PORT:
 * nullptr (the variable is unset) means the default 8080. Throws
 * std::invalid_argument quoting the value if it is not a number from 1 to 65535.
 */
int parseWebPort(const char* portValue);

/**
 * mb_webapp's body, apart from main() so the web server suite can link it:
 * serves the simulation until the server stops and returns 0, or writes why it
 * could not start to errors and returns 1.
 */
int runWebApp(const char* portValue, std::ostream& errors);

#endif
