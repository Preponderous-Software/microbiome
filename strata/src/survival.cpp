// Survival mode: one microorganism under the player's control, amoebae that
// hunt it, and the bacteriocin bursts it fights back with. See
// docs/STRATA.md, "Survival".
//
// Nothing here runs until embody() is first called, and stateHash() only
// covers this state once it has, so a run nobody plays is exactly the run it
// was before survival mode existed. Once it has started it is as deterministic
// as the rest: the same inputs on the same ticks give the same chamber.
#include "micro_environment.h"

#include <algorithm>
#include <cmath>

namespace mbstrata {

using strata::sim::Entity;

namespace {

constexpr uint32_t kPlayerLineage = 1;

int cellOf(float v) { return static_cast<int>(std::floor(v)); }

float distance2(float ax, float ay, float bx, float by) {
    const float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

}  // namespace

const char* deathCauseText(DeathCause cause) {
    switch (cause) {
        case DeathCause::Starved: return "starved";
        case DeathCause::Oxygen: return "poisoned by oxygen";
        case DeathCause::Waste: return "poisoned by waste";
        case DeathCause::Eaten: return "engulfed by an amoeba";
        case DeathCause::None: break;
    }
    return "";
}

const char* mutationName(Mutation m) {
    switch (m) {
        case Mutation::Flagella: return "Extra flagella";
        case Mutation::LeanDash: return "Lean sprint";
        case Mutation::PotentToxin: return "Potent toxin";
        case Mutation::QuickReload: return "Quick reload";
        case Mutation::Membrane: return "Thick membrane";
        case Mutation::Hardy: return "Hardy";
        case Mutation::Uptake: return "Efficient uptake";
    }
    return "";
}

const char* mutationEffect(Mutation m) {
    switch (m) {
        case Mutation::Flagella: return "swim 20% faster";
        case Mutation::LeanDash: return "dash costs 30% less";
        case Mutation::PotentToxin: return "toxin reaches 1 cell farther, hits 25% harder";
        case Mutation::QuickReload: return "toxin recharges 25% sooner, costs 15% less";
        case Mutation::Membrane: return "survive one more engulfing per division";
        case Mutation::Hardy: return "oxygen and waste burn 25% less";
        case Mutation::Uptake: return "15% more energy from food";
    }
    return "";
}

float MicroEnvironment::playerDashCost() const {
    return config_.dashCost * (1.0f - 0.3f * static_cast<float>(player_.level(Mutation::LeanDash)));
}

void MicroEnvironment::playerDivided() {
    ++player_.divisions;
    player_.membrane = player_.level(Mutation::Membrane);  // a fresh membrane for the daughter
    if (player_.offerCount > 0) ++player_.offersPending;
    else offerMutations();
}

void MicroEnvironment::offerMutations() {
    // Three different mutations not yet at their highest level, drawn from
    // the mutation stream; fewer when fewer are left.
    std::array<Mutation, kMutationCount> open{};
    int n = 0;
    for (int i = 0; i < kMutationCount; ++i)
        if (player_.mutations[static_cast<size_t>(i)] < kMutationMaxLevel) open[static_cast<size_t>(n++)] = static_cast<Mutation>(i);
    player_.offerCount = std::min(n, kMutationOffer);
    for (int i = 0; i < player_.offerCount; ++i) {
        const int j = i + static_cast<int>(mutationRng_.below(static_cast<uint32_t>(n - i)));
        std::swap(open[static_cast<size_t>(i)], open[static_cast<size_t>(j)]);
        player_.offer[static_cast<size_t>(i)] = open[static_cast<size_t>(i)];
    }
    if (player_.offerCount == 0) player_.offersPending = 0;  // everything is maxed
}

std::vector<std::string> mutationItems(const Player& player) {
    std::vector<std::string> items;
    for (int i = 0; i < kMutationCount; ++i) {
        const int lv = player.mutations[static_cast<size_t>(i)];
        if (lv > 0) items.push_back(std::string(mutationName(static_cast<Mutation>(i))) + " " + std::to_string(lv));
    }
    return items;
}

void MicroEnvironment::mutate(Mutation m) {
    uint8_t& level = player_.mutations[static_cast<size_t>(m)];
    if (level >= kMutationMaxLevel) return;
    ++level;
    if (m == Mutation::Membrane) ++player_.membrane;
}

bool MicroEnvironment::chooseMutation(int index) {
    if (index < 0 || index >= player_.offerCount) return false;
    mutate(player_.offer[static_cast<size_t>(index)]);
    player_.offerCount = 0;
    if (player_.offersPending > 0) {
        --player_.offersPending;
        offerMutations();
    }
    return true;
}

Entity MicroEnvironment::nearestCell(float x, float y, int species) const {
    Entity best;
    float bestD = 0.0f;
    world_.each<Body, Cell>([&](Entity e, const Body& b, const Cell& c) {
        if (species >= 0 && static_cast<int>(c.species) != species) return;
        const float d = distance2(x, y, b.x, b.y);
        if (!best.valid() || d < bestD) {
            best = e;
            bestD = d;
        }
    });
    return best;
}

bool MicroEnvironment::embody(Entity e) {
    Cell* cell = world_.get<Cell>(e);
    const Body* body = world_.get<Body>(e);
    if (!cell || !body) return false;
    if (!player_.started) {
        player_.started = true;
        // The amoebae arrive from wherever is farthest from the player.
        for (int i = 0; i < config_.predatorsAtStart; ++i) spawnPredator(-1.0f, -1.0f, config_.predatorEnergy);
        nextWaveTick_ = tick_ + static_cast<uint64_t>(std::max(1, config_.predatorWaveTicks));
    }
    cell->lineage = kPlayerLineage;
    cell->energy = std::max(cell->energy, config_.embodyEnergy);
    player_.embodied = true;
    player_.entity = e;
    player_.species = cell->species;
    player_.sinceTick = tick_;
    player_.energy = cell->energy;
    player_.lastX = body->x;
    player_.lastY = body->y;
    player_.cause = DeathCause::None;
    player_.membrane = player_.level(Mutation::Membrane);
    ++player_.lives;
    input_ = PlayerInput{};
    return true;
}

bool MicroEnvironment::continueLineage() {
    if (!player_.started || player_.embodied) return false;
    Entity best;
    float bestD = 0.0f;
    world_.each<Body, Cell>([&](Entity e, const Body& b, const Cell& c) {
        if (c.lineage != kPlayerLineage) return;
        const float d = distance2(player_.lastX, player_.lastY, b.x, b.y);
        if (!best.valid() || d < bestD) {
            best = e;
            bestD = d;
        }
    });
    if (!best.valid()) return false;
    const int divisions = player_.divisions;
    const bool ok = embody(best);
    player_.divisions = divisions;
    return ok;
}

void MicroEnvironment::release() {
    if (!player_.embodied) return;
    player_.bestTicks = std::max(player_.bestTicks, tick_ - player_.sinceTick);
    player_.embodied = false;
    player_.cause = DeathCause::None;
    player_.dashing = false;
    input_ = PlayerInput{};
}

int MicroEnvironment::lineageAlive() const {
    if (!player_.started) return 0;
    int n = 0;
    world_.each<Cell>([&](Entity, const Cell& c) {
        if (c.lineage == kPlayerLineage) ++n;
    });
    return n;
}

void MicroEnvironment::playerDied(DeathCause cause) {
    player_.bestTicks = std::max(player_.bestTicks, tick_ - player_.sinceTick);
    player_.embodied = false;
    player_.diedTick = tick_;
    player_.cause = cause;
    player_.energy = 0.0f;
    player_.dashing = false;
    input_ = PlayerInput{};
}

bool MicroEnvironment::moveSliding(float& x, float& y, float dx, float dy) const {
    if (openCell(x + dx, y + dy)) {
        x += dx;
        y += dy;
        return true;
    }
    // Slide along a grain or wall instead of stopping dead.
    if (dx != 0.0f && openCell(x + dx, y)) {
        x += dx;
        return true;
    }
    if (dy != 0.0f && openCell(x, y + dy)) {
        y += dy;
        return true;
    }
    return false;
}

void MicroEnvironment::playerActions() {
    if (player_.toxinReady > 0) --player_.toxinReady;
    if (!player_.embodied) return;
    Body* body = world_.get<Body>(player_.entity);
    Cell* cell = world_.get<Cell>(player_.entity);
    if (!body || !cell) {
        player_.embodied = false;
        return;
    }
    const bool moving = input_.dx * input_.dx + input_.dy * input_.dy > 1e-6f;
    player_.dashing = input_.dash && moving && cell->energy > playerDashCost() * 4.0f;

    const float potent = static_cast<float>(player_.level(Mutation::PotentToxin));
    const float reload = static_cast<float>(player_.level(Mutation::QuickReload));
    const float toxinCost = config_.toxinCost * (1.0f - 0.15f * reload);
    const float damage = config_.toxinDamage * (1.0f + 0.25f * potent);
    if (input_.toxin && player_.toxinReady == 0 && cell->energy > toxinCost + 20.0f) {
        cell->energy -= toxinCost;
        const float radius = config_.toxinRadius + potent;
        const float r2 = radius * radius;
        const float px = body->x, py = body->y;
        world_.each<Body, Cell>([&](Entity, Body& b, Cell& c) {
            if (c.lineage == kPlayerLineage) return;  // kin are immune
            if (distance2(px, py, b.x, b.y) > r2) return;
            const bool wasAlive = c.energy > 0.0f;
            c.energy -= damage;
            if (wasAlive && c.energy <= 0.0f) ++player_.rivalsKilled;  // dies in this tick's pass
        });
        for (Predator& p : predators_) {
            if (p.energy <= 0.0f || distance2(px, py, p.x, p.y) > r2) continue;
            p.energy -= damage;
            p.stunTicks = static_cast<uint32_t>(std::max(0, config_.toxinStun));
            if (p.energy <= 0.0f) ++player_.amoebaeKilled;  // removed in updatePredators
        }
        player_.burst = true;
        player_.burstX = px;
        player_.burstY = py;
        player_.burstTick = tick_;
        player_.toxinReady =
            static_cast<uint32_t>(std::max(0.0f, static_cast<float>(config_.toxinCooldown) * (1.0f - 0.25f * reload)));
    }
    input_.toxin = false;  // one burst per press
}

void MicroEnvironment::playerSwim(Body& body, const SpeciesTraits& traits) {
    const float len2 = input_.dx * input_.dx + input_.dy * input_.dy;
    if (len2 <= 1e-6f) return;  // holding still
    const float len = std::sqrt(len2);
    const float ux = input_.dx / len, uy = input_.dy / len;
    const float throttle = std::min(len, 1.0f);
    const float flagella = 1.0f + 0.2f * static_cast<float>(player_.level(Mutation::Flagella));
    const float speed = traits.speed * throttle * flagella * (player_.dashing ? config_.dashMultiplier : 1.0f);
    body.hx = ux;
    body.hy = uy;
    const int ox = cellOf(body.x), oy = cellOf(body.y);
    if (!moveSliding(body.x, body.y, ux * speed, uy * speed)) return;
    const int cx = cellOf(body.x), cy = cellOf(body.y);
    if (ox != cx || oy != cy) {
        bumpOccupancy(ox, oy, -1);
        bumpOccupancy(cx, cy, 1);
    }
}

void MicroEnvironment::spawnPredator(float x, float y, float energy) {
    if (x < 0.0f) {
        // Pick the open spot, of a handful tried, farthest from the player.
        float bestD = -1.0f;
        for (int attempt = 0; attempt < 24; ++attempt) {
            const float cx = rng_.uniform(0.0f, static_cast<float>(config_.width));
            const float cy = rng_.uniform(0.0f, static_cast<float>(config_.height));
            if (!openCell(cx, cy)) continue;
            const float d = distance2(cx, cy, player_.lastX, player_.lastY);
            if (d > bestD) {
                bestD = d;
                x = cx;
                y = cy;
            }
        }
        if (bestD < 0.0f) return;  // nowhere open: skip this one
    }
    Predator p;
    p.x = x;
    p.y = y;
    rng_.unitVector(p.hx, p.hy);
    p.energy = energy;
    p.id = nextPredatorId_++;
    predators_.push_back(p);
}

void MicroEnvironment::predatorWaves() {
    if (tick_ < nextWaveTick_) return;
    if (static_cast<int>(predators_.size()) < config_.maxPredators) spawnPredator(-1.0f, -1.0f, config_.predatorEnergy);
    ++waves_;
    const int interval =
        std::max(config_.predatorWaveMin, config_.predatorWaveTicks - config_.predatorWaveStep * waves_);
    nextWaveTick_ = tick_ + static_cast<uint64_t>(std::max(1, interval));
}

void MicroEnvironment::updatePredators() {
    const float sense2 = config_.predatorSense * config_.predatorSense;
    const float reach2 = config_.predatorReach * config_.predatorReach;
    const float bias2 = config_.predatorPlayerBias * config_.predatorPlayerBias;
    std::vector<Predator> born;
    for (Predator& p : predators_) {
        if (p.energy <= 0.0f) continue;  // killed by a burst this tick
        p.energy -= config_.predatorUpkeep;
        if (p.stunTicks > 0) {
            --p.stunTicks;
        } else if (p.digestTicks > 0) {
            --p.digestTicks;  // a full food vacuole: it rests, and its prey gets a head start
        } else {
            // Hunt the nearest prey it can sense; the player smells richer.
            // A sated amoeba ignores ordinary prey and goes after the player.
            const bool sated = p.energy >= config_.predatorEnergy;
            Entity target;
            float bestD = sense2;
            float tx = 0, ty = 0;
            world_.each<Body, Cell>([&](Entity e, const Body& b, const Cell&) {
                const bool isPlayer = player_.embodied && e == player_.entity;
                if (sated && !isPlayer) return;
                float d = distance2(p.x, p.y, b.x, b.y);
                if (isPlayer) d *= bias2;
                if (d < bestD) {
                    bestD = d;
                    target = e;
                    tx = b.x;
                    ty = b.y;
                }
            });
            if (target.valid()) {
                const float dx = tx - p.x, dy = ty - p.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    p.hx = dx / len;
                    p.hy = dy / len;
                }
            } else if (player_.embodied && rng_.chance(sated ? 0.5f : 0.15f)) {
                // Nothing in reach: drift up the player's scent (amoebae track
                // the folate bacteria shed), so sooner or later they come.
                const float dx = player_.lastX - p.x, dy = player_.lastY - p.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-4f) {
                    p.hx = dx / len;
                    p.hy = dy / len;
                }
            } else if (rng_.chance(0.08f)) {
                rng_.unitVector(p.hx, p.hy);  // wander
            }
            if (!moveSliding(p.x, p.y, p.hx * config_.predatorSpeed, p.hy * config_.predatorSpeed))
                rng_.unitVector(p.hx, p.hy);  // boxed in: try another way next tick
            // Engulf whatever it caught.
            if (target.valid()) {
                const Body* b = world_.get<Body>(target);
                const Cell* c = world_.get<Cell>(target);
                const bool isPlayer = player_.embodied && target == player_.entity;
                if (b && c && isPlayer && player_.membrane > 0 && distance2(p.x, p.y, b->x, b->y) <= reach2) {
                    // The thick membrane holds: the amoeba spits the cell out
                    // and recoils, stunned, while the membrane is spent.
                    --player_.membrane;
                    p.stunTicks = static_cast<uint32_t>(std::max(0, config_.toxinStun));
                    float ax = p.x, ay = p.y;
                    if (moveSliding(ax, ay, -p.hx * 1.5f, -p.hy * 1.5f)) {
                        p.x = ax;
                        p.y = ay;
                    }
                } else if (b && c && distance2(p.x, p.y, b->x, b->y) <= reach2) {
                    p.energy += std::max(0.0f, c->energy) * config_.predatorGain + 30.0f;
                    p.digestTicks = static_cast<uint32_t>(std::max(0, config_.predatorDigest));
                    bumpOccupancy(cellOf(b->x), cellOf(b->y), -1);
                    if (isPlayer) {
                        player_.lastX = b->x;
                        player_.lastY = b->y;
                        playerDied(DeathCause::Eaten);
                    }
                    world_.destroy(target);
                    ++deaths_;
                }
            }
        }
        if (p.energy <= 0.0f) continue;
        if (p.energy >= config_.predatorDivide &&
            static_cast<int>(predators_.size() + born.size()) < config_.maxPredators) {
            p.energy *= 0.5f;
            Predator d = p;
            d.id = nextPredatorId_++;
            rng_.unitVector(d.hx, d.hy);
            float nx = p.x, ny = p.y;
            if (moveSliding(nx, ny, d.hx * 1.5f, d.hy * 1.5f)) {
                d.x = nx;
                d.y = ny;
            }
            born.push_back(d);
        }
    }
    // The dead return their biomass where they lie.
    for (const Predator& p : predators_)
        if (p.energy <= 0.0f) nutrient_.add(cellOf(p.x), cellOf(p.y), config_.biomatterYieldPerDeath * 3.0f);
    predators_.erase(std::remove_if(predators_.begin(), predators_.end(), [](const Predator& p) { return p.energy <= 0.0f; }),
                     predators_.end());
    for (const Predator& d : born) predators_.push_back(d);

