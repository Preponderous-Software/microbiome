#include "micro_environment.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "strata/sim/hash.hpp"

namespace mbstrata {

using strata::sim::Entity;
using strata::sim::Field2D;

namespace {

const SpeciesTraits kTraits[kSpeciesCount] = {
    // name           speed  food            uptake aero  anaero O2/unit waste  O2tol  wasteTol attach color
    {"aerobe",        0.60f, Food::Nutrient, 4.0f, 3.00f, 0.40f, 0.35f, 0.30f, -1.0f,  6.0f,   0.0f,  {0.15f, 0.55f, 1.00f}},
    {"fermenter",     0.25f, Food::Nutrient, 4.0f, 0.75f, 0.75f, 0.00f, 0.60f,  2.5f, 12.0f,   0.03f, {1.00f, 0.45f, 0.08f}},
    {"cross-feeder",  0.45f, Food::Waste,    4.0f, 1.40f, 0.50f, 0.20f, 0.00f, -1.0f, -1.0f,   0.0f,  {0.85f, 0.20f, 0.75f}},
};

int cellOf(float v) { return static_cast<int>(std::floor(v)); }

}  // namespace

const SpeciesTraits& traitsOf(Species species) {
    const int i = static_cast<int>(species);
    if (i < 0 || i >= kSpeciesCount) throw std::invalid_argument("mbstrata: unknown species");
    return kTraits[i];
}

MicroEnvironment::MicroEnvironment(const Config& config)
    : config_(config),
      rng_(config.seed, 0x6d6963726f62ULL),  // "microb": a fixed stream for this game
      grains_(config.width, config.height),
      nutrient_(config.width, config.height, config.initialNutrient),
      waste_(config.width, config.height, 0.0f),
      oxygen_(config.width, config.height, 0.0f) {
    if (config.width < 8 || config.height < 8) throw std::invalid_argument("mbstrata: the environment must be at least 8x8");
    occupancy_.assign(static_cast<size_t>(config.width) * static_cast<size_t>(config.height), 0);
    generateGrains();
    generateSources();
    for (int y = 0; y < config_.height; ++y)
        for (int x = 0; x < config_.width; ++x)
            if (grains_.blocked(x, y)) nutrient_.set(x, y, 0.0f);
    // The air interface starts saturated; oxygen diffuses down from it.
    for (int x = 0; x < config_.width; ++x)
        if (!grains_.blocked(x, 0)) oxygen_.set(x, 0, config_.oxygenSurface);
    populate();
}

void MicroEnvironment::generateGrains() {
    for (int i = 0; i < config_.grainCount; ++i) {
        const float cx = rng_.uniform(0.0f, static_cast<float>(config_.width));
        // Keep the top rows (the air interface) clear of grains.
        const float cy = rng_.uniform(8.0f, static_cast<float>(config_.height));
        const float r = rng_.uniform(config_.grainRadiusMin, config_.grainRadiusMax);
        grains_.blockDisc(cx, cy, r);
    }
}

void MicroEnvironment::generateSources() {
    // Seeps sit below the top eighth of the chamber, away from the air
    // interface, so nutrient and oxygen form opposing gradients (as in a
    // Winogradsky column) with an oxic band near the top.
    const int minY = std::max(1, config_.height / 8);
    int attempts = 0;
    while (static_cast<int>(sources_.size()) < config_.nutrientSources && attempts++ < 10000) {
        const int x = rng_.range(1, config_.width - 2);
        const int y = rng_.range(minY, config_.height - 2);
        if (grains_.blocked(x, y)) continue;
        sources_.push_back({x, y});
    }
}

bool MicroEnvironment::openCell(float x, float y) const { return !grains_.blocked(cellOf(x), cellOf(y)); }

void MicroEnvironment::populate() {
    for (int s = 0; s < kSpeciesCount; ++s) {
        for (int i = 0; i < config_.initialPerSpecies; ++i) {
            float x = 0, y = 0;
            for (int attempt = 0; attempt < 1000; ++attempt) {
                x = rng_.uniform(0.0f, static_cast<float>(config_.width));
                y = rng_.uniform(0.0f, static_cast<float>(config_.height));
                if (openCell(x, y)) break;
            }
            // Same ranges as the console Microorganism: energy in
            // (minimum, max], metabolic rate in [1, factor].
            const float energy = static_cast<float>(rng_.range(config_.minimumInitialEnergy + 1, config_.maxInitialEnergy));
            const int rate = rng_.range(1, config_.maxMetabolicRateFactor);
            spawn(static_cast<Species>(s), x, y, energy, rate);
        }
    }
}

Entity MicroEnvironment::spawn(Species species, float x, float y, float energy, int metabolicRate) {
    traitsOf(species);
    x = std::min(std::max(x, 0.0f), static_cast<float>(config_.width) - 0.001f);
    y = std::min(std::max(y, 0.0f), static_cast<float>(config_.height) - 0.001f);
    for (int step = 0; step < config_.width && !openCell(x, y); ++step) {
        x += 1.0f;
        if (x >= static_cast<float>(config_.width)) x -= static_cast<float>(config_.width);
    }
    Entity e = world_.create();
    Body body;
    body.x = x;
    body.y = y;
    rng_.unitVector(body.hx, body.hy);
    world_.add(e, body);
    Cell cell;
    cell.energy = energy;
    cell.metabolicRate = std::max(1, metabolicRate);
    cell.species = species;
    world_.add(e, cell);
    bumpOccupancy(cellOf(x), cellOf(y), 1);
    return e;
}

void MicroEnvironment::rebuildOccupancy() {
    std::fill(occupancy_.begin(), occupancy_.end(), 0);
    world_.each<Body>([&](Entity, Body& b) { bumpOccupancy(cellOf(b.x), cellOf(b.y), 1); });
}

int MicroEnvironment::occupancyAt(int cx, int cy) const {
    if (!grains_.inBounds(cx, cy)) return 0;
    return occupancy_[static_cast<size_t>(cy) * static_cast<size_t>(config_.width) + static_cast<size_t>(cx)];
}

void MicroEnvironment::bumpOccupancy(int cx, int cy, int delta) {
    if (!grains_.inBounds(cx, cy)) return;
    uint16_t& o = occupancy_[static_cast<size_t>(cy) * static_cast<size_t>(config_.width) + static_cast<size_t>(cx)];
    const int v = static_cast<int>(o) + delta;
    o = static_cast<uint16_t>(std::min(std::max(v, 0), 65535));
}

int MicroEnvironment::neighbours(int cx, int cy) const {
    int n = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) n += occupancyAt(cx + dx, cy + dy);
    return n - 1;  // not counting the organism asking
}

