// Catch2 tests for the Strata micro-environment (strata/src/micro_environment.*).
#include "catch2.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
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

// An empty chamber with every rule off; a test turns on the one it is about.
Config stillChamber(int size = 16) {
    Config c = emptyChamber(size);
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    c.metabolismEnabled = false;
    c.reproductionEnabled = false;
    return c;
}

// A fermenter already in a biofilm, so it stays where it is put.
strata::sim::Entity settle(MicroEnvironment& env, float x, float y, float energy) {
    strata::sim::Entity e = env.spawn(Species::Fermenter, x, y, energy, 1);
    env.world().get<Body>(e)->attached = 1;
    return e;
}

// Fills the eight grid cells around (cx, cy) to the crowding limit.
void crowdAround(MicroEnvironment& env, int cx, int cy) {
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            for (int i = 0; i < env.config().crowdingLimit; ++i)
                settle(env, static_cast<float>(cx + dx) + 0.5f, static_cast<float>(cy + dy) + 0.5f, 100.0f);
        }
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

TEST_CASE("A new chamber holds each species with the console app's trait ranges, in open cells", "[strata][setup]") {
    Config c;
    c.width = 64;
    c.height = 64;
    c.initialPerSpecies = 50;
    MicroEnvironment env(c);
    const Stats s = env.stats();
    CHECK(s.alive[0] == 50);
    CHECK(s.alive[1] == 50);
    CHECK(s.alive[2] == 50);
    CHECK(s.attached == 0);
    env.world().each<Body, Cell>([&](strata::sim::Entity, const Body& body, const Cell& cell) {
        CHECK_FALSE(env.grains().blocked(static_cast<int>(body.x), static_cast<int>(body.y)));
        CHECK(cell.energy > 100.0f);
        CHECK(cell.energy <= 1000.0f);
        CHECK(cell.metabolicRate >= 1);
        CHECK(cell.metabolicRate <= 4);
        CHECK(cell.generation == 0);
    });
}

TEST_CASE("A chamber smaller than 8x8 is refused, and so is an unknown species", "[strata][setup]") {
    REQUIRE_THROWS_AS(MicroEnvironment(emptyChamber(7)), std::invalid_argument);
    REQUIRE_THROWS_AS(traitsOf(static_cast<Species>(kSpeciesCount)), std::invalid_argument);
}

TEST_CASE("spawn moves a position in a grain along +x and clamps one outside the chamber", "[strata][setup]") {
    SECTION("into the next open cell of the row") {
        Config c = stillChamber(32);
        c.grainCount = 6;
        MicroEnvironment env(c);
        REQUIRE(env.grains().blockedCount() > 0);

        int grainX = -1, grainY = -1;
        for (int y = 0; y < c.height && grainX < 0; ++y)
            for (int x = 0; x < c.width && grainX < 0; ++x)
                if (env.grains().blocked(x, y)) {
                    grainX = x;
                    grainY = y;
                }
        int openX = grainX;
        while (env.grains().blocked(openX, grainY)) openX = (openX + 1) % c.width;

        strata::sim::Entity e = env.spawn(Species::Aerobe, static_cast<float>(grainX) + 0.5f, static_cast<float>(grainY) + 0.5f, 500.0f, 1);
        const Body* body = env.world().get<Body>(e);
        CHECK(body->x == Approx(static_cast<float>(openX) + 0.5f));
        CHECK(body->y == Approx(static_cast<float>(grainY) + 0.5f));
    }

    SECTION("back inside the walls, with a metabolic rate of at least 1") {
        MicroEnvironment env(stillChamber(32));
        strata::sim::Entity e = env.spawn(Species::Aerobe, -5.0f, 100.0f, 500.0f, 0);
        const Body* body = env.world().get<Body>(e);
        CHECK(body->x == 0.0f);
        CHECK(static_cast<int>(body->y) == 31);
        CHECK(env.world().get<Cell>(e)->metabolicRate == 1);
    }
}

TEST_CASE("Seeps sit below the top eighth on open cells and add their rate every tick", "[strata][fields]") {
    Config c;
    c.initialPerSpecies = 0;
    MicroEnvironment chamber(c);
    REQUIRE(chamber.sources().size() == 12);
    for (const auto& seep : chamber.sources()) {
        CHECK(seep[0] >= 1);
        CHECK(seep[0] <= c.width - 2);
        CHECK(seep[1] >= c.height / 8);
        CHECK(seep[1] <= c.height - 2);
        CHECK_FALSE(chamber.grains().blocked(seep[0], seep[1]));
    }

    Config dry = emptyChamber(32);
    dry.nutrientSources = c.nutrientSources;
    dry.nutrientDecay = 0.0f;
    MicroEnvironment env(dry);
    env.run(3);
    CHECK(env.nutrient().total() == Approx(3.0 * 1800.0));  // the 1,800 a tick the README quotes
}

