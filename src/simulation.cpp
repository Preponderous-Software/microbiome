#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "header/simulation.h"

Simulation::Simulation(Config* config, int id, std::string name) {
    this->config = config;
    this->microbiome = new Microbiome(id, name, config->getEnvironmentSize(), config->getEntityFactor());
}

Simulation::~Simulation() {
    delete microbiome;
}

void Simulation::run() {
    while (running) {
        microbiome->initiateMicroorganismMovement();

        system("clear");
        microbiome->printConsoleRepresentation();
        printRunningStats();

        numTicks++;
        if (config->getMaxTicks() > 0 && numTicks >= config->getMaxTicks()) {
            running = false;
        }

        if (config->getTickLengthInSeconds() > 0) {
            sleep(config->getTickLengthInSeconds());
        }

        if (microbiome->getNumAliveMicroorganisms() == 0) {
            running = false;
        }
    }
}

void Simulation::runWithNoOutput() {
    while (running) {
        microbiome->initiateMicroorganismMovement();

        numTicks++;
        if (config->getMaxTicks() > 0 && numTicks >= config->getMaxTicks()) {
            running = false;
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