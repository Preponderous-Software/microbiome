#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "lib/catch2.hpp"

// Include the necessary headers from the project
#include "../src/header/appConfig.h"
#include "../src/header/simulation.h"
#include "../src/header/result.h"
#include "../src/header/microorganism.h"
#include "../src/header/microbiome.h"
#include "../src/header/logger.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

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
// Shared fixture setup (id/size/entityFactor/name/Microbiome construction) is factored via
// SECTIONs: Catch2 re-runs the TEST_CASE body once per SECTION, so each still gets a fresh,
// isolated Microbiome instance.
TEST_CASE("Microbiome entity tracking and lifecycle", "[microbiome]") {
    int id = 0;
    int size = 10;
    int entityFactor = 5;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    SECTION("creation results in correct entity count") {
        int numEntitiesInEnvironment = microbiome.getNumEntities();
        int numTrackedMicroorganisms = microbiome.getMicroorganisms().size();

        REQUIRE(numEntitiesInEnvironment == numTrackedMicroorganisms);
    }

    SECTION("can add and remove microorganisms") {
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
        std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
        bool microorganismFound = false;
        for (Microorganism* m : microorganisms) {
            if (m->getId() == microorganism.getId()) {
                microorganismFound = true;
            }
        }
        REQUIRE(microorganismFound == false);
    }
}

TEST_CASE("Microorganism reproduction increases the population", "[microbiome]") {
    int id = 0;
    int size = 3;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    size_t populationBefore = microbiome.getMicroorganisms().size();

    // give one microorganism enough energy to reproduce this tick; the
    // maximum single-tick metabolic cost (4) can't bring it back under the
    // reproduction threshold (900), so reproduction is guaranteed to occur.
    microbiome.getMicroorganisms()[0]->setEnergy(950);

    microbiome.initiateMicroorganismMovement();

    REQUIRE(microbiome.getMicroorganisms().size() > populationBefore);
}

// A 2x2 grid holds two microorganisms and gives every location two neighbours, so a parent always
// has somewhere to place its daughter. Neither organism can forage (nothing has died, so there is
// no biomatter), which leaves metabolism and fission as the only changes to their energy.
TEST_CASE("Binary fission splits the parent's remaining energy between two daughters", "[microbiome]") {
    int id = 0;
    int size = 2;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
    REQUIRE(microorganisms.size() == 2);
    Microorganism* parent = microorganisms[0];
    Microorganism* bystander = microorganisms[1];
    bystander->setMetabolicRate(1);
    bystander->setEnergy(500);

    SECTION("each daughter receives half of what is left after the fission cost") {
        parent->setMetabolicRate(2);
        parent->setEnergy(950);

        microbiome.initiateMicroorganismMovement();

        std::vector<Microorganism*> after = microbiome.getMicroorganisms();
        REQUIRE(after.size() == 3);
        REQUIRE(microbiome.getNumEntities() == 3);
        // 950 - 2 (metabolism) = 948; (948 - 100 fission cost) / 2 = 424
        REQUIRE(parent->getEnergy() == 424);
        REQUIRE(bystander->getEnergy() == 499);

        Microorganism* daughter = after[2];
        // entity ids continue on from the size x entityFactor initial population
        REQUIRE(daughter->getId() == 2);
        REQUIRE(daughter->getMetabolicRate() == 2);
        // the daughter is appended to the population mid-tick, so it moves and metabolizes in
        // the same tick it was created in: 424 - 2 = 422
        REQUIRE(daughter->getTimesMoved() == 1);
        REQUIRE(daughter->getEnergy() == 422);
    }

    SECTION("an organism at exactly the threshold after metabolizing divides") {
        parent->setMetabolicRate(1);
        parent->setEnergy(901);

        microbiome.initiateMicroorganismMovement();

        REQUIRE(microbiome.getMicroorganisms().size() == 3);
        // (900 - 100) / 2 = 400
        REQUIRE(parent->getEnergy() == 400);
    }

    SECTION("an organism one unit below the threshold after metabolizing does not divide") {
        parent->setMetabolicRate(1);
        parent->setEnergy(900);

        microbiome.initiateMicroorganismMovement();

        REQUIRE(microbiome.getMicroorganisms().size() == 2);
        REQUIRE(parent->getEnergy() == 899);
    }
}

// On a 1x1 grid nothing can move or divide (there are no adjacent locations), so the parent keeps
// every unit of energy and the population is unchanged however much energy it holds.
TEST_CASE("Binary fission needs an adjacent location for the daughter", "[microbiome]") {
    Microbiome microbiome(0, "Test Microbiome", 1, 1);
    Microorganism* microorganism = microbiome.getMicroorganisms()[0];
    microorganism->setMetabolicRate(1);
    microorganism->setEnergy(1000);

    microbiome.initiateMicroorganismMovement();

    REQUIRE(microbiome.getMicroorganisms().size() == 1);
    REQUIRE(microorganism->getEnergy() == 999);
}

