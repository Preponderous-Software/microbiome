#include <vector>
#include "microbiome.h"

using namespace envlibcpp;

Microbiome::Microbiome(int id, std::string name, int size, int entityFactor) : Environment(id, name, size) {
    generateMicroorganisms(size * entityFactor);
    addMicroorganismsToEnvironment();
}

void Microbiome::generateMicroorganisms(int numMicroorganisms) {
    for (int i = 0; i < numMicroorganisms; i++) {
        Microorganism microorganism(i, "test");
        microorganisms.push_back(microorganism);
    }
}

void Microbiome::addMicroorganismsToEnvironment() {
    for (Microorganism& microorganism : microorganisms) {
        addEntity(microorganism);
    }
}

void Microbiome::initiateMicroorganismMovement() {
    for (Microorganism& microorganism : microorganisms) {
        if (microorganism.isDead()) {
            continue;
        }
        Microorganism& retrievedMicroorganism = (Microorganism&) getEntity(microorganism.getId());     
        moveEntityToRandomAdjacentLocation(retrievedMicroorganism.getId());
        retrievedMicroorganism.incrementTimesMoved();
        retrievedMicroorganism.consumeEnergy();
    }
}

void Microbiome::printConsoleRepresentation() {
    // generate line with the width of the environment
    std::cout << "" << std::endl;
    std::string line = "";
    int numDashes = getGrid()->getSize();
    for (int i = 0; i < numDashes * 3; i++) {
        line += "=";
    }
    std::cout << line << std::endl;
    int index = 0;
    for (Location& location : getGrid()->getLocations()) {
        index++;
        std::string toPrint = " ";
        if (location.getNumEntities() > 0) {
            toPrint = "O";
        }
        // Microorganism& microorganism = (Microorganism&) location.getEntities()[location.getNumEntities() - 1];
        // if (microorganism.isDead()) {
        //     toPrint = "X";
        // }
        std::cout << " " << toPrint << " ";
        if (index == getGrid()->getSize()) {
            std::cout << "\n";
            index = 0;
        }
    }
    std::cout << line << std::endl;
    std::cout << std::endl;
}

std::vector<Microorganism> Microbiome::getMicroorganisms() {
    return microorganisms;
}

int Microbiome::getNumAliveMicroorganisms() {
    int numAlive = 0;
    for (Microorganism& microorganism : microorganisms) {
        if (!microorganism.isDead()) {
            numAlive++;
        }
    }
    return numAlive;
}

int Microbiome::getNumDeadMicroorganisms() {
    int numDead = 0;
    for (Microorganism& microorganism : microorganisms) {
        if (microorganism.isDead()) {
            numDead++;
        }
    }
    return numDead;
}

int Microbiome::getTotalEnergy() {
    int totalEnergy = 0;
    for (Microorganism& microorganism : microorganisms) {
        totalEnergy += microorganism.getEnergy();
    }
    if (totalEnergy < 0) {
        totalEnergy = 0;
    }
    return totalEnergy;
}