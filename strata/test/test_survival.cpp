// Catch2 tests for survival mode (strata/src/survival.cpp): embodying a cell,
// steering it, the toxin burst, the amoebae and the lineage.
#include "catch2.hpp"

#include <cmath>

#include "micro_environment.h"

using namespace mbstrata;
using strata::sim::Entity;

namespace {

// An empty chamber with every rule off and no amoebae arriving on their own;
// a test turns on what it is about.
Config quietChamber(int size = 32) {
    Config c;
    c.width = size;
    c.height = size;
    c.initialPerSpecies = 0;
    c.grainCount = 0;
    c.nutrientSources = 0;
    c.initialNutrient = 0.0f;
    c.oxygenSurface = 0.0f;
    c.fieldsEnabled = false;
    c.feedingEnabled = false;
    c.metabolismEnabled = false;
    c.reproductionEnabled = false;
    c.predatorsAtStart = 0;
    c.predatorWaveTicks = 1000000;
    c.predatorWaveMin = 1000000;
    c.predatorUpkeep = 0.0f;
    return c;
}

Entity embodyNew(MicroEnvironment& env, Species sp, float x, float y, float energy = 800.0f) {
    const Entity e = env.spawn(sp, x, y, energy, 1);
    REQUIRE(env.embody(e));
    return e;
}

const Body& bodyOf(const MicroEnvironment& env, Entity e) { return *env.world().get<Body>(e); }
const Cell& cellOf(const MicroEnvironment& env, Entity e) { return *env.world().get<Cell>(e); }

}  // namespace

TEST_CASE("Nothing about survival mode runs until a cell is embodied", "[strata][survival]") {
    Config c;
    c.width = c.height = 48;
    c.initialPerSpecies = 40;
    MicroEnvironment a(c), b(c);
    // Asking which cell is nearest only reads.
    CHECK(b.nearestCell(24.0f, 24.0f).valid());
    CHECK_FALSE(b.continueLineage());
    b.release();
    b.setPlayerInput(PlayerInput{1.0f, 0.0f, true, true});
    for (int t = 0; t < 100; ++t) {
        a.step();
        b.step();
        REQUIRE(a.stateHash() == b.stateHash());
    }
    CHECK_FALSE(b.player().started);
    CHECK(b.predators().empty());
    CHECK(b.lineageAlive() == 0);
}

TEST_CASE("Embodying a cell takes control of it and releases the amoebae", "[strata][survival]") {
    Config c = quietChamber();
    c.predatorsAtStart = 3;
    MicroEnvironment env(c);
    CHECK_FALSE(env.embody(Entity{}));
    const Entity e = env.spawn(Species::Fermenter, 10.5f, 10.5f, 200.0f, 2);
    CHECK(env.nearestCell(0.0f, 0.0f, static_cast<int>(Species::Fermenter)) == e);
    CHECK_FALSE(env.nearestCell(0.0f, 0.0f, static_cast<int>(Species::Aerobe)).valid());
    REQUIRE(env.embody(e));

    const Player& p = env.player();
    CHECK(p.started);
    CHECK(p.embodied);
    CHECK(p.entity == e);
    CHECK(p.species == Species::Fermenter);
    CHECK(p.lives == 1);
    CHECK(env.predators().size() == 3);
    CHECK(cellOf(env, e).energy == c.embodyEnergy);  // topped up from 200
    CHECK(cellOf(env, e).lineage == 1);
    CHECK(env.lineageAlive() == 1);
    for (const Predator& pr : env.predators()) CHECK_FALSE(env.grains().blocked(static_cast<int>(pr.x), static_cast<int>(pr.y)));

    SECTION("a cell with more energy keeps it") {
        const Entity rich = env.spawn(Species::Aerobe, 20.5f, 20.5f, 950.0f, 1);
        REQUIRE(env.embody(rich));
        CHECK(cellOf(env, rich).energy == 950.0f);
        CHECK(env.predators().size() == 3);  // amoebae arrive only the first time
    }
}

