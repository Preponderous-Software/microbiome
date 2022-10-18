#ifndef Microbiome_Application_h
#define Microbiome_Application_h

#include "config.h"
#include "simulation.h"
#include "result.h"

class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        bool run();
    private:
        Config config;
        Simulation* simulation;
        std::vector<Result> results;
        void printResults();
        void printResultAverages();
};

#endif