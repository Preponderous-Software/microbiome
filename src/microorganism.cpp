#include "header/microorganism.h"

Microorganism::Microorganism(int id, std::string name) : Entity(id, name) {
    // empty
}

int Microorganism::getEnergy() {
    return energy;
}

int Microorganism::getMetabolicRate() {
    return metabolicRate;
}

void Microorganism::setEnergy(int newEnergy) {
    energy = newEnergy;
}

void Microorganism::setMetabolicRate(int newRate) {
    metabolicRate = newRate;
}

void Microorganism::metabolize() {
    energy -= metabolicRate;
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

int Microorganism::getTimesEaten() {
    return timesEaten;
}

void Microorganism::incrementTimesEaten() {
    timesEaten++;
}