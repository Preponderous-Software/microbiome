#ifndef Config_h
#define Config_h

class Config {
    public:
        int getEnvironmentSize();
        int getEntityFactor();
        int getMaxTicks();
        int getTickLengthInSeconds();
        int getNumSimulations();
        bool isSimulationOutputEnabled();
    private:
        int environmentSize = 10;
        int entityFactor = 5;
        int maxTicks = 100; // set to 0 to run indefinitely
        int tickLengthInSeconds = 1; // set to 0 to run as fast as possible
        int numSimulations = 5;
        bool simulationOutputEnabled = true;
};

#endif