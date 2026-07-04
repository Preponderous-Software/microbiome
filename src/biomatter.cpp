#include "header/biomatter.h"

Biomatter::Biomatter(int id, std::string name, int startingEnergy) : Entity(id, name) {
    energy = startingEnergy;
}

int Biomatter::getEnergy() {
    return energy;
}

void Biomatter::setEnergy(int newEnergy) {
    energy = newEnergy;
}

bool Biomatter::isDepleted() {
    return energy <= 0;
}
