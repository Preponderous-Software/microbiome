#ifndef MicroEnvironment_h
#define MicroEnvironment_h

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "strata/sim/field.hpp"
#include "strata/sim/registry.hpp"
#include "strata/sim/rng.hpp"

/**
 * A continuous-space micro-environment simulated with Strata's simulation
 * module (strata::sim): diffusing nutrient, waste and oxygen fields on a grid
 * with solid grains in it, and microorganisms as Strata entities that sense,
 * swim, eat, excrete, divide and die.
 *
 * Deterministic: everything random comes from one seeded strata::sim::Rng,
 * entities are visited in Strata's fixed registry order, the tick is fixed and
 * nothing reads a clock. The same Config gives a bit-identical stateHash()
 * after the same number of ticks.
 *
 * The console app, mb_webapp and the browser build do not use this class; see
 * docs/STRATA.md.
 * @author Daniel McCoy Stephenson
 */
namespace mbstrata {

enum class Species : uint8_t {
    Aerobe = 0,      // swims up the nutrient gradient; needs oxygen for a full yield
    Fermenter = 1,   // anaerobe: oxygen costs it energy; settles into biofilms on surfaces
    CrossFeeder = 2  // eats the others' waste (syntrophy) and swims up the waste gradient
};
constexpr int kSpeciesCount = 3;

enum class Food : uint8_t { Nutrient = 0, Waste = 1 };

struct SpeciesTraits {
    const char* name;
    float speed;              // cells per tick while swimming
    Food food;                // which field it eats
    float uptakeMultiplier;   // most it eats per tick = metabolicRate * uptakeMultiplier
    float aerobicYield;       // energy per unit eaten with enough oxygen
    float anaerobicYield;     // energy per unit eaten without
    float oxygenPerUnit;      // oxygen used per unit eaten aerobically (0: never respires)
    float wasteFraction;      // fraction of what it eats excreted into the waste field
    float oxygenTolerance;    // oxygen above this costs energy (negative: no limit)
    float wasteTolerance;     // waste above this costs energy (negative: no limit)
    float attachChance;       // per-tick chance to attach to a surface it touches
    float color[3];           // linear RGB for drawing
};

struct Config {
    int width = 128;
    int height = 128;
    uint64_t seed = 1;
    int initialPerSpecies = 300;
    int maxPopulation = 20000;

    // Grains (obstacles): solid discs biofilms can grow on.
    int grainCount = 34;
    float grainRadiusMin = 2.5f;
    float grainRadiusMax = 7.5f;

    // Fields. Diffusion rates are per substep, in [0, 0.25].
    float initialNutrient = 12.0f;
    int nutrientSources = 12;         // seeps that release nutrient every tick
    float nutrientSourceRate = 150.0f; // per seep per tick
    float nutrientDiffusion = 0.22f;
    float nutrientDecay = 0.0003f;
    float wasteDiffusion = 0.20f;
    float wasteDecay = 0.004f;
    float oxygenDiffusion = 0.24f;
    float oxygenDecay = 0.0f;
    float oxygenSurface = 10.0f;      // concentration held at the top edge (the air interface)
    int diffusionSubsteps = 3;        // nutrient and waste diffuse this many stable steps per tick
    int oxygenSubsteps = 8;           // oxygen diffuses much faster in water than sugars do

    // Behaviour switches (tests turn pieces off to isolate one mechanic).
    bool chemotaxisEnabled = true;
    bool feedingEnabled = true;
    bool reproductionEnabled = true;
    bool metabolismEnabled = true;
    bool fieldsEnabled = true;        // sources, diffusion and decay

    // Traits kept from the console simulation (src/header/microbiome.h,
    // src/header/microorganism.h).
    int maxInitialEnergy = 1000;
    int minimumInitialEnergy = 100;
    int maxMetabolicRateFactor = 4;
    float reproductionEnergyThreshold = 900.0f;
    float fissionCost = 100.0f;
    float biomatterYieldPerDeath = 60.0f; // released into the nutrient field where a cell dies
    int chemotaxisBiasPercent = 80;

    // Run and tumble.
    float tumbleWhenImproving = 0.08f;
    float tumbleWhenWorsening = 0.45f;
    int crowdingLimit = 4;            // most cells in one grid cell before fission is suppressed
    int quorumThreshold = 3;          // neighbours that switch a fermenter into a biofilm
    float attachedMetabolismFactor = 0.5f;
    float detachWhenStarving = 0.02f; // per-tick chance a biofilm cell disperses where its food is under 1
    float oxygenToxicity = 1.5f;      // energy per tick per unit of oxygen above tolerance
    float wasteToxicity = 0.8f;       // energy per tick per unit of waste above tolerance

