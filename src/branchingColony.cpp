#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "header/branchingColony.h"

namespace {
    const int OFFSETS[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    const double TWO_PI = 6.283185307179586;

    // Nutrient packets are released this many locations outside the colony's
    // farthest cell, and given up on (and released again) once they wander
    // twice as far out: a packet that far away is about as likely to come
    // back from any direction as a fresh one, so this only saves time.
    const double LAUNCH_MARGIN = 5.0;
    const double KILL_FACTOR = 2.0;
}

BranchingColony::BranchingColony(int gridSize, Regime regime, uint32_t seed)
    : gridSize(gridSize), centre(gridSize / 2), regime(regime), random(seed) {
    if (gridSize < 16) {
        throw std::invalid_argument("BranchingColony needs a grid of at least 16x16");
    }
    occupied.assign(static_cast<size_t>(gridSize) * gridSize, 0);
    onFrontier.assign(static_cast<size_t>(gridSize) * gridSize, 0);
    occupy(centre, centre);
}

bool BranchingColony::grow() {
    if (nearEdge()) {
        return false;
    }
    return regime == Regime::NutrientPoor ? growNutrientPoor() : growNutrientRich();
}

int BranchingColony::growTo(int target) {
    while (getNumCells() < target && grow()) {
    }
    return getNumCells();
}

bool BranchingColony::growNutrientPoor() {
    const double launchRadius = radius + LAUNCH_MARGIN;
    const double killRadius = launchRadius * KILL_FACTOR;
    const double killRadiusSquared = killRadius * killRadius;
    while (true) {
        // A packet of nutrient arrives from the surrounding agar ...
        const double angle = TWO_PI * (random() / 4294967296.0);
        int x = centre + static_cast<int>(std::lround(launchRadius * std::cos(angle)));
        int y = centre + static_cast<int>(std::lround(launchRadius * std::sin(angle)));
        packetsReleased++;
        while (true) {
            // ... and is eaten as soon as it is next to the colony, which
            // divides into the location the packet reached.
            if (hasOccupiedNeighbour(x, y)) {
                occupy(x, y);
                return true;
            }
            const int* step = OFFSETS[next(4)];
            x += step[0];
            y += step[1];
            const double dx = x - centre;
            const double dy = y - centre;
            if (!inside(x, y) || dx * dx + dy * dy > killRadiusSquared) {
                break; // wandered off: release another packet
            }
        }
    }
}

bool BranchingColony::growNutrientRich() {
    if (frontier.empty()) {
        return false;
    }
    const size_t pick = next(static_cast<uint32_t>(frontier.size()));
    const int index = frontier[pick];
    frontier[pick] = frontier.back();
    frontier.pop_back();
    onFrontier[index] = 0;
    occupy(index % gridSize, index / gridSize);
    return true;
}

void BranchingColony::occupy(int x, int y) {
    occupied[static_cast<size_t>(y) * gridSize + x] = 1;
    cells.emplace_back(x, y);
    const double dx = x - centre;
    const double dy = y - centre;
    radius = std::max(radius, std::sqrt(dx * dx + dy * dy));

    if (regime == Regime::NutrientRich) {
        for (const auto& offset : OFFSETS) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!inside(nx, ny)) {
                continue;
            }
            const int index = ny * gridSize + nx;
            if (!occupied[index] && !onFrontier[index]) {
                onFrontier[index] = 1;
                frontier.push_back(index);
            }
        }
    }
}

bool BranchingColony::hasOccupiedNeighbour(int x, int y) const {
    for (const auto& offset : OFFSETS) {
        if (isOccupied(x + offset[0], y + offset[1])) {
            return true;
        }
    }
    return false;
}

bool BranchingColony::inside(int x, int y) const {
    return x >= 0 && y >= 0 && x < gridSize && y < gridSize;
}

bool BranchingColony::nearEdge() const {
    // Packets are released LAUNCH_MARGIN outside the colony and must start on
    // the dish, so growth stops a little short of the edge in both regimes.
    return radius + LAUNCH_MARGIN + 2.0 >= centre;
}

uint32_t BranchingColony::next(uint32_t bound) {
    // Rejection sampling, so every value below `bound` is equally likely and
    // the sequence depends only on the seed (no std:: distribution, whose
    // output may differ between standard libraries).
    const uint32_t limit = static_cast<uint32_t>(0x100000000ULL - (0x100000000ULL % bound));
    uint32_t value;
    do {
        value = random();
    } while (limit != 0 && value >= limit);
    return value % bound;
}

int BranchingColony::getGridSize() const {
    return gridSize;
}

BranchingColony::Regime BranchingColony::getRegime() const {
    return regime;
}

int BranchingColony::getNumCells() const {
    return static_cast<int>(cells.size());
}

bool BranchingColony::isOccupied(int x, int y) const {
    return inside(x, y) && occupied[static_cast<size_t>(y) * gridSize + x] != 0;
}

double BranchingColony::getRadius() const {
    return radius;
}

std::vector<std::pair<int, int>> BranchingColony::getCells() const {
    return cells;
}

long long BranchingColony::getPacketsReleased() const {
    return packetsReleased;
}
