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

void Result::print(std::ostream& out) {
    out << "=== Simulation Result ===" << std::endl;
    out << "Surviving Microorganisms: " << survivingMicroorganisms << std::endl;
    out << "Dead Microorganisms: " << deadMicroorganisms << std::endl;
    out << "Total Energy: " << energy << std::endl;
    out << "Ticks elapsed: " << ticksElapsed << std::endl;
}