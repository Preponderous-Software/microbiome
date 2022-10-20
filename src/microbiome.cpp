#include <vector>
#include "header/microbiome.h"

Microbiome::Microbiome(int id, std::string name, int size, int entityFactor) : Environment(id, name, size) {
    logger = new Logger("log.microbiome.txt");
    logger->log("Creating Microbiome");
    generateMicroorganisms(size * entityFactor);
    addMicroorganismsToEnvironment();
}

Microbiome::~Microbiome() {
    logger->log("Destroying Microbiome '" + getName() + "'");
    delete logger;
}

void Microbiome::generateMicroorganisms(int numMicroorganisms) {
    for (int i = 0; i < numMicroorganisms; i++) {
        logger->log("Generating microorganism " + std::to_string(i));
        Microorganism microorganism = microorganismFactory.createMicroorganism();
        logger->log("Adding microorganism " + std::to_string(i) + " to microorganisms vector");
        microorganisms.push_back(microorganism);
    }
}

void Microbiome::addMicroorganismsToEnvironment() {
    for (Microorganism& microorganism : microorganisms) {
        logger->log("Adding microorganism " + std::to_string(microorganism.getId()) + " to environment");
        addEntity(microorganism);
    }
}

void Microbiome::initiateMicroorganismMovement() {
    logger->log("Initiating microorganism movement");
    for (int i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = microorganisms[i];
        logger->log("Moving microorganism " + std::to_string(microorganism.getId()));

        // check if microorganism is alive
        if (microorganism.isDead()) {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is dead");
            continue;
        }
        else {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is alive");
        }

        // check if microorganism is in environment
        if (isMicroorganismPresent(microorganism.getId())) {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is in environment");
        }
        else {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is not in environment");
            continue;
        }

        logger->log("Invoking env-lib-cpp moveEntityToRandomAdjacentLocation() method");
        moveEntityToRandomAdjacentLocation(microorganism.getId());
        logger->log("env-lib-cpp moveEntityToRandomAdjacentLocation() method invoked");
        microorganism.incrementTimesMoved();
        logger->log("Microorganism " + std::to_string(microorganism.getId()) + " has moved " + std::to_string(microorganism.getTimesMoved()) + " times");

        microorganism.metabolize();
        logger->log("Microorganism " + std::to_string(microorganism.getId()) + " has " + std::to_string(microorganism.getEnergy()) + " energy");

        if (deadMicroorganismConsumptionEnabled) {
            initiateConsumptionOfDeadMicroorganisms(microorganism);
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

void Microbiome::addMicroorganism(Microorganism& microorganism) {
    logger->log("Adding microorganism " + std::to_string(microorganism.getId()));
    Environment::addEntity(microorganism);
    microorganisms.push_back(microorganism);
}

void Microbiome::removeMicroorganism(Microorganism& microorganism) {
    logger->log("Removing microorganism " + std::to_string(microorganism.getId()));
    Environment::removeEntity(microorganism);

    // // remove from microorganisms vector
    // for (int i = 0; i < microorganisms.size(); i++) { // TODO: fix
    //     Microorganism& retrievedMicroorganism = microorganisms[i];
    //     if (retrievedMicroorganism.getId() == microorganism.getId()) {
    //         microorganisms.erase(microorganisms.begin() + i);
    //         break;
    //     }
    // }
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

Microorganism& Microbiome::getMicroorganismById(int id) {
    for (Location& location : getGrid()->getLocations()) {
        std::vector<Entity*> entities = location.getEntities();
        for (Entity* entity : entities) {
            Microorganism& microorganism = (Microorganism&) *entity;
            if (microorganism.getId() == id) {
                return microorganism;
            }
        }
    }
    throw std::invalid_argument("Microorganism with id " + std::to_string(id) + " not found");
}

bool Microbiome::isMicroorganismPresent(int id) {
    try {
        getMicroorganismById(id);
        return true;
    }
    catch (std::invalid_argument& e) {
        return false;
    }
}

void Microbiome::initiateMicroorganismReproduction() {
    logger->log("Initiating microorganism reproduction");
    std::vector<Microorganism> microorganismsToAdd;
    for (int i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = microorganisms[i];
        logger->log("Checking microorganism " + std::to_string(microorganism.getId()));
        if (microorganism.isDead()) {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is dead");
            continue;
        }
        
        // check if microorganism has enough energy to reproduce
        if (!microorganism.canReproduce()) {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " does not have enough energy to reproduce");
            continue;
        }

        // intitiate reproduction
        logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is initiating reproduction");
        Microorganism child = createOffspring(microorganism);
        logger->log("Microorganism " + std::to_string(microorganism.getId()) + " spawned offspring " + std::to_string(child.getId()));
        microorganismsToAdd.push_back(child);
    }
    
    // add children
    for (Microorganism& child : microorganismsToAdd) {
        logger->log("Adding child microorganism " + std::to_string(child.getId()));
        addMicroorganism(child); // TODO: add microorganism to same location as parent
    }
}

Microorganism Microbiome::createOffspring(Microorganism& parent) {
    Microorganism child = microorganismFactory.createMicroorganism();
    child.setEnergy(parent.getEnergy() / 2);
    child.setMetabolicRate(parent.getMetabolicRate());
    child.setReproductionThreshold(parent.getReproductionThreshold());
    parent.setEnergy(parent.getEnergy() / 2);
    return child;
}