TEST_CASE("The embodied cell swims where it is steered, at its species' speed", "[strata][survival]") {
    MicroEnvironment env(quietChamber());
    const Entity e = embodyNew(env, Species::Aerobe, 10.5f, 10.5f);
    const float speed = traitsOf(Species::Aerobe).speed;

    SECTION("no input: it holds still") {
        env.run(10);
        CHECK(bodyOf(env, e).x == 10.5f);
        CHECK(bodyOf(env, e).y == 10.5f);
    }
    SECTION("steering right") {
        env.setPlayerInput(PlayerInput{1.0f, 0.0f, false, false});
        env.run(5);
        CHECK(bodyOf(env, e).x == Approx(10.5f + 5 * speed));
        CHECK(bodyOf(env, e).y == 10.5f);
        CHECK(bodyOf(env, e).hx == 1.0f);
    }
    SECTION("a long input is capped at full throttle; a short one is a slower swim") {
        env.setPlayerInput(PlayerInput{0.0f, 9.0f, false, false});
        env.step();
        CHECK(bodyOf(env, e).y == Approx(10.5f + speed));
        env.setPlayerInput(PlayerInput{0.0f, 0.5f, false, false});
        env.step();
        CHECK(bodyOf(env, e).y == Approx(10.5f + 1.5f * speed));
    }
    SECTION("dashing doubles the speed") {
        env.setPlayerInput(PlayerInput{-1.0f, 0.0f, true, false});
        env.run(4);
        CHECK(bodyOf(env, e).x == Approx(10.5f - 4 * speed * env.config().dashMultiplier));
        CHECK(env.player().dashing);
    }
    SECTION("it slides along the chamber wall instead of stopping") {
        env.world().get<Body>(e)->x = 0.3f;
        env.setPlayerInput(PlayerInput{-1.0f, 1.0f, false, false});
        env.step();
        CHECK(bodyOf(env, e).x == 0.3f);
        CHECK(bodyOf(env, e).y > 10.5f);
    }
}

TEST_CASE("A fermenter under control never settles into a biofilm", "[strata][survival]") {
    Config c = quietChamber();
    MicroEnvironment env(c);
    const Entity e = embodyNew(env, Species::Fermenter, 0.5f, 0.5f);  // touching two walls
    env.run(200);
    CHECK(bodyOf(env, e).attached == 0);
}

TEST_CASE("Dashing costs energy, holding still does not", "[strata][survival]") {
    Config c = quietChamber();
    c.metabolismEnabled = true;
    MicroEnvironment env(c);
    const Entity e = embodyNew(env, Species::CrossFeeder, 16.5f, 16.5f);
    env.setPlayerInput(PlayerInput{1.0f, 0.0f, false, false});
    env.step();
    const float swimming = 800.0f - cellOf(env, e).energy;
    env.setPlayerInput(PlayerInput{-1.0f, 0.0f, true, false});
    const float before = cellOf(env, e).energy;
    env.step();
    CHECK(before - cellOf(env, e).energy == Approx(swimming + c.dashCost));
    CHECK(env.player().dashDrain == c.dashCost);
}

TEST_CASE("A toxin burst hurts rivals and amoebae in range and spares kin", "[strata][survival]") {
    Config c = quietChamber();
    MicroEnvironment env(c);
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 800.0f);
    const Entity rival = env.spawn(Species::Fermenter, 18.5f, 16.5f, 300.0f, 1);
    const Entity weak = env.spawn(Species::CrossFeeder, 16.5f, 18.5f, 100.0f, 1);
    const Entity far = env.spawn(Species::Fermenter, 28.5f, 28.5f, 300.0f, 1);
    const Entity kin = env.spawn(Species::Aerobe, 15.5f, 16.5f, 300.0f, 1);
    env.world().get<Cell>(kin)->lineage = 1;
    env.addPredator(16.5f, 13.5f, 600.0f);
    env.addPredator(16.5f, 25.0f, 600.0f);  // outside the radius

    env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
    env.step();
    CHECK(cellOf(env, me).energy == 800.0f - c.toxinCost);
    CHECK_FALSE(env.world().alive(rival));
    CHECK_FALSE(env.world().alive(weak));
    CHECK(env.player().rivalsKilled == 2);
    CHECK(cellOf(env, far).energy == 300.0f);
    CHECK(cellOf(env, kin).energy == 300.0f);
    REQUIRE(env.predators().size() == 2);
    CHECK(env.predators()[0].energy == Approx(200.0f));  // 600 - 400
    CHECK(env.predators()[0].stunTicks > 0);
    CHECK(env.predators()[1].energy == 600.0f);
    CHECK(env.predators()[1].stunTicks == 0);
    CHECK(env.player().burst);
    CHECK(env.player().toxinReady == static_cast<uint32_t>(c.toxinCooldown));

    SECTION("the input is one burst; another needs a new press and the cooldown") {
        const float e0 = cellOf(env, me).energy;
        env.run(5);
        CHECK(cellOf(env, me).energy == e0);
        env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
        env.step();
        CHECK(cellOf(env, me).energy == e0);  // still cooling down
    }
}