void MicroEnvironment::tumble(Body& body, const Field2D& food) {
    float ax = 0, ay = 0;
    rng_.unitVector(ax, ay);
    // Chemotaxis: with chemotaxisBiasPercent probability, a tumble tries a
    // second direction and keeps whichever points further up the food
    // gradient (the console app's chemotaxisBiasPercent, now proportional to
    // how much food there is rather than whether a neighbour has any).
    if (config_.chemotaxisEnabled && static_cast<int>(rng_.below(100u)) < config_.chemotaxisBiasPercent) {
        float bx = 0, by = 0;
        rng_.unitVector(bx, by);
        float gx = 0, gy = 0;
        food.gradient(body.x, body.y, &grains_, gx, gy);
        if (bx * gx + by * gy > ax * gx + ay * gy) {
            ax = bx;
            ay = by;
        }
    }
    body.hx = ax;
    body.hy = ay;
}

void MicroEnvironment::swim(Body& body, const SpeciesTraits& traits, const Field2D& food) {
    const float before = food.sample(body.x, body.y);
    const float nx = body.x + body.hx * traits.speed;
    const float ny = body.y + body.hy * traits.speed;
    if (!openCell(nx, ny)) {
        tumble(body, food);  // bumped into a grain or the chamber wall
        return;
    }
    const int ox = cellOf(body.x), oy = cellOf(body.y);
    const int cx = cellOf(nx), cy = cellOf(ny);
    if (ox != cx || oy != cy) {
        bumpOccupancy(ox, oy, -1);
        bumpOccupancy(cx, cy, 1);
    }
    body.x = nx;
    body.y = ny;
    // Run and tumble with temporal sensing: keep running while things get
    // better, tumble more often when they get worse.
    const float after = food.sample(body.x, body.y);
    const float p = !config_.chemotaxisEnabled ? 0.5f * (config_.tumbleWhenImproving + config_.tumbleWhenWorsening)
                    : after > before           ? config_.tumbleWhenImproving
                                               : config_.tumbleWhenWorsening;
    if (rng_.chance(p)) tumble(body, food);
}

