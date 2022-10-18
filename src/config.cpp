#include "header/config.h"

int Config::getEnvironmentSize() {
    return environmentSize;
}

int Config::getEntityFactor() {
    return entityFactor;
}

int Config::getMaxTicks() {
    return maxTicks;
}

int Config::getTickLengthInSeconds() {
    return tickLengthInSeconds;
}

int Config::getNumSimulations() {
    return numSimulations;
}

bool Config::isSimulationOutputEnabled() {
    return simulationOutputEnabled;
}