TEST_CASE("A burst stuns an amoeba and two kill it", "[strata][survival]") {
    Config c = quietChamber();
    c.toxinStun = 200;  // still stunned when the toxin is ready again
    MicroEnvironment env(c);
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 900.0f);
    env.addPredator(16.5f, 13.5f, 700.0f);
    env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
    env.step();
    REQUIRE(env.predators().size() == 1);
    const Predator stunned = env.predators()[0];
    CHECK(stunned.energy == Approx(300.0f));
    CHECK(stunned.stunTicks == static_cast<uint32_t>(c.toxinStun) - 1);
    env.step();
    CHECK(env.predators()[0].x == stunned.x);  // a stunned amoeba does not move
    CHECK(env.predators()[0].y == stunned.y);
    const uint32_t cooldown = env.player().toxinReady;
    for (uint32_t t = 0; t < cooldown; ++t) env.step();
    REQUIRE(env.player().toxinReady == 0);
    env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
    env.step();
    CHECK(env.predators().empty());
    CHECK(env.player().amoebaeKilled == 1);
    CHECK(env.world().alive(me));
}

TEST_CASE("An amoeba engulfs prey it reaches, the player included", "[strata][survival]") {
    Config c = quietChamber();
    MicroEnvironment env(c);
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 800.0f);

    SECTION("an ordinary cell") {
        // A hungry amoeba eats the nearest cell, which is not the player.
        const Entity prey = env.spawn(Species::Fermenter, 4.5f, 4.5f, 300.0f, 1);
        env.addPredator(4.5f, 5.3f, 100.0f);
        env.step();
        CHECK_FALSE(env.world().alive(prey));
        CHECK(env.predators()[0].energy == Approx(100.0f + 300.0f * c.predatorGain + 30.0f));
        CHECK(env.predators()[0].digestTicks == static_cast<uint32_t>(c.predatorDigest));
        CHECK(env.stats().deaths == 1);
        CHECK(env.player().embodied);
    }
    SECTION("the player") {
        env.addPredator(16.5f, 18.5f, 600.0f);
        env.run(10);
        CHECK_FALSE(env.world().alive(me));
        CHECK_FALSE(env.player().embodied);
        CHECK(env.player().cause == DeathCause::Eaten);
        CHECK(std::string(deathCauseText(env.player().cause)) == "engulfed by an amoeba");
        CHECK(env.player().bestTicks > 0);
        CHECK_FALSE(env.continueLineage());  // no descendants
    }
}

TEST_CASE("A sated amoeba ignores other prey and comes for the player", "[strata][survival]") {
    Config c = quietChamber(64);
    MicroEnvironment env(c);
    embodyNew(env, Species::Aerobe, 50.5f, 10.5f);
    const Entity bait = env.spawn(Species::Fermenter, 10.5f, 12.0f, 300.0f, 1);
    env.addPredator(10.5f, 10.5f, c.predatorEnergy + 100.0f);
    const float d0 = std::hypot(env.predators()[0].x - 50.5f, env.predators()[0].y - 10.5f);
    env.run(60);
    CHECK(env.world().alive(bait));
    CHECK(std::hypot(env.predators()[0].x - 50.5f, env.predators()[0].y - 10.5f) < d0 - 5.0f);
}

