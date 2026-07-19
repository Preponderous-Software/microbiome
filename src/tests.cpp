#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <chrono>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>
#include <assert.h>

#include "header/appConfig.h"
#include "header/simulation.h"
#include "header/result.h"
#include "header/microorganism.h"
#include "header/microbiome.h"
#include "header/webServer.h"

void testTemplate() {
    std::cout << "Test - Template";
    assert(true);
    std::cout << " --- " << "Success" << std::endl;
}

// appConfig tests -------------------------------------------------------------

void testConfigSettersAndGetters() {
    std::cout << "Test - AppConfig";
    
    // create a config object
    AppConfig config;

    // set values
    config.setEnvironmentSize(10);
    config.setEntityFactor(5);
    config.setMaxTicks(100);
    config.setTickLengthInSeconds(1);
    config.setNumSimulations(5);
    config.setSimulationOutputEnabled(true);

    // get values
    assert(config.getEnvironmentSize() == 10);
    assert(config.getEntityFactor() == 5);
    assert(config.getMaxTicks() == 100);
    assert(config.getTickLengthInSeconds() == 1);
    assert(config.getNumSimulations() == 5);
    assert(config.isSimulationOutputEnabled() == true);

    std::cout << " --- " << "Success" << std::endl;
}


// microorganism tests -------------------------------------------------------------
void testMicroorganismEnergyConsumption() {
    std::cout << "Test - Microorganism Energy";
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.setEnergy(10);
    assert(microorganism.getEnergy() == 10);
    microorganism.metabolize();
    assert(microorganism.getEnergy() == 10 - microorganism.getMetabolicRate());
    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismTimesMoved() {
    std::cout << "Test - Microorganism Times Moved";
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 1);
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 2);
    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismTimesEaten() {
    std::cout << "Test - Microorganism Times Eaten";
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 1);
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 2);
    std::cout << " --- " << "Success" << std::endl;
}

// microorganism factory tests -------------------------------------------------------------
void testMicroorganismFactory() {
    std::cout << "Test - Microorganism Factory";
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    assert(microorganism.getId() == 0);

    Microorganism microorganism2 = factory.createMicroorganism();
    assert(microorganism2.getId() == 1);

    Microorganism microorganism3 = factory.createMicroorganism();
    assert(microorganism3.getId() == 2);
    std::cout << " --- " << "Success" << std::endl;
}


// microbiome tests -------------------------------------------------------------
void testMicrobiomeCreation() {
    std::cout << "Test - Microbiome Creation";
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);
    int numEntitiesInEnvironment = microbiome.getNumEntities();
    int numTrackedMicroorganisms = microbiome.getMicroorganisms().size();
    assert(numEntitiesInEnvironment == numTrackedMicroorganisms);
    std::cout << " --- " << "Success" << std::endl;
}

