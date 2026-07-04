#ifndef Microbiome_h
#define Microbiome_h

#include <memory>
#include <vector>

#include "../../env-lib-cpp/src/header/environment.h"

#include "microorganism.h"
#include "microorganismFactory.h"
#include "biomatter.h"
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
        std::vector<Microorganism*> getMicroorganisms();
        std::vector<Biomatter*> getBiomatter();
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
        void moveMicroorganism(Microorganism& microorganism);
        void initiateForaging(Microorganism& microorganism);
        void initiateReproduction(Microorganism& microorganism);
        void processDeath(Microorganism& microorganism);
        void purgeDepletedBiomatter();
        std::vector<Location*> getAdjacentLocations(Location& location);
        Microorganism* getMicroorganismAtLocation(Location& location);
        Biomatter* getBiomatterAtLocation(Location& location);

        std::vector<std::unique_ptr<Microorganism>> microorganisms;
        std::vector<std::unique_ptr<Biomatter>> biomatter;
        MicroorganismFactory microorganismFactory;
        Logger* logger;
        int nextEntityId = 0;
        size_t totalDeaths = 0;

        // microbiome config options
        std::string aliveMicroorganismRepresentation = "o";
        std::string subsistingMicroorganismRepresentation = "+";
        std::string dyingMicroorganismRepresentation = "!";
        std::string biomatterRepresentation = ".";
        bool foragingEnabled = true;
        bool reproductionEnabled = true;
        bool chemotaxisEnabled = true;

        // biology tuning constants
        int reproductionEnergyThreshold = 900;
        int fissionCost = 100;
        int biomatterYieldPerDeath = 60;
        int biomatterForagingMultiplier = 1;
        int chemotaxisBiasPercent = 80;
};

#endif