TEST_CASE("Deaths are put down to what drained the most", "[strata][survival]") {
    Config c = quietChamber();
    c.metabolismEnabled = true;
    MicroEnvironment env(c);
    SECTION("oxygen poisons a fermenter") {
        embodyNew(env, Species::Fermenter, 16.5f, 16.5f, 1.0f);
        env.world().get<Cell>(env.player().entity)->energy = 5.0f;
        env.oxygen().set(16, 16, 50.0f);
        env.step();
        CHECK(env.player().cause == DeathCause::Oxygen);
    }
    SECTION("waste poisons an aerobe") {
        embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
        env.world().get<Cell>(env.player().entity)->energy = 5.0f;
        env.waste().set(16, 16, 60.0f);
        env.step();
        CHECK(env.player().cause == DeathCause::Waste);
    }
    SECTION("nothing to eat") {
        embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
        env.world().get<Cell>(env.player().entity)->energy = 0.5f;
        env.step();
        CHECK(env.player().cause == DeathCause::Starved);
    }
}

TEST_CASE("Daughters share the lineage, and a death carries on in one of them", "[strata][survival]") {
    Config c = quietChamber();
    c.reproductionEnabled = true;
    MicroEnvironment env(c);
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 1000.0f);
    env.step();
    CHECK(env.player().divisions == 1);
    CHECK(env.lineageAlive() == 2);
    CHECK(env.player().entity == me);  // the player stays the parent

    env.addPredator(16.5f, 17.0f, 100.0f);
    env.step();  // engulfed (the nearest prey within reach)
    if (env.player().embodied) env.run(5);
    REQUIRE_FALSE(env.player().embodied);
    REQUIRE(env.lineageAlive() >= 1);
    REQUIRE(env.continueLineage());
    CHECK(env.player().embodied);
    CHECK(env.player().entity != me);
    CHECK(env.player().divisions == 1);  // a run's divisions carry over
    CHECK(env.player().lives == 2);
}

TEST_CASE("Letting go hands the cell back to its instincts", "[strata][survival]") {
    Config c = quietChamber();
    c.predatorsAtStart = 2;
    MicroEnvironment env(c);
    const Entity e = embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
    env.run(3);
    env.release();
    CHECK_FALSE(env.player().embodied);
    CHECK(env.player().cause == DeathCause::None);
    CHECK(env.predators().size() == 2);  // the amoebae stay
    env.setPlayerInput(PlayerInput{1.0f, 0.0f, false, false});
    const float x0 = bodyOf(env, e).x;
    env.run(20);
    CHECK(env.world().alive(e));
    CHECK(bodyOf(env, e).x != x0);  // swimming on its own again
}

TEST_CASE("Amoebae keep arriving, faster over time, up to the cap", "[strata][survival]") {
    Config c = quietChamber(48);
    c.predatorWaveTicks = 50;
    c.predatorWaveStep = 10;
    c.predatorWaveMin = 20;
    c.maxPredators = 5;
    MicroEnvironment env(c);
    embodyNew(env, Species::Aerobe, 24.5f, 24.5f);
    env.run(50);  // the first wave lands on tick 50, during the 51st step
    CHECK(env.predators().empty());
    env.run(1);
    CHECK(env.predators().size() == 1);
    env.run(40);  // the next wave comes 40 ticks later, not 50
    CHECK(env.predators().size() == 2);
    env.run(400);
    CHECK(env.predators().size() == 5);
}

TEST_CASE("Survival runs are deterministic for the same inputs", "[strata][survival][determinism]") {
    Config c;
    c.width = c.height = 64;
    c.initialPerSpecies = 60;
    MicroEnvironment a(c), b(c);
    a.run(50);
    b.run(50);
    REQUIRE(a.embody(a.nearestCell(32, 32, 0)));
    REQUIRE(b.embody(b.nearestCell(32, 32, 0)));
    for (int t = 0; t < 400; ++t) {
        a.setPlayerInput(autopilot(a));
        b.setPlayerInput(autopilot(b));
        a.step();
        b.step();
        REQUIRE(a.stateHash() == b.stateHash());
    }
    CHECK_FALSE(a.predators().empty());

    SECTION("and a different input changes the run") {
        MicroEnvironment d(c);
        d.run(50);
        REQUIRE(d.embody(d.nearestCell(32, 32, 0)));
        d.setPlayerInput(PlayerInput{1.0f, 0.0f, true, false});
        d.step();
        MicroEnvironment e(c);
        e.run(50);
        REQUIRE(e.embody(e.nearestCell(32, 32, 0)));
        e.step();
        CHECK(d.stateHash() != e.stateHash());
    }
}

