#include "mbapp.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace envlibcpp;

MicrobiomeApplication::MicrobiomeApplication() {
    microbiome = new Microbiome(0, "microbiome", environmentSize, entityFactor);
}

void MicrobiomeApplication::printRunningStats() {
        std::cout << "Name: " << microbiome->getName() << std::endl;
        std::cout << "Size: " << microbiome->getGrid()->getSize() << "x" << microbiome->getGrid()->getSize() << std::endl;
        std::cout << "Microorganisms: " << microbiome->getNumAliveMicroorganisms() << std::endl;
        std::cout << "Dead Microorganisms: " << microbiome->getNumDeadMicroorganisms() << "\n";
        std::cout << "Total Energy: " << microbiome->getTotalEnergy() << "\n";
        std::cout << "" << std::endl;
        std::cout << "Ticks Left: " << maxTicks - numTicks << std::endl;
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
    while (running) {
        microbiome->initiateMicroorganismMovement();

        system("clear");
        microbiome->printConsoleRepresentation();
        printRunningStats();

        numTicks++;
        if (numTicks == maxTicks) {
            running = false;
        }
        sleep(tickLengthInSeconds);

        if (microbiome->getNumAliveMicroorganisms() == 0) {
            running = false;
        }
    }
    printFinishedStats();
    return 0;
}

int main() {
    MicrobiomeApplication microbiomeApplication;
    microbiomeApplication.run();
    return 0;
}