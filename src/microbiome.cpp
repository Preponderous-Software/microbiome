#include <vector>
#include <iomanip>
#include <set>
#include "header/microbiome.h"

Microbiome::Microbiome(int id, std::string name, int size, int entityFactor) : Environment(id, name, size) {
    logger = new Logger("log.microbiome.txt");
    logger->log("Creating Microbiome");
    generateMicroorganisms(size * entityFactor);
}

Microbiome::~Microbiome() {
    logger->log("Destroying Microbiome '" + getName() + "'");
    delete logger;
}

void Microbiome::generateMicroorganisms(size_t numMicroorganisms) {
    for (size_t i = 0; i < numMicroorganisms; i++) {
        Microorganism microorganism = microorganismFactory.createMicroorganism();
        microorganisms.push_back(microorganism);
    }
    for (size_t i = 0; i < microorganisms.size(); i++) {
        addEntity(microorganisms[i]);
    }
}

void Microbiome::initiateMicroorganismMovement() {
    logger->log("Initiating microorganism movement");
    for (size_t i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = microorganisms[i];
        logger->log("Moving microorganism " + std::to_string(microorganism.getId()));

        // check if microorganism is alive
        if (microorganism.isDead()) {
            logger->log("Microorganism " + std::to_string(microorganism.getId()) + " is dead");
            continue;
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

        microorganism.metabolize();

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
    for (size_t i = 0; i < entities.size(); i++) {
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
            int metabolicConsumptionFactor = microorganism.getMetabolicConsumptionFactor();
            microorganism.setEnergy(energy + metabolicRate * metabolicConsumptionFactor);
            break;
        }
    }

    // remove dead microorganism
    if (toRemove != nullptr) {
        Microorganism& microorganismToRemove = (Microorganism&) *toRemove;
        removeMicroorganism(microorganismToRemove.getId());
    }
}

void Microbiome::printConsoleRepresentation() {
    // generate line with the width of the environment
    std::cout << "" << std::endl;
    std::string line = "";
    size_t numDashes = getGrid()->getSize();
    for (size_t i = 0; i < numDashes * 3; i++) {
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

void Microbiome::addMicroorganism(Microorganism microorganism) {
    logger->log("Adding microorganism " + std::to_string(microorganism.getId()));
    logger->log("entity ids before adding microorganism: " + getListOfEntityIds());

    microorganisms.push_back(microorganism);
    logger->log("entity ids after pushing back but not adding to environment: " + getListOfEntityIds());
    
    Environment::addEntity(microorganism);
    logger->log("entity ids after adding microorganism: " + getListOfEntityIds());
}

void Microbiome::removeMicroorganism(int id) {
    // retrieve microorganism
    Microorganism& microorganism = getMicroorganismById(id);

    logger->log("Removing microorganism " + std::to_string(id) + " from environment");
    removeEntity(microorganism);

    // re-create microorganisms vector without removed microorganism
    logger->log("Re-creating microorganisms vector without removed microorganism");
    logger->log("Number of microorganisms before re-creation: " + std::to_string(microorganisms.size()));
    std::vector<Microorganism> newMicroorganisms;
    for (Microorganism& currentMicroorganism : microorganisms) {
        if (currentMicroorganism.getId() != microorganism.getId()) {
            newMicroorganisms.push_back(currentMicroorganism);
        }
    }
    microorganisms = newMicroorganisms;
    logger->log("Number of microorganisms after re-creation: " + std::to_string(microorganisms.size()));
}

std::vector<Microorganism>& Microbiome::getMicroorganisms() {
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

Microorganism& Microbiome::getMicroorganismById(int id) {
    logger->log("Getting microorganism with id " + std::to_string(id));
    for (Location& location : getGrid()->getLocations()) {
        std::vector<Entity*> entities = location.getEntities();
        for (Entity* entity : entities) {
            Microorganism& microorganism = (Microorganism&) *entity;
            if (microorganism.getId() == id) {
                logger->log("Found microorganism with id " + std::to_string(id));
                return microorganism;
            }
        }
    }
    logger->log("ERROR: Could not find microorganism with id " + std::to_string(id));
    throw new std::runtime_error("Microorganism with id " + std::to_string(id) + " not found");
}

bool Microbiome::isMicroorganismPresent(int id) {
    logger->log("Checking if microorganism with id " + std::to_string(id) + " is present");
    try {
        getMicroorganismById(id);
        return true;
    }
    catch(const std::runtime_error* e) {
        return false;
    }
}

void Microbiome::initiateMicroorganismReproduction() {
    logger->log("Initiating microorganism reproduction");
    std::vector<Microorganism> microorganismsToAdd;
    for (size_t i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = microorganisms[i];
        if (microorganism.isDead()) {
            continue;
        }
        
        // check if microorganism has enough energy to reproduce
        if (!microorganism.canReproduce()) {
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

std::string Microbiome::getListOfEntityIds() {
    std::string toReturn = "";
    for (Location& location : getGrid()->getLocations()) {
        for (Entity* entity : location.getEntities()) {
            toReturn += "" + std::to_string(entity->getId()) + ", ";
        }
    }
    return toReturn.substr(0, toReturn.size() - 2);
}

void Microbiome::checkForEntityIdMismatch() {
    // get microorganism ids
    std::vector<int> microorganismIds;
    for (Microorganism& microorganism : microorganisms) {
        microorganismIds.push_back(microorganism.getId());
    }

    // get entities from all locations
    std::vector<Entity*> entities;
    for (Location& location : getGrid()->getLocations()) {
        for (Entity* entity : location.getEntities()) {
            entities.push_back(entity);
        }
    }

    // create sets for entity and microorganism ids
    std::set<int> entityIds;
    std::set<int> microorganismIdsSet;
    for (Entity* entity : entities) {
        entityIds.insert(entity->getId());
    }
    for (Microorganism& microorganism : microorganisms) {
        microorganismIdsSet.insert(microorganism.getId());
    }
    bool entityIdsEqualsMicroorganismIds = entityIds == microorganismIdsSet;
    if (!entityIdsEqualsMicroorganismIds) {
        logger->log("ERROR: Entity ids and microorganism ids are not equal");
        logger->log("entity ids: " + getListOfEntityIds());
        std::string listOfMicroorganismIds = "";
        for (int id : microorganismIds) {
            listOfMicroorganismIds += std::to_string(id) + ", ";
        }
        logger->log("microorganism ids: " + listOfMicroorganismIds);

        // get difference
        std::vector<int> difference;
        for (int id : microorganismIds) {
            if (entityIds.find(id) == entityIds.end()) {
                difference.push_back(id);
            }
        }
        std::string listOfDifference = "";
        for (int id : difference) {
            listOfDifference += std::to_string(id) + ", ";
        }
        logger->log("difference: " + listOfDifference);
        throw new std::runtime_error("Entity ids not equal microorganism ids");
    }
}

void Microbiome::checkForAmountMismatchBetweenEntitiesAndMicroorganisms() {
    // get microorganism ids
    std::vector<int> microorganismIds;
    for (Microorganism& microorganism : microorganisms) {
        microorganismIds.push_back(microorganism.getId());
    }

    // get entities from all locations
    std::vector<Entity*> entities;
    for (Location& location : getGrid()->getLocations()) {
        for (Entity* entity : location.getEntities()) {
            entities.push_back(entity);
        }
    }

    // check if amount of entities equals amount of microorganisms
    if (entities.size() != microorganisms.size()) {
        logger->log("ERROR: Amount of entities does not equal amount of microorganisms");
        logger->log("entity ids: " + getListOfEntityIds());
        std::string listOfMicroorganismIds = "";
        for (int id : microorganismIds) {
            listOfMicroorganismIds += std::to_string(id) + ", ";
        }
        logger->log("microorganism ids: " + listOfMicroorganismIds);
        throw new std::runtime_error("Amount of entities does not equal amount of microorganisms");
    }
}