TEST_CASE("Death produces forageable biomatter", "[microbiome]") {
    int id = 0;
    int size = 3;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
    size_t populationSize = microorganisms.size();
    for (Microorganism* microorganism : microorganisms) {
        // low enough that any metabolic rate (1-4) kills it this tick
        microorganism->setEnergy(1);
    }

    microbiome.initiateMicroorganismMovement();

    REQUIRE(microbiome.getNumAliveMicroorganisms() == 0);
    REQUIRE(microbiome.getNumDeadMicroorganisms() == (int) populationSize);
    // the population's energy didn't just vanish - it should still be
    // recoverable as biomatter in the environment.
    REQUIRE(microbiome.getTotalEnergy() > 0);
}

// On a 1x1 grid there are no adjacent locations, so nothing moves and every entity shares the
// single location. Microorganisms are processed in creation order, so the first one's death
// leaves biomatter where the second one then forages it within the same tick.
TEST_CASE("Microorganisms forage biomatter at their location", "[microbiome][biomatter]") {
    int id = 0;
    int size = 1;
    int entityFactor = 2;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    std::vector<Microorganism*> microorganisms = microbiome.getMicroorganisms();
    REQUIRE(microorganisms.size() == 2);
    Microorganism* dying = microorganisms[0];
    Microorganism* forager = microorganisms[1];
    dying->setMetabolicRate(1);
    dying->setEnergy(1);
    forager->setMetabolicRate(3);
    forager->setEnergy(500);

    microbiome.initiateMicroorganismMovement();

    SECTION("foraging replaces the energy spent metabolizing that tick") {
        REQUIRE(microbiome.getMicroorganisms().size() == 1);
        REQUIRE(forager->getEnergy() == 500);
        REQUIRE(forager->getTimesEaten() == 1);
        REQUIRE(microbiome.getBiomatter().size() == 1);
    }

    SECTION("each tick depletes the biomatter by the forager's metabolic rate") {
        Biomatter* biomatter = microbiome.getBiomatter()[0];
        int biomatterEnergyBefore = biomatter->getEnergy();

        microbiome.initiateMicroorganismMovement();

        REQUIRE(biomatter->getEnergy() == biomatterEnergyBefore - 3);
        REQUIRE(forager->getEnergy() == 500);
        REQUIRE(forager->getTimesEaten() == 2);
        REQUIRE(microbiome.getTotalEnergy() == forager->getEnergy() + biomatter->getEnergy());
    }

    SECTION("depleted biomatter is removed from the environment") {
        microbiome.getBiomatter()[0]->setEnergy(0);

        microbiome.initiateMicroorganismMovement();

        REQUIRE(microbiome.getBiomatter().empty());
        REQUIRE(microbiome.getNumEntities() == 1);
        // there was nothing left to forage, so the forager only paid its metabolic cost
        REQUIRE(forager->getEnergy() == 497);
        REQUIRE(forager->getTimesEaten() == 1);
    }
}

TEST_CASE("Microbiome total energy is clamped at zero", "[microbiome]") {
    int id = 0;
    int size = 3;
    int entityFactor = 1;
    std::string name = "Test Microbiome";
    Microbiome microbiome(id, name, size, entityFactor);

    for (Microorganism* microorganism : microbiome.getMicroorganisms()) {
        microorganism->setEnergy(-100);
    }

    REQUIRE(microbiome.getTotalEnergy() == 0);
}

// Console representation Tests
// Each cell is printed as " <glyph> ", one grid row per line, between two borders of
// 3 x size '=' characters, with a blank line before the first border and after the last.
namespace {

std::string renderConsole(Microbiome& microbiome) {
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    microbiome.printConsoleRepresentation();
    std::cout.rdbuf(original);
    return captured.str();
}

std::string singleCellFrame(const std::string& glyph) {
    return "\n===\n " + glyph + " \n===\n\n";
}

}

TEST_CASE("Console representation draws a bordered row per grid row", "[microbiome][console]") {
    // an entity factor of 0 leaves the 3x3 grid empty
    Microbiome microbiome(0, "Test Microbiome", 3, 0);

    REQUIRE(renderConsole(microbiome) ==
            "\n=========\n"
            "         \n"
            "         \n"
            "         \n"
            "=========\n\n");
}

// A 1x1 grid holds a single cell, so the frame is fully determined by that cell's glyph.
TEST_CASE("Console representation glyphs reflect each microorganism's state", "[microbiome][console]") {
    Microbiome microbiome(0, "Test Microbiome", 1, 1);
    Microorganism* microorganism = microbiome.getMicroorganisms()[0];
    microorganism->setEnergy(500);

    SECTION("a microorganism that has never foraged is drawn as o") {
        REQUIRE(renderConsole(microbiome) == singleCellFrame("o"));
    }

    SECTION("a microorganism that has foraged is drawn as +") {
        microorganism->incrementTimesEaten();
        REQUIRE(renderConsole(microbiome) == singleCellFrame("+"));
    }

    SECTION("a microorganism below 10 energy is drawn as !, even if it has foraged") {
        microorganism->incrementTimesEaten();
        microorganism->setEnergy(9);
        REQUIRE(renderConsole(microbiome) == singleCellFrame("!"));
    }

    SECTION("a microorganism with exactly 10 energy is not yet drawn as dying") {
        microorganism->setEnergy(10);
        REQUIRE(renderConsole(microbiome) == singleCellFrame("o"));
    }

    SECTION("the biomatter left by a death is drawn as .") {
        microorganism->setMetabolicRate(1);
        microorganism->setEnergy(1);
        microbiome.initiateMicroorganismMovement();

        REQUIRE(microbiome.getMicroorganisms().empty());
        REQUIRE(renderConsole(microbiome) == singleCellFrame("."));
    }
}

