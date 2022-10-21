#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "header/simulation.h"

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
        // system("clear");
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

        logger->log("initial entity ids: " + microbiome->getListOfEntityIds());

        logger->log("Making microorganisms move");
        microbiome->initiateMicroorganismMovement();

        logger->log("entity ids after movement: " + microbiome->getListOfEntityIds());

        if (config->isMicroorganismReproductionEnabled()) {
            logger->log("Making microorganisms reproduce");
            microbiome->initiateMicroorganismReproduction();
        }

        logger->log("entity ids after reproduction: " + microbiome->getListOfEntityIds());

        if (config->isSimulationOutputEnabled()) {
            // system("clear");
            microbiome->printConsoleRepresentation();
            printRunningStats();
        }

        if (config->getTickLengthInSeconds() > 0) {
            sleep(config->getTickLengthInSeconds());
        }

        if (microbiome->getNumAliveMicroorganisms() == 0) {
            running = false;
        }

        // initiate checks
        microbiome->checkForEntityIdMismatch();
        microbiome->checkForAmountMismatchBetweenEntitiesAndMicroorganisms();
    }
}

void Simulation::printRunningStats() {
    std::cout << "Name: " << microbiome->getName() << std::endl;
    std::cout << "Size: " << microbiome->getGrid()->getSize() << "x" << microbiome->getGrid()->getSize() << std::endl;
    std::cout << "Microbes: " << microbiome->getNumAliveMicroorganisms() << std::endl;
    std::cout << "Dead Microbe Biomatter: " << microbiome->getNumDeadMicroorganisms() << "\n";
    std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
    std::cout << "" << std::endl;
    if (config->getMaxTicks() > 0) {
        std::cout << "Ticks elapsed: " << numTicks << " of " << config->getMaxTicks() << std::endl;
    }
    else {
        std::cout << "Ticks elapsed: " << numTicks << std::endl;
    }

    // print number of entities in environment
    std::cout << "Entities in environment: " << microbiome->getNumEntities() << std::endl;

    // print number of microorganisms in vector
    std::cout << "Microorganisms in vector: " << microbiome->getMicroorganisms().size() << std::endl;
    
}

void Simulation::printFinishedStats() {
    std::cout << "" << std::endl;
    std::cout << "=== Simulation Complete ===" << std::endl;
    std::cout << "Locations: " << microbiome->getGrid()->getLocations().size() << "\n";
    std::cout << "Surviving Microbes: " << microbiome->getNumAliveMicroorganisms() << "\n";
    std::cout << "Dead Microbe Biomatter: " << microbiome->getNumDeadMicroorganisms() << "\n";
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