#ifndef Microbiome_Simulation_h
#define Microbiome_Simulation_h

#include "microbiome.h"
#include "appConfig.h"
#include "logger.h"

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
        size_t getSurvivingMicroorganisms();
        size_t getDeadMicroorganisms();
        size_t getEnergy();
        size_t getTicksElapsed();
        void printRunningStats();
        void printFinishedStats();
    private:
        bool running = true;
        size_t numTicks = 0;
        Microbiome* microbiome;
        AppConfig* config;
        Logger* logger;
};

#endif