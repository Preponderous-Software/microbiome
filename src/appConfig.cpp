#include "header/appConfig.h"

size_t AppConfig::getEnvironmentSize() {
    return environmentSize;
}

void AppConfig::setEnvironmentSize(int environmentSize) {
    this->environmentSize = environmentSize;
}

size_t AppConfig::getEntityFactor() {
    return entityFactor;
}

void AppConfig::setEntityFactor(int entityFactor) {
    this->entityFactor = entityFactor;
}

size_t AppConfig::getMaxTicks() {
    return maxTicks;
}

void AppConfig::setMaxTicks(int maxTicks) {
    this->maxTicks = maxTicks;
}

size_t AppConfig::getTickLengthInSeconds() {
    return tickLengthInSeconds;
}

void AppConfig::setTickLengthInSeconds(int tickLengthInSeconds) {
    this->tickLengthInSeconds = tickLengthInSeconds;
}

size_t AppConfig::getNumSimulations() {
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

bool AppConfig::isColonyScenarioEnabled() {
    return colonyScenarioEnabled;
}

void AppConfig::setColonyScenarioEnabled(bool colonyScenarioEnabled) {
    this->colonyScenarioEnabled = colonyScenarioEnabled;
}

bool AppConfig::isColonyNutrientRich() {
    return colonyNutrientRich;
}

void AppConfig::setColonyNutrientRich(bool colonyNutrientRich) {
    this->colonyNutrientRich = colonyNutrientRich;
}

size_t AppConfig::getColonyGridSize() {
    return colonyGridSize;
}

void AppConfig::setColonyGridSize(int colonyGridSize) {
    this->colonyGridSize = colonyGridSize;
}