void MicroEnvironment::updateOrganisms() {
    struct Birth {
        Species species;
        float x, y, energy;
        int32_t rate;
        uint8_t attached;
        uint32_t generation;
    };
    std::vector<Birth> births;
    size_t population = world_.size();

    world_.each<Body, Cell>([&](Entity e, Body& body, Cell& cell) {
        const SpeciesTraits& t = traitsOf(cell.species);
        Field2D& food = t.food == Food::Nutrient ? nutrient_ : waste_;
        ++cell.ageTicks;

        if (!body.attached) swim(body, t, food);
        const int cx = cellOf(body.x), cy = cellOf(body.y);

        // Biofilm: a fermenter that touches a surface, or is crowded by
        // neighbours (a quorum), may settle and stop swimming.
        if (!body.attached && t.attachChance > 0.0f &&
            (grains_.touchesSurface(cx, cy) || neighbours(cx, cy) >= config_.quorumThreshold) &&
            rng_.chance(t.attachChance))
            body.attached = 1;
        // Dispersal: a starving biofilm cell lets go and swims off.
        else if (body.attached && food.at(cx, cy) < 1.0f && rng_.chance(config_.detachWhenStarving))
            body.attached = 0;

        if (config_.metabolismEnabled) {
            const float factor = body.attached ? config_.attachedMetabolismFactor : 1.0f;
            cell.energy -= static_cast<float>(cell.metabolicRate) * factor;
            const float o2 = oxygen_.at(cx, cy);
            if (t.oxygenTolerance >= 0.0f && o2 > t.oxygenTolerance)
                cell.energy -= (o2 - t.oxygenTolerance) * config_.oxygenToxicity;
            const float w = waste_.at(cx, cy);
            if (t.wasteTolerance >= 0.0f && w > t.wasteTolerance)
                cell.energy -= (w - t.wasteTolerance) * config_.wasteToxicity;
        }

        if (config_.feedingEnabled && cell.energy > 0.0f) {
            const float want = static_cast<float>(cell.metabolicRate) * t.uptakeMultiplier;
            const float eaten = food.take(cx, cy, want);
            if (eaten > 0.0f) {
                float aerobic = 0.0f;
                if (t.oxygenPerUnit > 0.0f) {
                    const float gotO2 = oxygen_.take(cx, cy, eaten * t.oxygenPerUnit);
                    aerobic = gotO2 / t.oxygenPerUnit;
                }
                cell.energy += aerobic * t.aerobicYield + (eaten - aerobic) * t.anaerobicYield;
                if (t.wasteFraction > 0.0f) waste_.add(cx, cy, eaten * t.wasteFraction);
                ++cell.timesEaten;
            }
        }

        if (cell.energy <= 0.0f) {
            // Decomposition: the dead cell's biomass is released into the
            // nutrient field where it died.
            nutrient_.add(cx, cy, config_.biomatterYieldPerDeath);
            bumpOccupancy(cx, cy, -1);
            world_.destroy(e);
            ++deaths_;
            --population;
            return;
        }

        // Binary fission, as in the console app: two daughters share the
        // parent's energy minus the cost of dividing.
        if (config_.reproductionEnabled && cell.energy >= config_.reproductionEnergyThreshold &&
            population + births.size() < static_cast<size_t>(config_.maxPopulation)) {
            const float each = (cell.energy - config_.fissionCost) / 2.0f;
            if (each <= 0.0f) return;
            float dx = 0, dy = 0;
            rng_.unitVector(dx, dy);
            float bx = body.x + dx, by = body.y + dy;
            if (!openCell(bx, by) || occupancyAt(cellOf(bx), cellOf(by)) >= config_.crowdingLimit) {
                bx = body.x;
                by = body.y;
                if (occupancyAt(cx, cy) >= config_.crowdingLimit) return;  // contact inhibition
            }
            cell.energy = each;
            bumpOccupancy(cellOf(bx), cellOf(by), 1);
            births.push_back({cell.species, bx, by, each, cell.metabolicRate, body.attached, cell.generation + 1});
        }
    });

    for (const Birth& b : births) {
        Entity e = world_.create();
        Body body;
        body.x = b.x;
        body.y = b.y;
        body.attached = b.attached;
        rng_.unitVector(body.hx, body.hy);
        world_.add(e, body);
        Cell cell;
        cell.energy = b.energy;
        cell.metabolicRate = b.rate;
        cell.species = b.species;
        cell.generation = b.generation;
        world_.add(e, cell);
        ++births_;
    }
}

