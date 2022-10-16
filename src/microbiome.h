#ifndef Microbiome_h
#define Microbiome_h

#include "../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"

using namespace envlibcpp;

class Microbiome : public Environment {
    public:
        Microbiome(int id, std::string name, int size, int entityFactor);
        void initiateMicroorganismMovement();
        void printConsoleRepresentation();
        std::vector<Microorganism> getMicroorganisms();
        int getNumAliveMicroorganisms();
        int getNumDeadMicroorganisms();
        int getTotalEnergy();
    private:
        void generateMicroorganisms(int numMicroorganisms);
        void addMicroorganismsToEnvironment();
        
        std::vector<Microorganism> microorganisms;
};

#endif