// ---- Mutations

namespace {

// Tops the embodied cell up so the next tick divides it.
void divideOnce(MicroEnvironment& env) {
    env.world().get<Cell>(env.player().entity)->energy = env.config().reproductionEnergyThreshold + 200.0f;
    env.step();
}

Config breedingChamber() {
    Config c = quietChamber();
    c.reproductionEnabled = true;
    c.crowdingLimit = 64;
    return c;
}

}  // namespace

TEST_CASE("Dividing offers three different mutations, and choosing one keeps it", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
    CHECK(env.player().offerCount == 0);
    divideOnce(env);
    REQUIRE(env.player().divisions == 1);
    REQUIRE(env.player().offerCount == kMutationOffer);
    const Player& p = env.player();
    CHECK(p.offer[0] != p.offer[1]);
    CHECK(p.offer[0] != p.offer[2]);
    CHECK(p.offer[1] != p.offer[2]);
    const Mutation picked = p.offer[1];
    CHECK_FALSE(env.chooseMutation(3));
    CHECK(env.chooseMutation(1));
    CHECK(env.player().level(picked) == 1);
    CHECK(env.player().offerCount == 0);
    CHECK_FALSE(env.chooseMutation(0));  // nothing left to choose from
    CHECK(std::string(mutationName(picked)).size() > 0);
    CHECK(std::string(mutationEffect(picked)).size() > 0);
}

TEST_CASE("Divisions while an offer is open queue further offers", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
    divideOnce(env);
    divideOnce(env);
    CHECK(env.player().divisions == 2);
    CHECK(env.player().offersPending == 1);
    REQUIRE(env.chooseMutation(0));
    CHECK(env.player().offerCount == kMutationOffer);  // the queued one
    CHECK(env.player().offersPending == 0);
    REQUIRE(env.chooseMutation(0));
    CHECK(env.player().offerCount == 0);
}

TEST_CASE("Offers never include a maxed mutation and run out when all are maxed", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
    for (int i = 0; i < kMutationCount; ++i)
        for (int l = 0; l < kMutationMaxLevel; ++l) env.mutate(static_cast<Mutation>(i));
    env.mutate(Mutation::Flagella);  // already maxed: no change
    CHECK(env.player().level(Mutation::Flagella) == kMutationMaxLevel);
    // Un-max one by building a fresh run with all but Hardy maxed.
    MicroEnvironment env2(breedingChamber());
    embodyNew(env2, Species::Aerobe, 16.5f, 16.5f);
    for (int i = 0; i < kMutationCount; ++i)
        if (static_cast<Mutation>(i) != Mutation::Hardy)
            for (int l = 0; l < kMutationMaxLevel; ++l) env2.mutate(static_cast<Mutation>(i));
    divideOnce(env2);
    REQUIRE(env2.player().offerCount == 1);
    CHECK(env2.player().offer[0] == Mutation::Hardy);
    divideOnce(env);
    CHECK(env.player().offerCount == 0);
    CHECK(env.player().offersPending == 0);
}

TEST_CASE("Mutation offers are deterministic and leave the chamber's random stream alone", "[strata][survival][mutation][determinism]") {
    Config c = breedingChamber();
    c.initialPerSpecies = 6;
    MicroEnvironment a(c), b(c);
    embodyNew(a, Species::Aerobe, 16.5f, 16.5f);
    embodyNew(b, Species::Aerobe, 16.5f, 16.5f);
    divideOnce(a);
    divideOnce(b);
    REQUIRE(a.player().offerCount == kMutationOffer);
    for (int i = 0; i < kMutationOffer; ++i) CHECK(a.player().offer[static_cast<size_t>(i)] == b.player().offer[static_cast<size_t>(i)]);
    CHECK(a.stateHash() == b.stateHash());
}