void MicroEnvironment::updateFields() {
    if (!config_.fieldsEnabled) return;
    for (const auto& s : sources_) nutrient_.add(s[0], s[1], config_.nutrientSourceRate);
    for (int x = 0; x < config_.width; ++x)
        if (!grains_.blocked(x, 0)) oxygen_.set(x, 0, config_.oxygenSurface);
    const int sub = std::max(1, config_.diffusionSubsteps);
    nutrient_.diffuse(config_.nutrientDiffusion, &grains_, sub);
    waste_.diffuse(config_.wasteDiffusion, &grains_, sub);
    oxygen_.diffuse(config_.oxygenDiffusion, &grains_, std::max(1, config_.oxygenSubsteps));
    nutrient_.decay(config_.nutrientDecay);
    waste_.decay(config_.wasteDecay);
    oxygen_.decay(config_.oxygenDecay);
}

void MicroEnvironment::step() {
    rebuildOccupancy();
    updateOrganisms();
    updateFields();
    ++tick_;
}

void MicroEnvironment::run(uint64_t ticks) {
    for (uint64_t i = 0; i < ticks; ++i) step();
}

Stats MicroEnvironment::stats() const {
    Stats s;
    s.tick = tick_;
    s.births = births_;
    s.deaths = deaths_;
    s.nutrientTotal = nutrient_.total();
    s.wasteTotal = waste_.total();
    s.oxygenTotal = oxygen_.total();
    world_.each<Body, Cell>([&](Entity, const Body& b, const Cell& c) {
        ++s.alive[static_cast<int>(c.species)];
        if (b.attached) ++s.attached;
        s.organismEnergy += static_cast<double>(c.energy);
    });
    return s;
}

uint64_t MicroEnvironment::stateHash() const {
    strata::sim::StateHasher h;
    h.u64(tick_);
    h.u64(rng_.state());
    h.u64(rng_.increment());
    h.u64(births_);
    h.u64(deaths_);
    nutrient_.hashInto(h);
    waste_.hashInto(h);
    oxygen_.hashInto(h);
    world_.each<Body, Cell>([&](Entity e, const Body& b, const Cell& c) {
        h.u32(e.index);
        h.u32(e.generation);
        h.f32(b.x);
        h.f32(b.y);
        h.f32(b.hx);
        h.f32(b.hy);
        h.u8(b.attached);
        h.f32(c.energy);
        h.i32(c.metabolicRate);
        h.u8(static_cast<uint8_t>(c.species));
        h.u32(c.timesEaten);
        h.u32(c.ageTicks);
        h.u32(c.generation);
    });
    return h.value();
}

std::string MicroEnvironment::summary() const {
    const Stats s = stats();
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "tick %llu | aerobes %d, fermenters %d, cross-feeders %d (biofilm %d) | nutrient %.0f, waste %.0f, oxygen %.0f",
                  static_cast<unsigned long long>(s.tick), s.alive[0], s.alive[1], s.alive[2], s.attached,
                  s.nutrientTotal, s.wasteTotal, s.oxygenTotal);
    return buf;
}

}  // namespace mbstrata
