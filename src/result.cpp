#include "header/result.h"

Result::Result(Simulation* simulation) {
    survivingMicroorganisms = simulation->getSurvivingMicroorganisms();
    deadMicroorganisms = simulation->getDeadMicroorganisms();
    energy = simulation->getEnergy();
    ticksElapsed = simulation->getTicksElapsed();
}

size_t Result::getSurvivingMicroorganisms() {
    return survivingMicroorganisms;
}

size_t Result::getDeadMicroorganisms() {
    return deadMicroorganisms;
}

size_t Result::getEnergy() {
    return energy;
}

size_t Result::getTicksElapsed() {
    return ticksElapsed;
}

void Result::print() {
    std::cout << "=== Simulation Result ===" << std::endl;
    std::cout << "Surviving Microorganisms: " << survivingMicroorganisms << std::endl;
    std::cout << "Dead Microorganisms: " << deadMicroorganisms << std::endl;
    std::cout << "Total Energy: " << energy << std::endl;
    std::cout << "Ticks elapsed: " << ticksElapsed << std::endl;
}