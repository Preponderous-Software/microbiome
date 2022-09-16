#include "microbiome.h"

using namespace envlibcpp;

Microbiome::Microbiome() {
    environment = new Environment(0, "microbiome", 5);
}

void Microbiome::generateEntities(int numEntities) {
    for (int i = 0; i < numEntities; i++) {
        Entity entity(i, "test");
        entities.push_back(entity);
    }
}

void Microbiome::addEntitiesToEnvironment() {
    for (Entity& entity : entities) {
        environment->addEntity(entity);
    }
}

void Microbiome::initiateEntityMovement() {
    for (Entity& entity : entities) {
        Entity& retrievedEntity = environment->getEntity(entity.getId());     
        environment->moveEntityToRandomAdjacentLocation(retrievedEntity.getId());
    }
}

bool Microbiome::run() {;
    generateEntities(1);
    addEntitiesToEnvironment();
    int numTicks = 0;
    bool running = true;
    while (running) {
        initiateEntityMovement();
        environment->printConsoleRepresentation();
        numTicks++;
        if (numTicks == 100) {
            running = false;
        }
        sleep(1);
    }
    std::cout << "Locations: " << environment->getGrid()->getLocations().size() << "\n";
    std::cout << "Entities: " << environment->getNumEntities() << "\n";
    std::cout << "Ticks elapsed: " << numTicks << std::endl;
    return 0;
}

int main() {
    Microbiome microbiome;
    microbiome.run();
    return 0;
}