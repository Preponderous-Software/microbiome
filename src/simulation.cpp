#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "header/simulation.h"

// helper methods
std::string getListOfEntityIds(Microbiome* microbiome) {
    std::string toReturn = "";
    for (Location& location : microbiome->getGrid()->getLocations()) {
        for (Entity* entity : location.getEntities()) {
            toReturn += "" + std::to_string(entity->getId()) + ", ";
        }
    }
    return toReturn.substr(0, toReturn.size() - 2);
}

Simulation::Simulation(AppConfig* config, int id, std::string name) {
    this->logger = new Logger("log.simulation.txt");
    logger->log("Creating simulation " + std::to_string(id) + " with name '" + name + "'");
    this->config = config;
    this->microbiome = new Microbiome(id, name, config->getEnvironmentSize(), config->getEntityFactor());
}

Simulation::~Simulation() {
    logger->log("Destroying simulation " + std::to_string(microbiome->getId()));
    delete microbiome;
    delete logger;
}

void Simulation::run() {
    if (config->isSimulationOutputEnabled()) {
        system("clear");
        microbiome->printConsoleRepresentation();
        printRunningStats();
    }
    if (config->getTickLengthInSeconds() > 0) {
        sleep(config->getTickLengthInSeconds());
    }
    while (running) {
        numTicks++;
        if (config->getMaxTicks() > 0 && numTicks >= config->getMaxTicks()) {
            running = false;
        }

        logger->log("initial entity ids: " + getListOfEntityIds(microbiome));

        logger->log("Making microorganisms move");
        microbiome->initiateMicroorganismMovement();

        logger->log("entity ids after movement: " + getListOfEntityIds(microbiome));

        if (config->isMicroorganismReproductionEnabled()) {
            logger->log("Making microorganisms reproduce");
            microbiome->initiateMicroorganismReproduction();
        }

        logger->log("entity ids after reproduction: " + getListOfEntityIds(microbiome));

        // microbiome->purgeMicroorganismsNotInEnvironment();

        if (config->isSimulationOutputEnabled()) {
            system("clear");
            microbiome->printConsoleRepresentation();
            printRunningStats();
        }

        if (config->getTickLengthInSeconds() > 0) {
            sleep(config->getTickLengthInSeconds());
        }

        if (microbiome->getNumAliveMicroorganisms() == 0) {
            running = false;
        }
    }
}

void Simulation::printRunningStats() {
    std::cout << "Name: " << microbiome->getName() << std::endl;
    std::cout << "Size: " << microbiome->getGrid()->getSize() << "x" << microbiome->getGrid()->getSize() << std::endl;
    std::cout << "Microorganisms: " << microbiome->getNumAliveMicroorganisms() << std::endl;
    std::cout << "Dead Microorganisms: " << microbiome->getNumDeadMicroorganisms() << "\n";
    std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
    std::cout << "" << std::endl;
    if (config->getMaxTicks() > 0) {
        std::cout << "Ticks elapsed: " << numTicks << " of " << config->getMaxTicks() << std::endl;
    }
    else {
        std::cout << "Ticks elapsed: " << numTicks << std::endl;
    }
}

void Simulation::printFinishedStats() {
    std::cout << "" << std::endl;
    std::cout << "=== Simulation Complete ===" << std::endl;
    std::cout << "Locations: " << microbiome->getGrid()->getLocations().size() << "\n";
    std::cout << "Surviving Microorganisms: " << microbiome->getNumAliveMicroorganisms() << "\n";
    std::cout << "Dead Microorganisms: " << microbiome->getNumDeadMicroorganisms() << "\n";
    std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
}

int Simulation::getSurvivingMicroorganisms() {
    return microbiome->getNumAliveMicroorganisms();
}

int Simulation::getDeadMicroorganisms() {
    return microbiome->getNumDeadMicroorganisms();
}

int Simulation::getEnergy() {
    return microbiome->getTotalEnergy();
}

int Simulation::getTicksElapsed() {
    return numTicks;
}