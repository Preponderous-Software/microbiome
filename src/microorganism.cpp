#include "microorganism.h"

using namespace envlibcpp;

Microorganism::Microorganism(int id, std::string name) : Entity(id, name) {
}

int Microorganism::getTimesMoved() {
    return timesMoved;
}

void Microorganism::incrementTimesMoved() {
    timesMoved++;
}