// Biomatter Tests
TEST_CASE("Biomatter energy and depletion", "[biomatter]") {
    Biomatter biomatter(0, "Test Biomatter", 60);

    REQUIRE(biomatter.getEnergy() == 60);
    REQUIRE(biomatter.isDepleted() == false);

    biomatter.setEnergy(1);
    REQUIRE(biomatter.getEnergy() == 1);
    REQUIRE(biomatter.isDepleted() == false);

    biomatter.setEnergy(0);
    REQUIRE(biomatter.isDepleted() == true);

    biomatter.setEnergy(-5);
    REQUIRE(biomatter.isDepleted() == true);
}

// Simulation Tests
TEST_CASE("Simulation creation", "[simulation]") {
    AppConfig config;
    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);

    // a freshly constructed simulation has not run yet, and is populated with one
    // microorganism per (environment size x entity factor).
    REQUIRE(simulation.getTicksElapsed() == 0);
    REQUIRE(simulation.getSurvivingMicroorganisms() == config.getEnvironmentSize() * config.getEntityFactor());
    REQUIRE(simulation.getDeadMicroorganisms() == 0);
    // a microorganism is dead once its energy reaches zero, so every survivor
    // carries at least one unit of energy.
    REQUIRE(simulation.getEnergy() >= simulation.getSurvivingMicroorganisms());
}

// Shared fixture setup (config + Simulation construction + run()) is factored via SECTIONs:
// Catch2 re-runs the TEST_CASE body once per SECTION, so each still gets a fresh, isolated
// Simulation instance.
TEST_CASE("Simulation runs and produces consistent results", "[simulation]") {
    AppConfig config;
    config.setTickLengthInSeconds(0);
    config.setSimulationOutputEnabled(false);
    config.setMaxTicks(10);  // Set a small number of ticks for faster testing

    int id = 0;
    std::string name = "Test Simulation";
    Simulation simulation(&config, id, name);

    simulation.run();

    SECTION("ticks elapsed reaches configured maximum") {
        REQUIRE(simulation.getTicksElapsed() >= config.getMaxTicks());
    }

    SECTION("Result mirrors Simulation state") {
        Result result = Result(&simulation);

        REQUIRE(result.getSurvivingMicroorganisms() == simulation.getSurvivingMicroorganisms());
        REQUIRE(result.getDeadMicroorganisms() == simulation.getDeadMicroorganisms());
        REQUIRE(result.getEnergy() == simulation.getEnergy());
        REQUIRE(result.getTicksElapsed() == simulation.getTicksElapsed());
    }

    SECTION("Result prints each field under a header") {
        Result result = Result(&simulation);

        std::ostringstream captured;
        std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
        result.print();
        std::cout.rdbuf(original);

        std::ostringstream expected;
        expected << "=== Simulation Result ===\n"
                 << "Surviving Microorganisms: " << result.getSurvivingMicroorganisms() << "\n"
                 << "Dead Microorganisms: " << result.getDeadMicroorganisms() << "\n"
                 << "Total Energy: " << result.getEnergy() << "\n"
                 << "Ticks elapsed: " << result.getTicksElapsed() << "\n";
        REQUIRE(captured.str() == expected.str());
    }
}

// Logger Tests
// Each test logs to its own temporary file, so nothing here touches the log.*.txt files
// the simulation itself writes to the working directory.
namespace {

std::string makeTempLogPath() {
    char pattern[] = "/tmp/microbiome-logger-XXXXXX";
    int fd = mkstemp(pattern);
    close(fd);
    return pattern;
}

std::vector<std::string> readLines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

}

TEST_CASE("Logger writes each message on its own timestamped line", "[logger]") {
    std::string path = makeTempLogPath();
    {
        Logger logger(path);
        logger.log("first message");
        logger.log("second message");
    }

    std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 2);
    for (const std::string& line : lines) {
        // "[<ctime() timestamp without its trailing newline>] <message>"; ctime() always
        // yields the fixed 24-character "Www Mmm dd hh:mm:ss yyyy" form.
        REQUIRE(line.front() == '[');
        REQUIRE(line.find("] ") == 25);
    }
    REQUIRE(lines[0].substr(lines[0].find("] ") + 2) == "first message");
    REQUIRE(lines[1].substr(lines[1].find("] ") + 2) == "second message");

    std::remove(path.c_str());
}

TEST_CASE("Logger truncates an existing log file when constructed", "[logger]") {
    std::string path = makeTempLogPath();
    {
        Logger logger(path);
        logger.log("from an earlier run");
    }
    {
        Logger logger(path);
        logger.log("from this run");
    }

    std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].substr(lines[0].find("] ") + 2) == "from this run");

    std::remove(path.c_str());
}
