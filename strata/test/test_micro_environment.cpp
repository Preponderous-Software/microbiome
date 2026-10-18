// Catch2 tests for the Strata micro-environment (strata/src/micro_environment.*).
#include "catch2.hpp"

#include <array>
#include <cmath>
#include <vector>

#include "micro_environment.h"
#include "strata/sim/field.hpp"

using namespace mbstrata;

namespace {

// A small, empty chamber: no grains, no seeps, nobody in it.
Config emptyChamber(int size = 48) {
    Config c;
    c.width = size;
    c.height = size;
    c.initialPerSpecies = 0;
    c.grainCount = 0;
    c.nutrientSources = 0;
    c.initialNutrient = 0.0f;
    c.oxygenSurface = 0.0f;
    return c;
}

}  // namespace

TEST_CASE("Diffusion conserves nutrient mass around grains when nothing decays or eats", "[strata][fields]") {
    Config c;
    c.width = 96;
    c.height = 80;
    c.initialPerSpecies = 0;
    c.nutrientSources = 0;
    c.nutrientDecay = 0.0f;
    c.wasteDecay = 0.0f;
    c.oxygenSurface = 0.0f;  // the air interface is a source; switch it off
    MicroEnvironment env(c);
    REQUIRE(env.grains().blockedCount() > 0);

    // A lumpy start: a few hot spots (in open cells) on top of the uniform
    // initial nutrient.
    auto open = [&](int x, int y) {
        while (env.grains().blocked(x, y)) x = (x + 1) % c.width;
        return std::array<int, 2>{x, y};
    };
    const auto a = open(10, 70), b = open(80, 20), w = open(40, 40);
    env.nutrient().add(a[0], a[1], 5000.0f);
    env.nutrient().add(b[0], b[1], 3000.0f);
    env.waste().add(w[0], w[1], 1000.0f);
    const double nutrientBefore = env.nutrient().total();
    const double wasteBefore = env.waste().total();

    env.run(500);

    CHECK(std::fabs(env.nutrient().total() - nutrientBefore) / nutrientBefore < 1e-5);
    CHECK(std::fabs(env.waste().total() - wasteBefore) / wasteBefore < 1e-5);
    // and it really did spread
    CHECK(env.nutrient().maxValue() < 5000.0f);
    // nothing leaks into the grains
    for (int y = 0; y < c.height; ++y)
        for (int x = 0; x < c.width; ++x)
            if (env.grains().blocked(x, y)) REQUIRE(env.nutrient().at(x, y) == 0.0f);
}

TEST_CASE("Decay removes the configured fraction each tick", "[strata][fields]") {
    Config c = emptyChamber(32);
    c.initialNutrient = 10.0f;
    c.nutrientDecay = 0.01f;
    MicroEnvironment env(c);
    const double before = env.nutrient().total();
    env.run(10);
    CHECK(env.nutrient().total() == Approx(before * std::pow(0.99, 10)).epsilon(1e-4));
}

TEST_CASE("Chemotaxis moves aerobes up a nutrient gradient", "[strata][chemotaxis]") {
    auto meanDrift = [](bool chemotaxis) {
        Config c = emptyChamber(64);
        c.seed = 7;
        c.chemotaxisEnabled = chemotaxis;
        c.feedingEnabled = false;      // keep the gradient fixed
        c.metabolismEnabled = false;   // nobody dies
        c.reproductionEnabled = false; // nobody divides
        c.fieldsEnabled = false;       // no diffusion flattening the ramp
        MicroEnvironment env(c);
        // A linear ramp: richer towards +x.
        for (int y = 0; y < c.height; ++y)
            for (int x = 0; x < c.width; ++x) env.nutrient().set(x, y, static_cast<float>(x));
        for (int i = 0; i < 300; ++i) env.spawn(Species::Aerobe, 32.0f, 16.0f + static_cast<float>(i % 32), 500.0f, 2);
        env.run(40);
        double sum = 0;
        int n = 0;
        env.world().each<Body>([&](strata::sim::Entity, const Body& b) {
            sum += b.x - 32.0;
            ++n;
        });
        return sum / n;
    };
    const double withChemotaxis = meanDrift(true);
    const double without = meanDrift(false);
    INFO("mean x drift with chemotaxis " << withChemotaxis << ", without " << without);
    CHECK(withChemotaxis > 3.0);
    CHECK(std::fabs(without) < 1.5);
    CHECK(withChemotaxis > without + 3.0);
}

TEST_CASE("Cross-feeders swim up the waste gradient, not the nutrient one", "[strata][chemotaxis]") {
    Config c = emptyChamber(64);
    c.seed = 11;
    c.feedingEnabled = false;
    c.metabolismEnabled = false;
    c.reproductionEnabled = false;
    c.fieldsEnabled = false;
    MicroEnvironment env(c);
    for (int y = 0; y < c.height; ++y)
        for (int x = 0; x < c.width; ++x) {
            env.nutrient().set(x, y, static_cast<float>(x));        // richer to the right
            env.waste().set(x, y, static_cast<float>(c.width - x)); // richer to the left
        }
    for (int i = 0; i < 300; ++i) env.spawn(Species::CrossFeeder, 32.0f, 16.0f + static_cast<float>(i % 32), 500.0f, 2);
    env.run(40);
    double sum = 0;
    int n = 0;
    env.world().each<Body>([&](strata::sim::Entity, const Body& b) {
        sum += b.x - 32.0;
        ++n;
    });
    CHECK(sum / n < -2.0);
}

