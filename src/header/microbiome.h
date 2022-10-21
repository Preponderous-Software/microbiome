#ifndef Microbiome_h
#define Microbiome_h

#include "../../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"
#include "logger.h"
#include "microorganismFactory.h"

using namespace envlibcpp;

/**
 * The Microbiome class represents a 2D virtual environment populated with microorganisms.
 * @author Daniel McCoy Stephenson
 */
class Microbiome : public Environment {
    public:
        Microbiome(int id, std::string name, int size, int entityFactor);
        ~Microbiome();
        void addMicroorganism(Microorganism& microorganism);
        void removeMicroorganism(Microorganism& microorganism);
        std::vector<Microorganism>& getMicroorganisms();
        void initiateMicroorganismMovement();
        void printConsoleRepresentation();
        int getNumAliveMicroorganisms();
        int getNumDeadMicroorganisms();
        int getTotalEnergy();
        void purgeMicroorganismsNotInEnvironment();
        Microorganism& getMicroorganismById(int id);
        bool isMicroorganismPresent(int id);
        void initiateMicroorganismReproduction();
        Microorganism createOffspring(Microorganism& parent);
        std::string getListOfEntityIds();
    private:
        void generateMicroorganisms(size_t numMicroorganisms);
        void addMicroorganismsToEnvironment();
        void initiateConsumptionOfDeadMicroorganisms(Microorganism& microorganism);
        void checkForEntityIdMismatch();
        
        std::vector<Microorganism> microorganisms;
        Logger* logger = nullptr;
        MicroorganismFactory microorganismFactory;
        
        // microbiome config options
        std::string aliveMicroorganismRepresentation = "o";
        std::string subsistingMicroorganismRepresentation = "+";
        std::string dyingMicroorganismRepresentation = "!";
        std::string deadMicroorganismRepresentation = ".";
        bool deadMicroorganismConsumptionEnabled = true;
};

#endif