TEST_CASE("Mutations change how the embodied cell swims, dashes and fights", "[strata][survival][mutation]") {
    MicroEnvironment env(quietChamber());
    const Entity e = embodyNew(env, Species::Aerobe, 10.5f, 10.5f);
    const float speed = traitsOf(Species::Aerobe).speed;

    SECTION("extra flagella: 20% faster per level") {
        env.mutate(Mutation::Flagella);
        env.mutate(Mutation::Flagella);
        env.setPlayerInput(PlayerInput{1.0f, 0.0f, false, false});
        env.run(5);
        CHECK(bodyOf(env, e).x == Approx(10.5f + 5 * speed * 1.4f));
    }
    SECTION("lean sprint: dashing costs 30% less per level") {
        Config c = quietChamber();
        c.metabolismEnabled = true;
        MicroEnvironment m(c);
        const Entity me = embodyNew(m, Species::Aerobe, 10.5f, 10.5f, 800.0f);
        m.mutate(Mutation::LeanDash);
        const float before = cellOf(m, me).energy;
        m.setPlayerInput(PlayerInput{1.0f, 0.0f, true, false});
        m.step();
        const float rate = static_cast<float>(cellOf(m, me).metabolicRate);
        CHECK(before - cellOf(m, me).energy == Approx(rate + c.dashCost * 0.7f));
    }
    SECTION("potent toxin: reaches farther and hits harder") {
        const Config& c = env.config();
        env.mutate(Mutation::PotentToxin);
        // Just outside the plain radius, inside the mutated one.
        const Entity rival = env.spawn(Species::Fermenter, 10.5f + c.toxinRadius + 0.5f, 10.5f, 1000.0f, 1);
        env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
        env.step();
        CHECK(cellOf(env, rival).energy == Approx(1000.0f - c.toxinDamage * 1.25f));
    }
    SECTION("quick reload: the toxin recharges sooner") {
        env.mutate(Mutation::QuickReload);
        env.setPlayerInput(PlayerInput{0.0f, 0.0f, false, true});
        env.step();
        CHECK(env.player().toxinReady == static_cast<uint32_t>(static_cast<float>(env.config().toxinCooldown) * 0.75f));
    }
}

TEST_CASE("A thick membrane survives an engulfing, once per level, and regrows on division", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 500.0f);
    env.mutate(Mutation::Membrane);
    REQUIRE(env.player().membrane == 1);
    env.addPredator(16.5f, 17.3f, 600.0f);
    env.step();
    CHECK(env.world().alive(me));
    CHECK(env.player().embodied);
    CHECK(env.player().membrane == 0);
    CHECK(env.predators()[0].stunTicks > 0);
    // A division regrows it.
    divideOnce(env);
    CHECK(env.player().membrane == 1);
}

TEST_CASE("Mutations stay with the lineage when a death carries on", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    const Entity me = embodyNew(env, Species::Aerobe, 16.5f, 16.5f, 1000.0f);
    env.mutate(Mutation::Hardy);
    env.step();  // divides
    REQUIRE(env.lineageAlive() == 2);
    REQUIRE(env.chooseMutation(0));
    env.world().get<Cell>(me)->energy = -1.0f;  // starves this tick
    env.step();
    REQUIRE_FALSE(env.player().embodied);
    REQUIRE(env.continueLineage());
    CHECK(env.player().level(Mutation::Hardy) >= 1);
}

TEST_CASE("The bot picks a membrane first when one is offered", "[strata][survival][mutation]") {
    MicroEnvironment env(breedingChamber());
    embodyNew(env, Species::Aerobe, 16.5f, 16.5f);
    CHECK(autopilotMutation(env) == -1);
    for (int i = 0; i < 6 && env.player().level(Mutation::Membrane) == 0; ++i) {
        divideOnce(env);
        const int pick = autopilotMutation(env);
        REQUIRE(pick >= 0);
        bool offered = false;
        for (int j = 0; j < env.player().offerCount; ++j)
            offered = offered || env.player().offer[static_cast<size_t>(j)] == Mutation::Membrane;
        if (offered) CHECK(env.player().offer[static_cast<size_t>(pick)] == Mutation::Membrane);
        env.chooseMutation(pick);
    }
}
