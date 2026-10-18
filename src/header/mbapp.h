#ifndef Microbiome_Application_h
#define Microbiome_Application_h

#include "appConfig.h"
#include "simulation.h"
#include "result.h"

#include <string>

/**
 * The MicrobiomeApplication class is the main class for the Microbiome application.
 * @author Daniel McCoy Stephenson
 */
class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        bool run();
        void setRecordPath(std::string recordPath);
    private:
        std::string recordPath; // empty unless started with --record <path>
        AppConfig config;
        Simulation* simulation;
        std::vector<Result> results;
        void printResults();
        void printResultAverages();
};

#endif