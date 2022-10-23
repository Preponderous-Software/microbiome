#ifndef Microbiome_h
#define Microbiome_h

#include "../../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"
#include "microorganismFactory.h"
#include "logger.h"

using namespace envlibcpp;

/**
 * The Microbiome class represents a 2D virtual environment populated with microorganisms.
 * @author Daniel McCoy Stephenson
 */
class Microbiome : public Environment {
    public:
        Microbiome(int id, std::string name, int size, int entityFactor);
        ~Microbiome();
        void initiateMicroorganismMovement();
        void printConsoleRepresentation();
        void removeEntity(Entity& entity);
        std::vector<Microorganism> getMicroorganisms();
        int getNumAliveMicroorganisms();
        int getNumDeadMicroorganisms();
        int getTotalEnergy();
        void purgeMicroorganismsNotInEnvironment();
        bool isMicroorganismPresent(int id); 
        void addMicroorganism(Microorganism microorganism);
        void removeMicroorganism(int id);
    private:
        void generateMicroorganisms(int numMicroorganisms);
        void addMicroorganismsToEnvironment();
        void initiateConsumptionOfDeadMicroorganisms(Microorganism& microorganism);
        
        std::vector<Microorganism> microorganisms;
        MicroorganismFactory microorganismFactory;
        Logger* logger;
        
        // microbiome config options
        std::string aliveMicroorganismRepresentation = "o";
        std::string subsistingMicroorganismRepresentation = "+";
        std::string dyingMicroorganismRepresentation = "!";
        std::string deadMicroorganismRepresentation = ".";
        bool deadMicroorganismConsumptionEnabled = true;
};

#endif