TEST_CASE("The air interface refills the top edge and oxygen falls away below it", "[strata][fields]") {
    Config c = emptyChamber(32);
    c.oxygenSurface = 10.0f;
    MicroEnvironment env(c);
    env.oxygen().fill(0.0f);

    env.step();
    CHECK(env.oxygen().at(16, 0) > 0.0f);
    CHECK(env.oxygen().at(16, 20) == 0.0f);  // eight substeps reach eight rows at most

    env.run(60);
    CHECK(env.oxygen().at(16, 0) > env.oxygen().at(16, 8));
    CHECK(env.oxygen().at(16, 8) > env.oxygen().at(16, 24));
    CHECK(env.oxygen().at(16, 24) > 0.0f);
    CHECK(env.oxygen().maxValue() <= c.oxygenSurface);
}

TEST_CASE("An aerobe's yield depends on how much of its meal oxygen covers", "[strata][metabolism]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    c.feedingEnabled = true;
    MicroEnvironment env(c);
    env.nutrient().fill(10.0f);
    const SpeciesTraits& t = traitsOf(Species::Aerobe);
    const float meal = 2.0f * t.uptakeMultiplier;

    SECTION("no oxygen: all of it at the anaerobic yield") {
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 2);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(500.0f - 2.0f + meal * t.anaerobicYield));
    }

    SECTION("oxygen for half the meal: half at each yield") {
        env.oxygen().fill(meal * 0.5f * t.oxygenPerUnit);
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 2);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(500.0f - 2.0f + meal * 0.5f * (t.aerobicYield + t.anaerobicYield)));
    }
}

TEST_CASE("A meal is capped by what the cell holds", "[strata][metabolism]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    c.feedingEnabled = true;
    MicroEnvironment env(c);
    const SpeciesTraits& t = traitsOf(Species::Aerobe);

    SECTION("less food than it wants") {
        env.nutrient().fill(3.0f);
        const double before = env.nutrient().total();
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 2);
        env.step();
        const Cell* cell = env.world().get<Cell>(e);
        CHECK(cell->energy == Approx(500.0f - 2.0f + 3.0f * t.anaerobicYield));
        CHECK(cell->timesEaten == 1);
        CHECK(env.nutrient().total() == Approx(before - 3.0));
    }

    SECTION("no food is no meal") {
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 2);
        env.step();
        const Cell* cell = env.world().get<Cell>(e);
        CHECK(cell->energy == Approx(498.0f));
        CHECK(cell->timesEaten == 0);
    }
}

TEST_CASE("Each species draws only on its own fields", "[strata][metabolism]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    c.feedingEnabled = true;
    MicroEnvironment env(c);
    env.nutrient().fill(10.0f);
    env.waste().fill(5.0f);
    const double nutrientBefore = env.nutrient().total();
    const double wasteBefore = env.waste().total();

    SECTION("a cross-feeder eats waste, leaves nutrient alone and excretes nothing") {
        const SpeciesTraits& t = traitsOf(Species::CrossFeeder);
        strata::sim::Entity e = env.spawn(Species::CrossFeeder, 8.5f, 8.5f, 500.0f, 1);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(500.0f - 1.0f + t.uptakeMultiplier * t.anaerobicYield));
        CHECK(env.nutrient().total() == Approx(nutrientBefore));
        CHECK(env.waste().total() == Approx(wasteBefore - t.uptakeMultiplier));
    }

    SECTION("a fermenter never takes oxygen, however much is there") {
        env.oxygen().fill(2.0f);  // below its tolerance, so this costs it nothing
        const double oxygenBefore = env.oxygen().total();
        const SpeciesTraits& t = traitsOf(Species::Fermenter);
        strata::sim::Entity e = env.spawn(Species::Fermenter, 8.5f, 8.5f, 500.0f, 1);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(500.0f - 1.0f + t.uptakeMultiplier * t.anaerobicYield));
        CHECK(env.oxygen().total() == Approx(oxygenBefore));
        CHECK(env.waste().total() == Approx(wasteBefore + t.uptakeMultiplier * t.wasteFraction));
    }
}

TEST_CASE("Waste above a species' tolerance costs it energy", "[strata][species]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    MicroEnvironment env(c);
    const SpeciesTraits& t = traitsOf(Species::Aerobe);

    SECTION("above the tolerance") {
        env.waste().fill(t.wasteTolerance + 4.0f);
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 1);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(500.0f - 1.0f - 4.0f * c.wasteToxicity));
    }

    SECTION("at the tolerance") {
        env.waste().fill(t.wasteTolerance);
        strata::sim::Entity e = env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 1);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(499.0f));
    }

    SECTION("a cross-feeder has no limit") {
        env.waste().fill(100.0f);
        strata::sim::Entity e = env.spawn(Species::CrossFeeder, 8.5f, 8.5f, 500.0f, 1);
        env.step();
        CHECK(env.world().get<Cell>(e)->energy == Approx(499.0f));
    }
}