    // Survival mode (docs/STRATA.md, "Survival"): only once someone embodies
    // a cell. Until then none of this is used and a run is unchanged.
    int predatorsAtStart = 3;         // amoebae released when survival starts
    int maxPredators = 8;
    float predatorSpeed = 0.40f;      // cells per tick: aerobes outrun them, fermenters must dash or fight
    float predatorSense = 12.0f;      // how far an amoeba notices prey
    float predatorPlayerBias = 0.5f;  // the embodied cell counts as this much nearer (it smells richer)
    float predatorReach = 1.1f;       // engulfs prey this close
    float predatorEnergy = 600.0f;
    float predatorUpkeep = 1.5f;      // energy per tick
    float predatorGain = 0.35f;       // share of the prey's energy an amoeba keeps
    int predatorDigest = 30;          // ticks an amoeba sits still digesting each meal
    float predatorDivide = 2000.0f;   // divides in two at this energy
    int predatorWaveTicks = 600;      // a new amoeba drifts in this often at first...
    int predatorWaveMin = 200;        // ...shrinking by predatorWaveStep per wave down to this
    int predatorWaveStep = 40;
    float dashMultiplier = 2.0f;      // speed while dashing (flagellar sprint)
    float dashCost = 2.0f;            // energy per tick of dashing
    float embodyEnergy = 700.0f;      // an embodied cell starts with at least this much
    float toxinCost = 120.0f;         // a burst of bacteriocin
    float toxinRadius = 4.5f;
    float toxinDamage = 400.0f;       // to every cell outside your lineage and every amoeba in range
    int toxinCooldown = 60;           // ticks
    int toxinStun = 45;               // ticks an amoeba hit by toxin stops moving
};

const SpeciesTraits& traitsOf(Species species);

// Components of a microorganism entity.
struct Body {
    float x = 0, y = 0;       // position in cells; cell (i, j) spans [i, i+1) x [j, j+1)
    float hx = 1, hy = 0;     // unit heading
    uint8_t attached = 0;     // 1: part of a biofilm, does not swim
};

struct Cell {
    float energy = 0;
    int32_t metabolicRate = 1;
    Species species = Species::Aerobe;
    uint32_t timesEaten = 0;
    uint32_t ageTicks = 0;
    uint32_t generation = 0;
    uint32_t lineage = 0;     // survival mode: 1 for the embodied cell and its descendants
};

// A predatory amoeba (survival mode only): hunts and engulfs microorganisms.
struct Predator {
    float x = 0, y = 0;
    float hx = 1, hy = 0;
    float energy = 0;
    uint32_t stunTicks = 0;
    uint32_t digestTicks = 0;
    uint32_t id = 0;
};

// What the player asks of the embodied cell this tick. (dx, dy) is a
// direction in chamber cells (x right, y down the chamber); its length,
// capped at 1, is the throttle.
struct PlayerInput {
    float dx = 0, dy = 0;
    bool dash = false;
    bool toxin = false;       // fire a burst if the cooldown allows
};

enum class DeathCause : uint8_t { None = 0, Starved, Oxygen, Waste, Eaten };
const char* deathCauseText(DeathCause cause);

// Heritable mutations (survival mode). Each time the embodied cell divides,
// three are offered and one is kept; they stay with the lineage for the run.
// Each can be taken up to kMutationMaxLevel times.
enum class Mutation : uint8_t { Flagella = 0, LeanDash, PotentToxin, QuickReload, Membrane, Hardy, Uptake };
constexpr int kMutationCount = 7;
constexpr int kMutationMaxLevel = 3;
constexpr int kMutationOffer = 3;
const char* mutationName(Mutation m);
// What one more level does, e.g. "swim +20%".
const char* mutationEffect(Mutation m);

struct Player {
    bool started = false;     // survival mode has begun (amoebae exist)
    bool embodied = false;    // a cell is under control right now
    strata::sim::Entity entity;
    Species species = Species::Aerobe;
    uint64_t sinceTick = 0;   // when this life began
    uint64_t diedTick = 0;
    float lastX = 0, lastY = 0; // where the embodied cell was last seen
    float energy = 0;
    int divisions = 0;        // fissions in this run (all lives)
    int amoebaeKilled = 0;
    int rivalsKilled = 0;
    int lives = 0;            // cells embodied in this run
    uint64_t bestTicks = 0;   // longest single life
    uint32_t toxinReady = 0;  // ticks until the next burst
    DeathCause cause = DeathCause::None;
    bool dashing = false;
    // Drains on the embodied cell in its last tick, for the HUD's warnings.
    float oxygenDrain = 0, wasteDrain = 0, dashDrain = 0, meal = 0;
    // The latest toxin burst, for drawing it.
    float burstX = 0, burstY = 0;
    uint64_t burstTick = 0;
    bool burst = false;
    // Mutations taken so far (levels), the offer waiting for a choice, and
    // divisions still owed an offer after this one.
    std::array<uint8_t, kMutationCount> mutations{};
    std::array<Mutation, kMutationOffer> offer{};
    int offerCount = 0;
    int offersPending = 0;
    int membrane = 0;         // engulfings the membrane can still shrug off
    int level(Mutation m) const { return mutations[static_cast<size_t>(m)]; }
};

using World = strata::sim::Registry<Body, Cell>;

struct Stats {
    uint64_t tick = 0;
    std::array<int, kSpeciesCount> alive{};
    int attached = 0;
    uint64_t births = 0;
    uint64_t deaths = 0;
    double nutrientTotal = 0;
    double wasteTotal = 0;
    double oxygenTotal = 0;
    double organismEnergy = 0;
    int population() const { return alive[0] + alive[1] + alive[2]; }
};

class MicroEnvironment {
    public:
        explicit MicroEnvironment(const Config& config);

