#ifndef Microbiome_Simulation_h
#define Microbiome_Simulation_h

#include "microbiome.h"
#include "appConfig.h"

/**
 * The Simulation class represents a simulation of a Microbiome.
 * @author Daniel McCoy Stephenson
 */
class Simulation {
    public:
        Simulation() = delete;
        Simulation(AppConfig* config, int id, std::string name);
        ~Simulation();
        void run();
        int getSurvivingMicroorganisms();
        int getDeadMicroorganisms();
        int getEnergy();
        int getTicksElapsed();
        void printRunningStats();
        void printFinishedStats();
    private:
        bool running = true;
        int numTicks = 0;
        Microbiome* microbiome;
        AppConfig* config;
};

#endif