    if (player_.embodied) {
        if (const Cell* c = world_.get<Cell>(player_.entity)) {
            const Body* b = world_.get<Body>(player_.entity);
            player_.energy = c->energy;
            player_.lastX = b->x;
            player_.lastY = b->y;
        } else {
            player_.embodied = false;
        }
    }
}

int autopilotMutation(const MicroEnvironment& env) {
    // Staying alive first, then getting away, then eating and fighting.
    static const Mutation kPreference[kMutationCount] = {Mutation::Membrane,    Mutation::Flagella, Mutation::Hardy,
                                                         Mutation::Uptake,      Mutation::PotentToxin,
                                                         Mutation::QuickReload, Mutation::LeanDash};
    const Player& pl = env.player();
    for (Mutation want : kPreference)
        for (int i = 0; i < pl.offerCount; ++i)
            if (pl.offer[static_cast<size_t>(i)] == want) return i;
    return pl.offerCount > 0 ? 0 : -1;
}

PlayerInput autopilot(const MicroEnvironment& env) {
    PlayerInput in;
    const Player& pl = env.player();
    if (!pl.embodied) return in;
    const Body* b = env.world().get<Body>(pl.entity);
    if (!b) return in;
    const Config& cfg = env.config();
    const SpeciesTraits& t = traitsOf(pl.species);
    const Predator* threat = nullptr;
    float threatD = cfg.predatorSense * 0.8f;
    for (const Predator& p : env.predators()) {
        const float d = std::sqrt(distance2(p.x, p.y, b->x, b->y));
        if (p.stunTicks == 0 && d < threatD) {
            threatD = d;
            threat = &p;
        }
    }
    if (threat) {
        const float dx = b->x - threat->x, dy = b->y - threat->y;
        const float len = std::max(std::sqrt(dx * dx + dy * dy), 1e-3f);
        in.dx = dx / len;
        in.dy = dy / len;
        in.dash = true;
        in.toxin = threatD < (cfg.toxinRadius + static_cast<float>(pl.level(Mutation::PotentToxin))) * 0.8f && pl.toxinReady == 0;
        return in;
    }
    float gx = 0, gy = 0;
    const strata::sim::Field2D& food = t.food == Food::Nutrient ? env.nutrient() : env.waste();
    food.gradient(b->x, b->y, &env.grains(), gx, gy);
    float ox = 0, oy = 0;
    if (pl.oxygenDrain > 0.0f) env.oxygen().gradient(b->x, b->y, &env.grains(), ox, oy);
    float wx = 0, wy = 0;
    if (pl.wasteDrain > 0.0f) env.waste().gradient(b->x, b->y, &env.grains(), wx, wy);
    // Aerobes also climb towards oxygen, where their yield is highest.
    float ax = 0, ay = 0;
    if (t.oxygenPerUnit > 0.0f && pl.oxygenDrain <= 0.0f) env.oxygen().gradient(b->x, b->y, &env.grains(), ax, ay);
    in.dx = gx + ax * 0.5f - ox * 4.0f - wx * 6.0f;
    in.dy = gy + ay * 0.5f - oy * 4.0f - wy * 6.0f;
    float len = std::sqrt(in.dx * in.dx + in.dy * in.dy);
    const float need = static_cast<float>(env.world().get<Cell>(pl.entity)->metabolicRate) * 2.0f;
    if (t.food == Food::Nutrient && pl.meal < need && len < 0.5f && pl.oxygenDrain <= 0.0f && pl.wasteDrain <= 0.0f) {
        // Hungry with nothing to follow: head for the nearest seep's glow.
        float best = 1e30f, sx = 0, sy = 0;
        for (const auto& src : env.sources()) {
            const float d = distance2(b->x, b->y, static_cast<float>(src[0]) + 0.5f, static_cast<float>(src[1]) + 0.5f);
            if (d < best) {
                best = d;
                sx = static_cast<float>(src[0]) + 0.5f - b->x;
                sy = static_cast<float>(src[1]) + 0.5f - b->y;
            }
        }
        if (best > 4.0f) {
            in.dx = sx;
            in.dy = sy;
            len = std::sqrt(sx * sx + sy * sy);
        }
    }
    if (len < 1e-4f) {
        in.dx = b->hx;  // nothing to go on: keep swimming
        in.dy = b->hy;
    } else if (len < 0.5f && pl.meal > need) {
        in.dx = in.dy = 0;  // a good spot: stay and eat
    } else {
        in.dx /= len;
        in.dy /= len;
    }
    return in;
}

}  // namespace mbstrata