        void step();
        void run(uint64_t ticks);

        // Adds one microorganism; returns the entity. Positions in blocked
        // cells are moved to the nearest open cell along +x.
        strata::sim::Entity spawn(Species species, float x, float y, float energy, int metabolicRate);

        const Config& config() const { return config_; }
        const strata::sim::Mask2D& grains() const { return grains_; }
        const strata::sim::Field2D& nutrient() const { return nutrient_; }
        const strata::sim::Field2D& waste() const { return waste_; }
        const strata::sim::Field2D& oxygen() const { return oxygen_; }
        strata::sim::Field2D& nutrient() { return nutrient_; }
        strata::sim::Field2D& waste() { return waste_; }
        strata::sim::Field2D& oxygen() { return oxygen_; }
        World& world() { return world_; }
        const World& world() const { return world_; }
        const std::vector<std::array<int, 2>>& sources() const { return sources_; }

        Stats stats() const;
        uint64_t tick() const { return tick_; }
        // Fingerprint of the whole state: tick, RNG, fields and every entity.
        uint64_t stateHash() const;
        std::string summary() const;

        // ---- Survival mode: embody one cell and keep it alive.
        // The living cell nearest (x, y), of `species` (< 0: any); invalid if none.
        strata::sim::Entity nearestCell(float x, float y, int species = -1) const;
        // Takes control of `e`; the first time, releases the amoebae. False if `e` is not alive.
        bool embody(strata::sim::Entity e);
        // Embodies the descendant nearest where the last life ended. False if none is left.
        bool continueLineage();
        // Hands the cell back to its instincts (amoebae stay).
        void release();
        void setPlayerInput(const PlayerInput& input) { input_ = input; }
        // Keeps offer[index] of the pending mutation offer. False if there is none.
        bool chooseMutation(int index);
        // Adds one level of `m` outright, without an offer (tests, debugging).
        void mutate(Mutation m);
        const Player& player() const { return player_; }
        const std::vector<Predator>& predators() const { return predators_; }
        int lineageAlive() const;
        // Puts an amoeba at (x, y); x < 0 picks an open spot far from the player.
        void addPredator(float x, float y, float energy) { spawnPredator(x, y, energy); }
        uint64_t playerAgeTicks() const { return player_.embodied ? tick_ - player_.sinceTick : 0; }

    private:
        void generateGrains();
        void generateSources();
        void populate();
        void rebuildOccupancy();
        int occupancyAt(int cx, int cy) const;
        void bumpOccupancy(int cx, int cy, int delta);
        void updateOrganisms();
        void updateFields();
        void swim(Body& body, const SpeciesTraits& traits, const strata::sim::Field2D& food);
        void tumble(Body& body, const strata::sim::Field2D& food);
        bool openCell(float x, float y) const;
        int neighbours(int cx, int cy) const;
        // Survival mode (survival.cpp).
        void playerActions();
        void playerSwim(Body& body, const SpeciesTraits& traits);
        void playerDied(DeathCause cause);
        void updatePredators();
        void spawnPredator(float x, float y, float energy);
        void predatorWaves();
        void playerDivided();
        float playerDashCost() const;
        void offerMutations();
        bool moveSliding(float& x, float& y, float dx, float dy) const;

        Config config_;
        strata::sim::Rng rng_;
        strata::sim::Mask2D grains_;
        strata::sim::Field2D nutrient_;
        strata::sim::Field2D waste_;
        strata::sim::Field2D oxygen_;
        World world_;
        std::vector<std::array<int, 2>> sources_;
        std::vector<uint16_t> occupancy_;
        uint64_t tick_ = 0;
        uint64_t births_ = 0;
        uint64_t deaths_ = 0;
        Player player_;
        PlayerInput input_;
        strata::sim::Rng mutationRng_;  // its own stream, so offers never shift the chamber's
        std::vector<Predator> predators_;
        uint32_t nextPredatorId_ = 1;
        uint64_t nextWaveTick_ = 0;
        int waves_ = 0;
};

// A simple bot for the embodied cell (headless demos and tests): flee the
// nearest amoeba, burst it when it is close, otherwise swim up the food
// gradient and away from oxygen or waste that hurts. Reads only.
PlayerInput autopilot(const MicroEnvironment& env);
// The bot's pick from a pending mutation offer (an index into Player::offer).
int autopilotMutation(const MicroEnvironment& env);

}  // namespace mbstrata

#endif
