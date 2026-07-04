#include <algorithm>
#include <memory>
#include <vector>

#include "header/microbiome.h"

Microbiome::Microbiome(int id, std::string name, int size, int entityFactor) : Environment(id, name, size) {
    logger = new Logger("log.microbiome.txt");
    generateMicroorganisms(size * entityFactor);
    addMicroorganismsToEnvironment();
    nextEntityId = size * entityFactor;
}

Microbiome::~Microbiome() {
    delete logger;
}

void Microbiome::generateMicroorganisms(int numMicroorganisms) {
    logger->log("Generating " + std::to_string(numMicroorganisms) + " microorganisms...");
    for (int i = 0; i < numMicroorganisms; i++) {
        microorganisms.push_back(std::make_unique<Microorganism>(microorganismFactory.createMicroorganism()));
    }
}

void Microbiome::addMicroorganismsToEnvironment() {
    logger->log("Adding microorganisms to environment...");
    for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
        addEntity(*microorganism);
    }
}

void Microbiome::initiateMicroorganismMovement() {
    logger->log("Initiating microorganism movement...");
    for (size_t i = 0; i < microorganisms.size(); i++) {
        Microorganism& microorganism = *microorganisms[i];
        if (microorganism.isDead()) {
            continue;
        }

        moveMicroorganism(microorganism);
        microorganism.incrementTimesMoved();
        microorganism.metabolize();

        if (microorganism.isDead()) {
            processDeath(microorganism);
            continue;
        }

        if (foragingEnabled) {
            initiateForaging(microorganism);
        }

        if (reproductionEnabled) {
            initiateReproduction(microorganism);
        }
    }

    purgeMicroorganismsNotInEnvironment();
    purgeDepletedBiomatter();
}

std::vector<Location*> Microbiome::getAdjacentLocations(Location& location) {
    std::vector<Location*> adjacentLocations;
    int x = location.getX();
    int y = location.getY();
    int offsets[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (auto& offset : offsets) {
        try {
            Location& adjacent = getGrid()->getLocationByCoordinates(x + offset[0], y + offset[1]);
            adjacentLocations.push_back(&adjacent);
        } catch (const std::runtime_error* e) {
            // no location at these coordinates; edge of the grid
        }
    }
    return adjacentLocations;
}

Microorganism* Microbiome::getMicroorganismAtLocation(Location& location) {
    for (Entity* entity : location.getEntities()) {
        for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
            if (microorganism->getId() == entity->getId()) {
                return microorganism.get();
            }
        }
    }
    return nullptr;
}

Biomatter* Microbiome::getBiomatterAtLocation(Location& location) {
    for (Entity* entity : location.getEntities()) {
        for (std::unique_ptr<Biomatter>& b : biomatter) {
            if (b->getId() == entity->getId()) {
                return b.get();
            }
        }
    }
    return nullptr;
}

void Microbiome::moveMicroorganism(Microorganism& microorganism) {
    Location& currentLocation = getGrid()->getLocation(microorganism.getLocationId());
    std::vector<Location*> adjacentLocations = getAdjacentLocations(currentLocation);
    if (adjacentLocations.empty()) {
        return;
    }

    // chemotaxis: bias movement toward nutrient-rich neighboring locations,
    // similar to how real bacteria bias a run-and-tumble walk up a nutrient
    // gradient, rather than moving with pure uniform randomness.
    std::vector<Location*> nutrientRichLocations;
    if (chemotaxisEnabled) {
        for (Location* candidate : adjacentLocations) {
            if (getBiomatterAtLocation(*candidate) != nullptr) {
                nutrientRichLocations.push_back(candidate);
            }
        }
    }

    Location* destination;
    if (!nutrientRichLocations.empty() && (rand() % 100) < chemotaxisBiasPercent) {
        destination = nutrientRichLocations[rand() % nutrientRichLocations.size()];
    }
    else {
        destination = adjacentLocations[rand() % adjacentLocations.size()];
    }

    moveEntityToNewLocation(microorganism.getId(), destination->getId());
}

void Microbiome::initiateForaging(Microorganism& microorganism) {
    Location& location = getGrid()->getLocation(microorganism.getLocationId());
    Biomatter* foundBiomatter = getBiomatterAtLocation(location);
    if (foundBiomatter == nullptr) {
        return;
    }

    int amountForaged = std::min(foundBiomatter->getEnergy(), microorganism.getMetabolicRate() * biomatterForagingMultiplier);
    if (amountForaged <= 0) {
        return;
    }

    foundBiomatter->setEnergy(foundBiomatter->getEnergy() - amountForaged);
    microorganism.setEnergy(microorganism.getEnergy() + amountForaged);
    microorganism.incrementTimesEaten();
}

