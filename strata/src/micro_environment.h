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
};

}  // namespace mbstrata

#endif
