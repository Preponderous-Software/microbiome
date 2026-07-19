#include <cstdlib>
#include <iostream>

#include "header/appConfig.h"
#include "header/webServer.h"

int main() {
    AppConfig config;
    config.setSimulationOutputEnabled(false);

    int port = 8080;
    const char* portFromEnv = std::getenv("MICROBIOME_WEB_PORT");
    if (portFromEnv != nullptr) {
        port = std::atoi(portFromEnv);
    }

    WebServer server(&config, port);
    server.run();
    return 0;
}