TEST_CASE("A cell that runs out of energy dies before it can eat", "[strata][death]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    c.feedingEnabled = true;
    MicroEnvironment env(c);
    env.nutrient().fill(10.0f);
    const double before = env.nutrient().total();
    env.spawn(Species::Aerobe, 8.5f, 8.5f, 2.0f, 4);  // a meal would otherwise carry it past zero
    env.step();
    CHECK(env.world().size() == 0);
    CHECK(env.nutrient().total() - before == Approx(c.biomatterYieldPerDeath));
}

TEST_CASE("A cell in a biofilm stays put and pays half its metabolic cost", "[strata][biofilm]") {
    Config c = stillChamber();
    c.metabolismEnabled = true;
    c.detachWhenStarving = 0.0f;
    MicroEnvironment env(c);
    strata::sim::Entity settled = env.spawn(Species::Fermenter, 8.5f, 8.5f, 500.0f, 2);
    env.world().get<Body>(settled)->attached = 1;
    strata::sim::Entity swimmer = env.spawn(Species::Fermenter, 4.5f, 8.5f, 500.0f, 2);
    env.run(10);

    CHECK(env.world().get<Body>(settled)->x == 8.5f);
    CHECK(env.world().get<Body>(settled)->y == 8.5f);
    CHECK(env.world().get<Cell>(settled)->energy == Approx(500.0f - 10.0f * 2.0f * c.attachedMetabolismFactor));
    CHECK(env.world().get<Cell>(swimmer)->energy == Approx(500.0f - 10.0f * 2.0f));
    CHECK(env.stats().attached == 1);
}

TEST_CASE("A starving biofilm cell disperses; one with food stays", "[strata][biofilm]") {
    Config c = stillChamber();
    c.detachWhenStarving = 1.0f;  // every chance it gets
    MicroEnvironment env(c);
    strata::sim::Entity e = settle(env, 8.5f, 8.5f, 500.0f);

    SECTION("food under 1") {
        env.nutrient().fill(0.99f);
        env.step();
        CHECK(env.world().get<Body>(e)->attached == 0);
    }

    SECTION("food of 1 or more") {
        env.nutrient().fill(1.0f);
        env.run(20);
        CHECK(env.world().get<Body>(e)->attached == 1);
    }
}

TEST_CASE("A crowd of fermenters forms a biofilm in open water; a lone one does not", "[strata][biofilm]") {
    Config c = stillChamber(64);
    c.detachWhenStarving = 0.0f;
    const int ticks = 60;  // at 0.25 cells a tick nobody can reach a wall 32 cells away

    MicroEnvironment crowd(c);
    for (int i = 0; i < 40; ++i) crowd.spawn(Species::Fermenter, 32.5f, 32.5f, 500.0f, 1);
    crowd.run(ticks);
    CHECK(crowd.stats().attached > 0);

    MicroEnvironment lone(c);
    lone.spawn(Species::Fermenter, 32.5f, 32.5f, 500.0f, 1);
    lone.run(ticks);
    CHECK(lone.stats().attached == 0);
}

TEST_CASE("Only fermenters settle, however crowded the others get", "[strata][biofilm]") {
    MicroEnvironment env(stillChamber());
    for (int i = 0; i < 40; ++i) {
        env.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 1);
        env.spawn(Species::CrossFeeder, 8.5f, 8.5f, 500.0f, 1);
    }
    env.run(100);  // long enough to reach the walls as well
    CHECK(env.stats().attached == 0);
}

TEST_CASE("A daughter lands one cell away and inherits its parent's biofilm", "[strata][reproduction]") {
    Config c = stillChamber();
    c.reproductionEnabled = true;
    c.detachWhenStarving = 0.0f;
    MicroEnvironment env(c);
    settle(env, 8.5f, 8.5f, 1000.0f);
    env.step();
    REQUIRE(env.world().size() == 2);

    int daughters = 0;
    env.world().each<Body, Cell>([&](strata::sim::Entity, const Body& body, const Cell& cell) {
        CHECK(body.attached == 1);
        if (cell.generation == 0) return;
        ++daughters;
        CHECK(cell.generation == 1);
        CHECK(std::hypot(body.x - 8.5f, body.y - 8.5f) == Approx(1.0f));
    });
    CHECK(daughters == 1);
}

