#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <iostream>

#include "../env-lib-cpp/src/header/entity.h"
#include "../env-lib-cpp/src/header/environment.h"
#include "../env-lib-cpp/src/header/location.h"
#include "../env-lib-cpp/src/header/grid.h"

using namespace envlibcpp;

std::vector<Entity> generateEntities(int numEntities) {
    std::vector<Entity> entities;
    for (int i = 0; i < numEntities; i++) {
        Entity entity(i, "test");
        entities.push_back(entity);
    }
    return entities;
}

void addEntitiesToEnvironment(std::vector<Entity>& entities, Environment& environment) {
    for (Entity& entity : entities) {
        environment.addEntity(entity);
    }
}

void initiateEntityMovement(std::vector<Entity>& entities, Environment& environment) {
    for (Entity& entity : entities) {
        Entity& retrievedEntity = environment.getEntity(entity.getId());     
        environment.moveEntityToRandomAdjacentLocation(retrievedEntity.getId());
    }
}

int main() {
    int environmentSize = 8;
    Environment environment(0, "microbiome", environmentSize);
    std::vector<Entity> entities = generateEntities(environmentSize);
    addEntitiesToEnvironment(entities, environment);
    int numTicks = 0;
    bool running = true;
    while (running) {
        initiateEntityMovement(entities, environment);
        environment.printConsoleRepresentation();
        numTicks++;
        if (numTicks == 100) {
            running = false;
        }
    }
    std::cout << "Locations: " << environment.getGrid()->getLocations().size() << "\n";
    std::cout << "Entities: " << environment.getNumEntities() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
    return 0;
}