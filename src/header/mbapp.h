#ifndef Microbiome_Application_h
#define Microbiome_Application_h

#include "appConfig.h"
#include "simulation.h"
#include "result.h"
#include "logger.h"

/**
 * The MicrobiomeApplication class is the main class for the Microbiome application.
 * @author Daniel McCoy Stephenson
 */
class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        ~MicrobiomeApplication();
        bool run();
    private:
        AppConfig config;
        Simulation* simulation;
        std::vector<Result> results;
        Logger* logger;

        void printResults();
        void printResultAverages();
};

#endif