void Microbiome::initiateReproduction(Microorganism& microorganism) {
    // binary fission: once an organism has accumulated enough energy, it
    // splits into two daughter cells that share the parent's remaining
    // energy (minus the energetic cost of dividing).
    if (microorganism.getEnergy() < reproductionEnergyThreshold) {
        return;
    }

    Location& currentLocation = getGrid()->getLocation(microorganism.getLocationId());
    std::vector<Location*> adjacentLocations = getAdjacentLocations(currentLocation);
    if (adjacentLocations.empty()) {
        return;
    }

    int energyPerDaughter = (microorganism.getEnergy() - fissionCost) / 2;
    if (energyPerDaughter <= 0) {
        return;
    }

    microorganism.setEnergy(energyPerDaughter);

    std::unique_ptr<Microorganism> offspring = std::make_unique<Microorganism>(nextEntityId++, "microorganism");
    offspring->setEnergy(energyPerDaughter);
    offspring->setMetabolicRate(microorganism.getMetabolicRate());

    Location* destination = adjacentLocations[rand() % adjacentLocations.size()];
    addEntityToLocation(*offspring, *destination);
    microorganisms.push_back(std::move(offspring));
}

void Microbiome::processDeath(Microorganism& microorganism) {
    // decomposition: a dead microorganism's structural biomass becomes
    // biomatter that other, living microorganisms can forage for energy,
    // instead of leaving an inert corpse on the grid indefinitely.
    Location& location = getGrid()->getLocation(microorganism.getLocationId());
    removeEntity(microorganism);
    totalDeaths++;

    std::unique_ptr<Biomatter> newBiomatter = std::make_unique<Biomatter>(nextEntityId++, "biomatter", biomatterYieldPerDeath);
    addEntityToLocation(*newBiomatter, location);
    biomatter.push_back(std::move(newBiomatter));
}

void Microbiome::purgeDepletedBiomatter() {
    for (std::unique_ptr<Biomatter>& b : biomatter) {
        if (b->isDepleted() && isEntityPresent(*b)) {
            removeEntity(*b);
        }
    }

    biomatter.erase(
        std::remove_if(biomatter.begin(), biomatter.end(),
            [](const std::unique_ptr<Biomatter>& b) {
                return b->isDepleted();
            }),
        biomatter.end()
    );
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

        Microorganism* organism = getMicroorganismAtLocation(location);
        if (organism != nullptr) {
            toPrint = aliveMicroorganismRepresentation;

            // print + if microorganism has foraged
            if (organism->getTimesEaten() > 0) {
                toPrint = subsistingMicroorganismRepresentation;
            }

            // print ! if microorganism is low on energy
            if (organism->getEnergy() < 10) {
                toPrint = dyingMicroorganismRepresentation;
            }
        }
        else if (getBiomatterAtLocation(location) != nullptr) {
            toPrint = biomatterRepresentation;
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
    logger->log("Removing entity " + std::to_string(entity.getId()) + " from environment...");
    Environment::removeEntity(entity);
}

std::vector<Microorganism*> Microbiome::getMicroorganisms() {
    std::vector<Microorganism*> result;
    for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
        result.push_back(microorganism.get());
    }
    return result;
}

int Microbiome::getNumAliveMicroorganisms() {
    int numAlive = 0;
    for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
        if (!microorganism->isDead()) {
            numAlive++;
        }
    }
    return numAlive;
}

int Microbiome::getNumDeadMicroorganisms() {
    return static_cast<int>(totalDeaths);
}

int Microbiome::getTotalEnergy() {
    int totalEnergy = 0;
    for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
        totalEnergy += microorganism->getEnergy();
    }
    for (std::unique_ptr<Biomatter>& b : biomatter) {
        totalEnergy += b->getEnergy();
    }
    if (totalEnergy < 0) {
        totalEnergy = 0;
    }
    return totalEnergy;
}

bool Microbiome::isMicroorganismPresent(int id) {
    for (std::unique_ptr<Microorganism>& microorganism : microorganisms) {
        if (microorganism->getId() == id) {
            return true;
        }
    }
    return false;
}

void Microbiome::addMicroorganism(Microorganism microorganism) {
    logger->log("Adding microorganism " + std::to_string(microorganism.getId()) + "...");
    microorganisms.push_back(std::make_unique<Microorganism>(microorganism));
}

void Microbiome::removeMicroorganism(int id) {
    logger->log("Removing microorganism " + std::to_string(id) + "...");
    microorganisms.erase(
        std::remove_if(microorganisms.begin(), microorganisms.end(),
            [id](const std::unique_ptr<Microorganism>& microorganism) {
                return microorganism->getId() == id;
            }),
        microorganisms.end()
    );
}

void Microbiome::purgeMicroorganismsNotInEnvironment() {
    microorganisms.erase(
        std::remove_if(microorganisms.begin(), microorganisms.end(),
            [this](const std::unique_ptr<Microorganism>& microorganism) {
                return !isEntityPresent(*microorganism);
            }),
        microorganisms.end()
    );
}
