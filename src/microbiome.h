#ifndef Microbiome_h
#define Microbiome_h

#include "../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"

using namespace envlibcpp;

class Microbiome : public Environment {
    public:
        Microbiome(int id, std::string name, int size);
        void generateMicroorganisms(int numMicroorganisms);
        void addMicroorganismsToEnvironment();
        void initiateMicroorganismMovement();
        void printConsoleRepresentation();
    private:
        std::vector<Microorganism> microorganisms;
};

#endif