#include <vector>
#include "header/microbiome.h"

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
    for (int i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = microorganisms[i];
        if (microorganism.isDead()) {
            continue;
        }
        Microorganism& retrievedMicroorganism = (Microorganism&) getEntity(microorganism.getId());     
        moveEntityToRandomAdjacentLocation(retrievedMicroorganism.getId());
        retrievedMicroorganism.incrementTimesMoved();
        retrievedMicroorganism.metabolize();

        if (deadMicroorganismConsumptionEnabled) {
            initiateConsumptionOfDeadMicroorganisms(retrievedMicroorganism);
        }
    }
}

void Microbiome::initiateConsumptionOfDeadMicroorganisms(Microorganism& microorganism ) {
    // get location of microorganism
    Location& microorganismLocation = getGrid()->getLocation(microorganism.getLocationId());

    // get entities at location
    std::vector<Entity*> entities = microorganismLocation.getEntities();

    if (entities.size() == 1) {
        return;
    }

    // iterate through entities to check for the presence of a dead microorganism
    Entity* toRemove = nullptr;
    for (int i = 0; i < entities.size(); i++) {
        Entity* entity = entities[i];
        if (entity->getId() == microorganism.getId()) {
            continue;
        }
        Microorganism& retrievedMicroorganism = (Microorganism&) *entity;
        if (retrievedMicroorganism.isDead()) {
            microorganism.incrementTimesEaten();
            toRemove = entity;

            // increase energy
            int energy = microorganism.getEnergy();
            int metabolicRate = microorganism.getMetabolicRate();
            microorganism.setEnergy(energy + metabolicRate * 2);
            break;
        }
    }

    // remove dead microorganism
    if (toRemove != nullptr) {
        removeEntity(*toRemove);
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
            int entityId = location.getEntities()[location.getEntities().size() - 1]->getId();
            Microorganism& microorganism = (Microorganism&) getEntity(entityId);
            if (microorganism.isDead()) {
                toPrint = deadMicroorganismRepresentation;
            }
            else {
                toPrint = aliveMicroorganismRepresentation;

                // print + if microorganism has eaten
                if (microorganism.getTimesEaten() > 0) {
                    toPrint = subsistingMicroorganismRepresentation;
                }

                // print ! if microorganism is low on energy
                if (microorganism.getEnergy() < 10) {
                    toPrint = dyingMicroorganismRepresentation;
                }
            }
        }

        std::cout << " " << toPrint << " ";
        if (index == getGrid()->getSize()) {
            std::cout << "\n";
            index = 0;
        }
    }
    std::cout << line << std::endl;
    std::cout << std::endl;
}

void Microbiome::removeEntity(Entity& entity) {
    Environment::removeEntity(entity);
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

void Microbiome::purgeMicroorganismsNotInEnvironment() {
    std::vector<Microorganism*> microorganismsToRemove;
    for (Microorganism& microorganism : microorganisms) {
        if (microorganism.getEnvironmentId() == -1) {
            microorganismsToRemove.push_back(&microorganism);
        }
    }
    // use iterator to remove elements
    for (Microorganism* microorganism : microorganismsToRemove) {
        std::vector<Microorganism>::iterator iterator = microorganisms.begin();
        while (iterator != microorganisms.end()) {
            if (iterator->getId() == microorganism->getId()) {
                iterator = microorganisms.erase(iterator);
            }
            else {
                iterator++;
            }
        }
    }
}