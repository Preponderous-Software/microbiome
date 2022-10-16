#include "mbapp.h"

using namespace envlibcpp;

MicrobiomeApplication::MicrobiomeApplication() {
    microbiome = new Microbiome(0, "microbiome", 5);
}

bool MicrobiomeApplication::run() {;
    int numTicks = 0;
    bool running = true;
    while (running) {
        microbiome->initiateMicroorganismMovement();
        microbiome->printConsoleRepresentation();
        numTicks++;
        if (numTicks == 100) {
            running = false;
        }
        sleep(1);
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