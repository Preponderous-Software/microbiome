#ifndef App_Config_h
#define App_Config_h

#include <cstddef>

/**
 * The AppConfig class contains the configuration for the application and 
 * is used in the MicrobiomeApplication and Simulation classes.
 * @author Daniel McCoy Stephenson
 */
class AppConfig {
    public:
        size_t getEnvironmentSize();
        size_t getEntityFactor();
        size_t getMaxTicks();
        size_t getTickLengthInSeconds();
        size_t getNumSimulations();
        bool isSimulationOutputEnabled();
        bool isMicroorganismReproductionEnabled();

        void setEnvironmentSize(size_t environmentSize);
        void setEntityFactor(size_t entityFactor);
        void setMaxTicks(size_t maxTicks);
        void setTickLengthInSeconds(size_t tickLengthInSeconds);
        void setNumSimulations(size_t numSimulations);
        void setSimulationOutputEnabled(bool simulationOutputEnabled);
        void setMicroorganismReproductionEnabled(bool microorganismReproductionEnabled);
    private:
        size_t environmentSize = 10;
        size_t entityFactor = 5;
        size_t maxTicks = 120; // set to 0 to run indefinitely until all microorganisms die
        size_t tickLengthInSeconds = 0; // set to 0 to run as fast as possible
        size_t numSimulations = 10;
        bool simulationOutputEnabled = true;
        bool microorganismReproductionEnabled = false; // for debugging purposes
};

#endif