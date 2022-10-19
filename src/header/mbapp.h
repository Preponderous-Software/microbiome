#ifndef Microbiome_Application_h
#define Microbiome_Application_h

#include "appConfig.h"
#include "simulation.h"
#include "result.h"

/**
 * The MicrobiomeApplication class is the main class for the Microbiome application.
 * @author Daniel McCoy Stephenson
 */
class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        bool run();
    private:
        AppConfig config;
        Simulation* simulation;
        std::vector<Result> results;
        void printResults();
        void printResultAverages();
};

#endif