#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "lib/catch2.hpp"

// Include the necessary headers from the project
#include "../src/header/appConfig.h"
#include "../src/header/simulation.h"
#include "../src/header/result.h"
#include "../src/header/microorganism.h"
#include "../src/header/microbiome.h"

// AppConfig Tests
TEST_CASE("AppConfig setters and getters work correctly", "[appconfig]") {
    AppConfig config;

    // Set values
    config.setEnvironmentSize(10);
    config.setEntityFactor(5);
    config.setMaxTicks(100);
    config.setTickLengthInSeconds(1);
    config.setNumSimulations(5);
    config.setSimulationOutputEnabled(true);

    // Verify values
    REQUIRE(config.getEnvironmentSize() == 10);
    REQUIRE(config.getEntityFactor() == 5);
    REQUIRE(config.getMaxTicks() == 100);
    REQUIRE(config.getTickLengthInSeconds() == 1);
    REQUIRE(config.getNumSimulations() == 5);
    REQUIRE(config.isSimulationOutputEnabled() == true);
}

// Microorganism Tests
TEST_CASE("Microorganism energy consumption", "[microorganism]") {
    Microorganism microorganism(0, "Test Microorganism");
    microorganism.setEnergy(10);
    
    REQUIRE(microorganism.getEnergy() == 10);
    
    int energyBefore = microorganism.getEnergy();
    int metabolicRate = microorganism.getMetabolicRate();
    microorganism.metabolize();
    
    REQUIRE(microorganism.getEnergy() == energyBefore - metabolicRate);
}

TEST_CASE("Microorganism times moved counter", "[microorganism]") {
    Microorganism microorganism(0, "Test Microorganism");
    
    REQUIRE(microorganism.getTimesMoved() == 0);
    
    microorganism.incrementTimesMoved();
    REQUIRE(microorganism.getTimesMoved() == 1);
    
    microorganism.incrementTimesMoved();
    REQUIRE(microorganism.getTimesMoved() == 2);
}

TEST_CASE("Microorganism times eaten counter", "[microorganism]") {
    Microorganism microorganism(0, "Test Microorganism");
    
    REQUIRE(microorganism.getTimesEaten() == 0);
    
    microorganism.incrementTimesEaten();
    REQUIRE(microorganism.getTimesEaten() == 1);
    
    microorganism.incrementTimesEaten();
    REQUIRE(microorganism.getTimesEaten() == 2);
}

// MicroorganismFactory Tests
TEST_CASE("MicroorganismFactory creates microorganisms with incremented IDs", "[factory]") {
    MicroorganismFactory factory;
    
    Microorganism microorganism1 = factory.createMicroorganism();
    REQUIRE(microorganism1.getId() == 0);
    
    Microorganism microorganism2 = factory.createMicroorganism();
    REQUIRE(microorganism2.getId() == 1);
    
    Microorganism microorganism3 = factory.createMicroorganism();
    REQUIRE(microorganism3.getId() == 2);
}

// Microbiome Tests
TEST_CASE("Microbiome creation with correct entity count", "[microbiome]") {
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    
    Microbiome microbiome(id, name, size, entityFactor);
    
    int numEntitiesInEnvironment = microbiome.getNumEntities();
    int numTrackedMicroorganisms = microbiome.getMicroorganisms().size();
    
    REQUIRE(numEntitiesInEnvironment == numTrackedMicroorganisms);
}

TEST_CASE("Microbiome can add and remove microorganisms", "[microbiome]") {
    // Create microbiome
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    // Create microorganism
    MicroorganismFactory factory;
    Microorganism microorganism = factory.createMicroorganism();

    // Add microorganism to microbiome
    microbiome.addMicroorganism(microorganism);

    // Verify presence
    REQUIRE(microbiome.isMicroorganismPresent(microorganism.getId()) == true);

    // Remove microorganism from microbiome
    microbiome.removeMicroorganism(microorganism.getId());

    // Verify absence
    REQUIRE(microbiome.isMicroorganismPresent(microorganism.getId()) == false);

    // Verify that microorganism is no longer in the microorganisms vector
    std::vector<Microorganism> microorganisms = microbiome.getMicroorganisms();
    bool microorganismFound = false;
    for (Microorganism& m : microorganisms) {
        if (m.getId() == microorganism.getId()) {
            microorganismFound = true;
        }
    }
    REQUIRE(microorganismFound == false);
}

// Simulation Tests
TEST_CASE("Simulation creation", "[simulation]") {
    AppConfig config;
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    
    if (config.getMaxTicks() > 0) {
        REQUIRE(simulation.getTicksElapsed() >= config.getMaxTicks());
    }
}

TEST_CASE("Simulation runs correctly", "[simulation]") {
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    config.setMaxTicks(10);  // Set a small number of ticks for faster testing
    
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    
    simulation.run();
    
    REQUIRE(simulation.getTicksElapsed() >= config.getMaxTicks());
}

TEST_CASE("Simulation results are consistent", "[simulation]") {
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    config.setMaxTicks(10);  // Set a small number of ticks for faster testing
    
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);
    
    simulation.run();
    Result result = Result(&simulation);
    
    REQUIRE(result.getSurvivingMicroorganisms() == simulation.getSurvivingMicroorganisms());
    REQUIRE(result.getDeadMicroorganisms() == simulation.getDeadMicroorganisms());
    REQUIRE(result.getEnergy() == simulation.getEnergy());
    REQUIRE(result.getTicksElapsed() == simulation.getTicksElapsed());
}