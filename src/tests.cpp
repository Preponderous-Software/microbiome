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
#include "header/microorganismFactory.h"

Logger logger("log.tests.txt");

void testTemplate() {
    std::cout << "Test 0 - Template" << std::endl;
    assert(true);
}

void testConfigSettersAndGetters() {
    std::cout << "Test 1 - AppConfig" << std::endl;
    
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

}

void testMicroorganismCreation() {
    std::cout << "Test 2 - Microorganism Creation" << std::endl;

    MicroorganismFactory factory;
    Microorganism organism = factory.createMicroorganism();

    // assert id and name are expected values
    assert(organism.getId() == 0);
    assert(organism.getName() == "test name");
}

void testMicroorganismEnergyConsumption() {
    std::cout << "Test 3 - Microorganism Energy" << std::endl;
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    microorganism.setEnergy(10);
    assert(microorganism.getEnergy() == 10);
    microorganism.metabolize();
    assert(microorganism.getEnergy() == 10 - microorganism.getMetabolicRate());
}

void testMicroorganismTimesMoved() {
    std::cout << "Test 4 - Microorganism Times Moved" << std::endl;
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 1);
    microorganism.incrementTimesMoved();
    assert(microorganism.getTimesMoved() == 2);
}

void testMicroorganismTimesEaten() {
    std::cout << "Test 5 - Microorganism Times Eaten" << std::endl;
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 1);
    microorganism.incrementTimesEaten();
    assert(microorganism.getTimesEaten() == 2);
}

void testMicrobiomeCreation() {
    std::cout << "Test 6 - Microbiome Creation" << std::endl;
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);
    int numEntitiesInEnvironment = microbiome.getNumEntities();
    int numTrackedMicroorganisms = microbiome.getMicroorganisms().size();
    assert(numEntitiesInEnvironment == numTrackedMicroorganisms);
}

void testAddingMicroorganismToMicrobiome() {
    std::cout << "Test 7 - Adding Microorganisms to Microbiome" << std::endl;
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    microbiome.addMicroorganism(microorganism);

    // verify presence
    assert(microbiome.isMicroorganismPresent(microorganism.getId()));
}

void testRemovingMicroorganismFromMicrobiome() {
    std::cout << "Test 8 - Removing Microorganism From Microbiome" << std::endl;
    
    // create microbiome
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    // create microorganism
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();

    // add microorganism to microbiome
    microbiome.addMicroorganism(microorganism);

    // verify presence
    assert(microbiome.isMicroorganismPresent(microorganism.getId()));

    // remove microorganism from microbiome
    microbiome.removeMicroorganism(microorganism);

    // verify absence
    assert(!microbiome.isMicroorganismPresent(microorganism.getId()));

    // verify that microorganism is no longer in the microorganisms vector
    std::vector<Microorganism> microorganisms = microbiome.getMicroorganisms();
    bool microorganismFound = false;
    for (Microorganism m : microorganisms) {
        if (m.getId() == microorganism.getId()) {
            microorganismFound = true;
        }
    }
    assert(!microorganismFound);
}

void testMicroorganismReproductionEligibilityCheck() {
    std::cout << "Test 9 - Microorganism Reproduction Eligibility Check";
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    microorganism.setEnergy(100);
    microorganism.setReproductionThreshold(50);
    assert(microorganism.canReproduce());
}

void testMicroorganismReproduction() {
    std::cout << "Test 10 - Microorganism Reproduction" << std::endl;
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();
    int initialParentId = microorganism.getId();
    int initialEnergy = 100;
    int reproductionTheshold = 50;
    microorganism.setEnergy(initialEnergy);
    microorganism.setReproductionThreshold(reproductionTheshold);
    assert(microorganism.canReproduce());

    Microorganism child = factory.createMicroorganism();
    child.setEnergy(microorganism.getEnergy() / 2);
    child.setMetabolicRate(microorganism.getMetabolicRate());
    child.setReproductionThreshold(microorganism.getReproductionThreshold());
    microorganism.setEnergy(microorganism.getEnergy() / 2);

    assert(child.getEnergy() == initialEnergy / 2);
    assert(child.getReproductionThreshold() == microorganism.getReproductionThreshold());
    assert(child.getMetabolicRate() == microorganism.getMetabolicRate());
    assert(child.getTimesMoved() == 0);
    assert(child.getTimesEaten() == 0);

    // verify parent id hasn't changed
    assert(microorganism.getId() == initialParentId);
}

void testInitiatingMicroorganismReproductionInMicrobiome() {
    std::cout << "Test 11 - Initiating Microorganism Reproduction in Microbiome" << std::endl;
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    int initialNumMicroorganisms = microbiome.getMicroorganisms().size();

    // initiate reproduction
    microbiome.initiateMicroorganismReproduction();
    
    // verify that the number of microorganisms has increased
    assert(microbiome.getMicroorganisms().size() > initialNumMicroorganisms);

}

void testSimulationCreation() {
    std::cout << "Test 12 - Simulation Creation" << std::endl;
    AppConfig config;
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    assert(simulation.getTicksElapsed() == 0);
}

void testRunningSimulation() {
    std::cout << "Test 13 - Running Simulation" << std::endl;
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    simulation.run();
    assert(simulation.getTicksElapsed() <= config.getMaxTicks());
}

void testSimulationResults() {
    std::cout << "Test 14 - Simulation Results" << std::endl;
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
}

void seedRandomNumberGenerator() {
    srand (time (NULL));
}

int main() {
    // seed RNG
    seedRandomNumberGenerator();
    
    // tests
    testConfigSettersAndGetters();
    testMicroorganismCreation();
    testMicroorganismEnergyConsumption();
    testMicroorganismTimesMoved();
    testMicroorganismTimesEaten();
    testMicrobiomeCreation();
    testAddingMicroorganismToMicrobiome();
    testRemovingMicroorganismFromMicrobiome();
    testMicroorganismReproductionEligibilityCheck();
    testMicroorganismReproduction();
    testInitiatingMicroorganismReproductionInMicrobiome();
    testSimulationCreation();
    testRunningSimulation();
    testSimulationResults();
    return 0;
}