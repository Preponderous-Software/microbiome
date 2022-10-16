#include "mbapp.h"

using namespace envlibcpp;

MicrobiomeApplication::MicrobiomeApplication() {
    microbiome = new Microbiome(0, "microbiome", environmentSize, entityFactor);
}

bool MicrobiomeApplication::run() {;
    int numTicks = 0;
    bool running = true;
    while (running) {
        system("clear");
        microbiome->initiateMicroorganismMovement();
        microbiome->printConsoleRepresentation();
        
        // print stats
        std::cout << "Name: " << microbiome->getName() << std::endl;
        std::cout << "Size: " << microbiome->getGrid()->getSize() << "x" << microbiome->getGrid()->getSize() << std::endl;
        std::cout << "Microorganisms: " << microbiome->getNumEntities() << std::endl;
        std::cout << "" << std::endl;
        std::cout << "Ticks Left: " << maxTicks - numTicks << std::endl;

        numTicks++;
        if (numTicks == maxTicks) {
            running = false;
        }
        sleep(tickLengthInSeconds);
    }
    std::cout << "Locations: " << microbiome->getGrid()->getLocations().size() << "\n";
    std::cout << "Microorganisms: " << microbiome->getNumEntities() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
    return 0;
}

int main() {
    MicrobiomeApplication microbiomeApplication;
    microbiomeApplication.run();
    return 0;
}