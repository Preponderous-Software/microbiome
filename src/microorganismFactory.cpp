#include "header/microorganismFactory.h"

MicroorganismFactory::MicroorganismFactory() {
    entityCounter = 0;
}

Microorganism MicroorganismFactory::createMicroorganism() {
    Microorganism microorganism(entityCounter, "test name");
    entityCounter++;
    return microorganism;
}