void testRemovingMicroorganismFromMicrobiome() {
    std::cout << "Test - Removing Microorganism From Microbiome" << std::endl;
    
    // create microbiome
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    // create microorganism
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();

    // add microorganism to microbiome
    microbiome.addMicroorganism(microorganism);

    // verify presence
    assert(microbiome.isMicroorganismPresent(microorganism.getId()));

    // remove microorganism from microbiome
    microbiome.removeMicroorganism(microorganism.getId());

    // verify absence
    assert(!microbiome.isMicroorganismPresent(microorganism.getId()));

    // verify that microorganism is no longer in the microorganisms vector
    std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
    bool microorganismFound = false;
    for (Microorganism* m : microorganisms) {
        if (m->getId() == microorganism.getId()) {
            microorganismFound = true;
        }
    }
    assert(!microorganismFound);
    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismReproduction() {
    std::cout << "Test - Microorganism Reproduction";
    int id = 0;
    int size = 3;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    size_t populationBefore = microbiome.getMicroorganisms().size();

    // give one microorganism enough energy to reproduce this tick; the
    // maximum single-tick metabolic cost (4) can't bring it back under the
    // reproduction threshold (900), so reproduction is guaranteed to occur.
    microbiome.getMicroorganisms()[0]->setEnergy(950);

    microbiome.initiateMicroorganismMovement();

    size_t populationAfter = microbiome.getMicroorganisms().size();
    assert(populationAfter > populationBefore);
    std::cout << " --- " << "Success" << std::endl;
}

void testDeathProducesForageableBiomatter() {
    std::cout << "Test - Death Produces Foragable Biomatter";
    int id = 0;
    int size = 3;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
    size_t populationSize = microorganisms.size();
    for (Microorganism* microorganism : microorganisms) {
        // low enough that any metabolic rate (1-4) kills it this tick
        microorganism->setEnergy(1);
    }

    microbiome.initiateMicroorganismMovement();

    assert(microbiome.getNumAliveMicroorganisms() == 0);
    assert(microbiome.getNumDeadMicroorganisms() == (int) populationSize);
    // the population's energy didn't just vanish - it should still be
    // recoverable as biomatter in the environment.
    assert(microbiome.getTotalEnergy() > 0);
    std::cout << " --- " << "Success" << std::endl;
}


// simulation tests -------------------------------------------------------------
void testSimulationCreation() {
    std::cout << "Test - Simulation Creation";
    AppConfig config;
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    if (config.getMaxTicks() > 0) {
        assert(simulation.getTicksElapsed() >= config.getMaxTicks());
    }
    std::cout << " --- " << "Success" << std::endl;
}

void testRunningSimulation() {
    std::cout << "Test - Running Simulation";
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    simulation.run();
    assert(simulation.getTicksElapsed() >= config.getMaxTicks());
    std::cout << " --- " << "Success" << std::endl;
}

void testSimulationResults() {
    std::cout << "Test - Simulation Results";
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    simulation.run();
    Result result = Result(&simulation);
    assert(result.getSurvivingMicroorganisms() == simulation.getSurvivingMicroorganisms());
    assert(result.getDeadMicroorganisms() == simulation.getDeadMicroorganisms());
    assert(result.getEnergy() == simulation.getEnergy());
    assert(result.getTicksElapsed() == simulation.getTicksElapsed());
    std::cout << " --- " << "Success" << std::endl;
}

// web server tests -------------------------------------------------------------
std::string httpGetBody(int port, std::string path) {
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

            size_t bodyStart = response.find("\r\n\r\n");
            if (bodyStart == std::string::npos) {
                return "";
            }
            return response.substr(bodyStart + 4);
        }

        // server may not have started listening yet
        close(sock);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return "";
}

void testWebServerServesSimulationState() {
    std::cout << "Test - Web Server Serves Simulation State";

    AppConfig config;
    config.setEnvironmentSize(3);
    config.setEntityFactor(1);
    config.setSimulationOutputEnabled(false);

    int port = 18123;
    WebServer server(&config, port);
    std::thread serverThread(&WebServer::run, &server);

    std::string body = httpGetBody(port, "/api/state");

    server.stop();
    serverThread.join();

    assert(!body.empty());
    assert(body.find("\"gridSize\":3") != std::string::npos);
    assert(body.find("\"microorganisms\"") != std::string::npos);
    assert(body.find("\"biomatter\"") != std::string::npos);
    std::cout << " --- " << "Success" << std::endl;
}


void seedRandomNumberGenerator() {
    srand (time (NULL));
}

int main() {
    // seed RNG
    seedRandomNumberGenerator();
    
    // run appConfig tests
    testConfigSettersAndGetters();

    // run microorganism tests
    testMicroorganismEnergyConsumption();
    testMicroorganismTimesMoved();
    testMicroorganismTimesEaten();

    // run microorganism factory tests
    testMicroorganismFactory();

    // run microbiome tests
    testMicrobiomeCreation();
    testMicroorganismReproduction();
    testDeathProducesForageableBiomatter();
    testSimulationCreation();

    // run simulation tests
    testRunningSimulation();
    testSimulationResults();

    // run web server tests
    testWebServerServesSimulationState();
    return 0;
}