TEST_CASE("Crowding keeps a daughter in the parent's cell, then stops fission", "[strata][reproduction]") {
    Config c = stillChamber();
    c.reproductionEnabled = true;
    c.detachWhenStarving = 0.0f;
    MicroEnvironment env(c);
    crowdAround(env, 8, 8);

    SECTION("every neighbour is full: the daughter stays with its parent") {
        settle(env, 8.5f, 8.5f, 1000.0f);
        env.step();
        REQUIRE(env.stats().births == 1);
        int atCentre = 0;
        env.world().each<Body>([&](strata::sim::Entity, const Body& body) {
            if (body.x == 8.5f && body.y == 8.5f) ++atCentre;
        });
        CHECK(atCentre == 2);
    }

    SECTION("the parent's cell is full too: no fission, and the energy is kept") {
        for (int i = 0; i < c.crowdingLimit - 1; ++i) settle(env, 8.5f, 8.5f, 100.0f);
        strata::sim::Entity parent = settle(env, 8.5f, 8.5f, 1000.0f);
        env.step();
        CHECK(env.stats().births == 0);
        CHECK(env.world().get<Cell>(parent)->energy == 1000.0f);
    }
}

TEST_CASE("maxPopulation stops fission once births would pass it", "[strata][reproduction]") {
    Config c = stillChamber();
    c.reproductionEnabled = true;
    c.maxPopulation = 4;
    MicroEnvironment env(c);
    env.spawn(Species::Aerobe, 4.5f, 4.5f, 1000.0f, 1);
    env.spawn(Species::Aerobe, 8.5f, 8.5f, 1000.0f, 1);
    env.spawn(Species::Aerobe, 12.5f, 12.5f, 1000.0f, 1);
    env.run(5);

    CHECK(env.world().size() == 4);
    CHECK(env.stats().births == 1);
    int undivided = 0;
    env.world().each<Cell>([&](strata::sim::Entity, const Cell& cell) {
        if (cell.energy == 1000.0f) ++undivided;
    });
    CHECK(undivided == 2);
}

TEST_CASE("Nothing swims or divides into a grain or out of the chamber", "[strata][setup]") {
    Config c;
    c.width = 48;
    c.height = 48;
    c.grainCount = 12;
    c.initialPerSpecies = 60;
    MicroEnvironment env(c);
    for (int t = 0; t < 200; ++t) {
        env.step();
        env.world().each<Body>([&](strata::sim::Entity, const Body& body) {
            REQUIRE_FALSE(env.grains().blocked(static_cast<int>(std::floor(body.x)), static_cast<int>(std::floor(body.y))));
        });
    }
    CHECK(env.stats().births > 0);  // daughters were placed too
}

TEST_CASE("stats and summary count what is in the chamber", "[strata][stats]") {
    MicroEnvironment env(stillChamber());
    env.spawn(Species::Aerobe, 2.5f, 2.5f, 100.0f, 1);
    env.spawn(Species::Aerobe, 4.5f, 2.5f, 200.0f, 1);
    settle(env, 8.5f, 8.5f, 300.0f);
    env.spawn(Species::CrossFeeder, 12.5f, 2.5f, 400.0f, 1);
    env.nutrient().set(1, 1, 7.0f);
    env.waste().set(1, 1, 8.0f);
    env.oxygen().set(1, 1, 9.0f);

    const Stats s = env.stats();
    CHECK(s.population() == 4);
    CHECK(s.alive[static_cast<int>(Species::Aerobe)] == 2);
    CHECK(s.alive[static_cast<int>(Species::Fermenter)] == 1);
    CHECK(s.alive[static_cast<int>(Species::CrossFeeder)] == 1);
    CHECK(s.attached == 1);
    CHECK(s.organismEnergy == Approx(1000.0));
    CHECK(env.summary() == "tick 0 | aerobes 2, fermenters 1, cross-feeders 1 (biofilm 1) | nutrient 7, waste 8, oxygen 9");
}

TEST_CASE("The state hash changes with every field and every entity", "[strata][determinism]") {
    Config c = stillChamber();
    MicroEnvironment a(c), b(c);
    a.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 1);
    strata::sim::Entity e = b.spawn(Species::Aerobe, 8.5f, 8.5f, 500.0f, 1);
    REQUIRE(a.stateHash() == b.stateHash());

    SECTION("nutrient") { b.nutrient().set(3, 3, 0.5f); }
    SECTION("waste") { b.waste().set(3, 3, 0.5f); }
    SECTION("oxygen") { b.oxygen().set(3, 3, 0.5f); }
    SECTION("a cell's energy") { b.world().get<Cell>(e)->energy = 501.0f; }
    SECTION("a cell's position") { b.world().get<Body>(e)->x = 8.75f; }
    SECTION("a cell's biofilm state") { b.world().get<Body>(e)->attached = 1; }

    CHECK(a.stateHash() != b.stateHash());
}
