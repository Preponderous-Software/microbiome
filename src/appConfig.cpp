#include "header/appConfig.h"

size_t AppConfig::getEnvironmentSize() {
    return environmentSize;
}

void AppConfig::setEnvironmentSize(size_t environmentSize) {
    this->environmentSize = environmentSize;
}

size_t AppConfig::getEntityFactor() {
    return entityFactor;
}

void AppConfig::setEntityFactor(size_t entityFactor) {
    this->entityFactor = entityFactor;
}

size_t AppConfig::getMaxTicks() {
    return maxTicks;
}

void AppConfig::setMaxTicks(size_t maxTicks) {
    this->maxTicks = maxTicks;
}

size_t AppConfig::getTickLengthInSeconds() {
    return tickLengthInSeconds;
}

void AppConfig::setTickLengthInSeconds(size_t tickLengthInSeconds) {
    this->tickLengthInSeconds = tickLengthInSeconds;
}

size_t AppConfig::getNumSimulations() {
    return numSimulations;
}

void AppConfig::setNumSimulations(size_t numSimulations) {
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