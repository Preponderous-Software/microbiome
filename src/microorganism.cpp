#include "header/microorganism.h"

using namespace envlibcpp;

Microorganism::Microorganism(int id, std::string name) : Entity(id, name) {
    // empty
}

int Microorganism::getEnergy() {
    return energy;
}

int Microorganism::getEnergyConsumptionRate() {
    return energyConsumptionRate;
}

void Microorganism::setEnergy(int newEnergy) {
    energy = newEnergy;
}

void Microorganism::setEnergyConsumptionRate(int newRate) {
    energyConsumptionRate = newRate;
}

void Microorganism::consumeEnergy() {
    energy -= energyConsumptionRate;
}

int Microorganism::getTimesMoved() {
    return timesMoved;
}

void Microorganism::incrementTimesMoved() {
    timesMoved++;
}

bool Microorganism::isDead() {
    return energy <= 0;
}