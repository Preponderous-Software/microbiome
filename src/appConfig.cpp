#include "header/appConfig.h"

int AppConfig::getEnvironmentSize() {
    return environmentSize;
}

void AppConfig::setEnvironmentSize(int environmentSize) {
    this->environmentSize = environmentSize;
}

int AppConfig::getEntityFactor() {
    return entityFactor;
}

void AppConfig::setEntityFactor(int entityFactor) {
    this->entityFactor = entityFactor;
}

int AppConfig::getMaxTicks() {
    return maxTicks;
}

void AppConfig::setMaxTicks(int maxTicks) {
    this->maxTicks = maxTicks;
}

int AppConfig::getTickLengthInSeconds() {
    return tickLengthInSeconds;
}

void AppConfig::setTickLengthInSeconds(int tickLengthInSeconds) {
    this->tickLengthInSeconds = tickLengthInSeconds;
}

int AppConfig::getNumSimulations() {
    return numSimulations;
}

void AppConfig::setNumSimulations(int numSimulations) {
    this->numSimulations = numSimulations;
}

bool AppConfig::isSimulationOutputEnabled() {
    return simulationOutputEnabled;
}

void AppConfig::setSimulationOutputEnabled(bool simulationOutputEnabled) {
    this->simulationOutputEnabled = simulationOutputEnabled;
}

bool AppConfig::isMicroorganismReproductionEnabled() {
    return microorganismReproductionEnabled;
}

void AppConfig::setMicroorganismReproductionEnabled(bool microorganismReproductionEnabled) {
    this->microorganismReproductionEnabled = microorganismReproductionEnabled;
}