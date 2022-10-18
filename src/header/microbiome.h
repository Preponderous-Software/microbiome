#ifndef Microbiome_h
#define Microbiome_h

#include "../../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"

using namespace envlibcpp;

class Microbiome : public Environment {
    public:
        Microbiome(int id, std::string name, int size, int entityFactor);
        void initiateMicroorganismMovement();
        void printConsoleRepresentation();
        void removeEntity(Entity& entity);
        std::vector<Microorganism> getMicroorganisms();
        int getNumAliveMicroorganisms();
        int getNumDeadMicroorganisms();
        int getTotalEnergy();
    private:
        void generateMicroorganisms(int numMicroorganisms);
        void addMicroorganismsToEnvironment();
        void initiateConsumptionOfDeadMicroorganisms(Microorganism& microorganism);
        
        std::vector<Microorganism> microorganisms;
        std::string aliveMicroorganismRepresentation = "o";
        std::string deadMicroorganismRepresentation = ".";
};

#endif