TEST_CASE("Same seed gives an identical state hash every tick; another seed does not", "[strata][determinism]") {
    Config c;
    c.width = 96;
    c.height = 96;
    c.initialPerSpecies = 150;
    c.seed = 2026;
    MicroEnvironment a(c), b(c);
    REQUIRE(a.stateHash() == b.stateHash());
    for (int t = 0; t < 300; ++t) {
        a.step();
        b.step();
        REQUIRE(a.stateHash() == b.stateHash());
    }
    // The run actually did something: births, deaths and field changes.
    const Stats s = a.stats();
    CHECK(s.births > 0);
    CHECK(s.deaths > 0);

    Config other = c;
    other.seed = 2027;
    MicroEnvironment d(other);
    d.run(300);
    CHECK(d.stateHash() != a.stateHash());
}

TEST_CASE("Binary fission keeps the console app's threshold and cost", "[strata][reproduction]") {
    Config c = emptyChamber(16);
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    c.metabolismEnabled = false;
    MicroEnvironment env(c);
    env.spawn(Species::Fermenter, 8.5f, 8.5f, 1000.0f, 1);
    env.step();
    REQUIRE(env.world().size() == 2);
    env.world().each<Cell>([&](strata::sim::Entity, const Cell& cell) {
        CHECK(cell.energy == Approx((1000.0f - c.fissionCost) / 2.0f));
        CHECK(cell.metabolicRate == 1);
        CHECK(cell.species == Species::Fermenter);
    });
    CHECK(env.stats().births == 1);
}

TEST_CASE("A cell below the fission threshold does not divide", "[strata][reproduction]") {
    Config c = emptyChamber(16);
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    c.metabolismEnabled = false;
    MicroEnvironment env(c);
    env.spawn(Species::Aerobe, 8.5f, 8.5f, c.reproductionEnergyThreshold - 1.0f, 1);
    env.run(5);
    CHECK(env.world().size() == 1);
}

TEST_CASE("Death releases the biomatter yield into the nutrient field", "[strata][death]") {
    Config c = emptyChamber(16);
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    MicroEnvironment env(c);
    env.spawn(Species::Aerobe, 4.5f, 4.5f, 2.0f, 4);  // metabolises 4 a tick: dies on the first
    const double before = env.nutrient().total();
    env.step();
    CHECK(env.world().size() == 0);
    CHECK(env.stats().deaths == 1);
    CHECK(env.nutrient().total() - before == Approx(c.biomatterYieldPerDeath));
}

TEST_CASE("Aerobes eat nutrient and oxygen and excrete waste", "[strata][metabolism]") {
    Config c = emptyChamber(16);
    c.fieldsEnabled = false;
    c.reproductionEnabled = false;
    MicroEnvironment env(c);
    env.nutrient().fill(10.0f);
    env.oxygen().fill(10.0f);
    strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 2);
    env.step();
    const Cell* cell = env.world().get<Cell>(e);
    REQUIRE(cell != nullptr);
    const SpeciesTraits& t = traitsOf(Species::Aerobe);
    const float eaten = 2.0f * t.uptakeMultiplier;
    CHECK(cell->energy == Approx(500.0f - 2.0f + eaten * t.aerobicYield));
    CHECK(cell->timesEaten == 1);
    CHECK(env.waste().total() == Approx(eaten * t.wasteFraction));
    CHECK(env.oxygen().total() == Approx(16.0 * 16.0 * 10.0 - eaten * t.oxygenPerUnit));
}

TEST_CASE("Oxygen costs fermenters energy; fermenters settle into biofilms on grains", "[strata][species]") {
    Config c = emptyChamber(16);
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    c.reproductionEnabled = false;
    MicroEnvironment env(c);
    env.oxygen().fill(10.0f);
    strata::sim::Entity f = env.spawn(Species::Fermenter, 8.5f, 8.5f, 500.0f, 1);
    env.step();
    const SpeciesTraits& t = traitsOf(Species::Fermenter);
    // The cell may swim within the chamber, but the oxygen is uniform.
    CHECK(env.world().get<Cell>(f)->energy == Approx(500.0f - 1.0f - (10.0f - t.oxygenTolerance) * c.oxygenToxicity));

    // At the chamber wall (a surface), a fermenter eventually attaches.
    Config wall = emptyChamber(16);
    wall.fieldsEnabled = false;
    wall.feedingEnabled = false;
    wall.metabolismEnabled = false;
    wall.reproductionEnabled = false;
    MicroEnvironment env2(wall);
    for (int i = 0; i < 20; ++i) env2.spawn(Species::Fermenter, 0.5f, 0.5f + static_cast<float>(i % 16), 500.0f, 1);
    env2.run(400);
    CHECK(env2.stats().attached > 0);
}

TEST_CASE("A default chamber stays alive and mixed for a while", "[strata][ecology]") {
    Config c;  // the defaults mb_strata runs
    MicroEnvironment env(c);
    env.run(600);
    const Stats s = env.stats();
    INFO(env.summary());
    CHECK(s.population() > 0);
    CHECK(s.alive[0] > 0);
    CHECK(s.alive[1] > 0);
    CHECK(s.alive[2] > 0);
}
