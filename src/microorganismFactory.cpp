#include "header/microorganismFactory.h"

MicroorganismFactory::MicroorganismFactory() {
    this->entityCounter = 0;
}

Microorganism MicroorganismFactory::createMicroorganism() {
    Microorganism microorganism(this->entityCounter++, "test name");
    return microorganism;
}