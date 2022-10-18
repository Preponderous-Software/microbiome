#ifndef Microbiome_Simulation_h
#define Microbiome_Simulation_h

#include "microbiome.h"
#include "config.h"

class Simulation {
    public:
        Simulation() = delete;
        Simulation(Config* config, int id, std::string name);
        ~Simulation();
        void run();
        void runWithNoOutput();
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
        Config* config;
};

#endif