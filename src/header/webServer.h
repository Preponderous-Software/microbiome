#ifndef WebServer_h
#define WebServer_h

#include <atomic>
#include <mutex>
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

#endif
