#include "microbiome.h"

using namespace envlibcpp;

Microbiome::Microbiome() {
    environment = new Environment(0, "microbiome", 5);
}

void Microbiome::generateMicroorganisms(int numMicroorganisms) {
    for (int i = 0; i < numMicroorganisms; i++) {
        Microorganism microorganism(i, "test");
        microorganisms.push_back(microorganism);
    }
}

void Microbiome::addMicroorganismsToEnvironment() {
    for (Microorganism& microorganism : microorganisms) {
        environment->addEntity(microorganism);
    }
}

void Microbiome::initiateMicroorganismMovement() {
    for (Microorganism& microorganism : microorganisms) {
        Microorganism& retrievedMicroorganism = (Microorganism&) environment->getEntity(microorganism.getId());     
        environment->moveEntityToRandomAdjacentLocation(retrievedMicroorganism.getId());
        retrievedMicroorganism.incrementTimesMoved();
    }
}

bool Microbiome::run() {;
    generateMicroorganisms(10);
    addMicroorganismsToEnvironment();
    int numTicks = 0;
    bool running = true;
    while (running) {
        initiateMicroorganismMovement();
        environment->printConsoleRepresentation();
        numTicks++;
        if (numTicks == 100) {
            running = false;
        }
        sleep(1);
    }
    std::cout << "Locations: " << environment->getGrid()->getLocations().size() << "\n";
    std::cout << "Microorganisms: " << environment->getNumEntities() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
    return 0;
}

int main() {
    Microbiome microbiome;
    microbiome.run();
    return 0;
}