#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <string>
#include <iostream>
#include <assert.h>

#include "header/appConfig.h"
#include "header/simulation.h"
#include "header/result.h"
#include "header/microorganism.h"
#include "header/microbiome.h"

void testTemplate() {
    std::cout << "Test 0 - Template";
    assert(true);
    std::cout << " --- " << "Success" << std::endl;
}

void testConfigSettersAndGetters() {
    std::cout << "Test 1 - AppConfig";
    
    // create a config object
    AppConfig config;

    // set values
    config.setEnvironmentSize(10);
    config.setEntityFactor(5);
    config.setMaxTicks(100);
    config.setTickLengthInSeconds(1);
    config.setNumSimulations(5);
    config.setSimulationOutputEnabled(true);

    // get values
    assert(config.getEnvironmentSize() == 10);
    assert(config.getEntityFactor() == 5);
    assert(config.getMaxTicks() == 100);
    assert(config.getTickLengthInSeconds() == 1);
    assert(config.getNumSimulations() == 5);
    assert(config.isSimulationOutputEnabled() == true);

    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismEnergyConsumption() {
    std::cout << "Test 2 - Microorganism Energy";
    Microorganism microorganism(0, "Test Microorganism");
    int originalEnergy = microorganism.getEnergy();
    microorganism.setEnergy(10);
    assert(microorganism.getEnergy() == 10);
    microorganism.metabolize();
    assert(microorganism.getEnergy() == 10 - microorganism.getMetabolicRate());
    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismTimesMoved() {
    std::cout << "Test 3 - Microorganism Times Moved";
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 1);
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 2);
    std::cout << " --- " << "Success" << std::endl;
}

void testMicroorganismTimesEaten() {
    std::cout << "Test 4 - Microorganism Times Eaten";
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 1);
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 2);
    std::cout << " --- " << "Success" << std::endl;
}

void testMicrobiomeCreation() {
    std::cout << "Test 5 - Microbiome Creation";
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);
    int numEntitiesInEnvironment = microbiome.getNumEntities();
    int numTrackedMicroorganisms = microbiome.getMicroorganisms().size();
    assert(numEntitiesInEnvironment == numTrackedMicroorganisms);
    std::cout << " --- " << "Success" << std::endl;
}

void testSimulationCreation() {
    std::cout << "Test 6 - Simulation Creation";
    AppConfig config;
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    assert(simulation.getTicksElapsed() == 0);
    std::cout << " --- " << "Success" << std::endl;
}

void testRunningSimulation() {
    std::cout << "Test 7 - Running Simulation";
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    simulation.run();
    assert(simulation.getTicksElapsed() <= config.getMaxTicks());
    std::cout << " --- " << "Success" << std::endl;
}

void testSimulationResults() {
    std::cout << "Test 8 - Simulation Results";
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    simulation.run();
    Result result = Result(&simulation);
    assert(result.getSurvivingMicroorganisms() == simulation.getSurvivingMicroorganisms());
    assert(result.getDeadMicroorganisms() == simulation.getDeadMicroorganisms());
    assert(result.getEnergy() == simulation.getEnergy());
    assert(result.getTicksElapsed() == simulation.getTicksElapsed());
    std::cout << " --- " << "Success" << std::endl;
}

// void testRemovingMicroorganismFromMicrobiome() {
//     std::cout << "Test 9 - Removing Microorganism From Microbiome";

//     // TODO: implement

//     std::cout << " --- " << "Success" << std::endl;
// }

void seedRandomNumberGenerator() {
    srand (time (NULL));
}

int main() {
    // seed RNG
    seedRandomNumberGenerator();
    
    // tests
    testConfigSettersAndGetters();
    testMicroorganismEnergyConsumption();
    testMicroorganismTimesMoved();
    testMicroorganismTimesEaten();
    testMicrobiomeCreation();
    testSimulationCreation();
    testRunningSimulation();
    testSimulationResults();
    // testRemovingMicroorganismFromMicrobiome();
    return 0;
}