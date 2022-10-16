#include "header/mbapp.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace envlibcpp;

MicrobiomeApplication::MicrobiomeApplication() {
    microbiome = new Microbiome(0, "microbiome", config.getEnvironmentSize(), config.getEntityFactor());
}

void MicrobiomeApplication::printRunningStats() {
        std::cout << "Name: " << microbiome->getName() << std::endl;
        std::cout << "Size: " << microbiome->getGrid()->getSize() << "x" << microbiome->getGrid()->getSize() << std::endl;
        std::cout << "Microorganisms: " << microbiome->getNumAliveMicroorganisms() << std::endl;
        std::cout << "Dead Microorganisms: " << microbiome->getNumDeadMicroorganisms() << "\n";
        std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
        std::cout << "" << std::endl;
        std::cout << "Ticks Left: " << config.getMaxTicks() - numTicks << std::endl;
}

void MicrobiomeApplication::printFinishedStats() {
    std::cout << "" << std::endl;
    std::cout << "=== Simulation Complete ===" << std::endl;
    std::cout << "Locations: " << microbiome->getGrid()->getLocations().size() << "\n";
    std::cout << "Surviving Microorganisms: " << microbiome->getNumAliveMicroorganisms() << "\n";
    std::cout << "Dead Microorganisms: " << microbiome->getNumDeadMicroorganisms() << "\n";
    std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
}

bool MicrobiomeApplication::run() {
    int totalSurvivingMicroorganisms = 0;
    int totalDeadMicroorganisms = 0;
    int totalEnergy = 0;
    int totalTicksElapsed = 0;    
    for (int i = 0; i < numSimulations; i++) {
        while (running) {
            microbiome->initiateMicroorganismMovement();

            system("clear");
            microbiome->printConsoleRepresentation();
            printRunningStats();

            numTicks++;
            if (numTicks == config.getMaxTicks()) {
                running = false;
            }
            if (config.getTickLengthInSeconds() > 0) {
                sleep(config.getTickLengthInSeconds());
            }

            if (microbiome->getNumAliveMicroorganisms() == 0) {
                running = false;
            }
        }
        printFinishedStats();
        totalSurvivingMicroorganisms += microbiome->getNumAliveMicroorganisms();
        totalDeadMicroorganisms += microbiome->getNumDeadMicroorganisms();
        totalEnergy += microbiome->getTotalEnergy();
        totalTicksElapsed += numTicks;
        microbiome = new Microbiome(i, "microbiome " + i, config.getEnvironmentSize(), config.getEntityFactor());
    }
    if (numSimulations > 1) {
        std::cout << "" << std::endl;
        std::cout << "=== Average Stats ===" << std::endl;
        std::cout << "Average Surviving Microorganisms: " << totalSurvivingMicroorganisms / numSimulations << "\n";
        std::cout << "Average Dead Microorganisms: " << totalDeadMicroorganisms / numSimulations << "\n";
        std::cout << "Average Total Energy: " << totalEnergy / numSimulations << "\n";
        std::cout << "Average Ticks elapsed: " << totalTicksElapsed / numSimulations << std::endl;
        running = true;
    }
    return 0;
}

int main() {
    MicrobiomeApplication microbiomeApplication;
    microbiomeApplication.run();
    return 0;
}