// Tests for the opt-in branching-colony scenario (BranchingColony) and the
// fractal-dimension measures (fractalDimension.h) it is judged by.
#include "lib/catch2.hpp"

#include "../src/header/appConfig.h"
#include "../src/header/branchingColony.h"
#include "../src/header/fractalDimension.h"

#include <cstdlib>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using Cells = std::vector<std::pair<int, int>>;

Cells filledSquare(int side) {
    Cells cells;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            cells.emplace_back(x, y);
        }
    }
    return cells;
}

Cells filledDisc(int radius) {
    Cells cells;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                cells.emplace_back(x, y);
            }
        }
    }
    return cells;
}

Cells line(int length) {
    Cells cells;
    for (int x = 0; x < length; x++) {
        cells.emplace_back(x, 0);
    }
    return cells;
}

double massRadius(const BranchingColony& colony) {
    const int centre = colony.getGridSize() / 2;
    return fractal::massRadiusDimension(colony.getCells(), centre, centre, 4.0, colony.getRadius() / 2);
}

// The grid the dimension tests grow on: big enough for a few octaves of box
// sizes between the branch width and the colony, small enough to run quickly.
const int DISH = 201;

}

TEST_CASE("Box counting measures a filled square as 2 and a line as 1", "[fractal]") {
    REQUIRE(fractal::boxCountingDimension(filledSquare(256), 1, 32) == Approx(2.0).margin(0.05));
    REQUIRE(fractal::boxCountingDimension(line(256), 1, 32) == Approx(1.0).margin(0.05));
}

TEST_CASE("Box counts halve the side and quarter the boxes for a filled square", "[fractal]") {
    std::vector<fractal::BoxCount> counts = fractal::countBoxes(filledSquare(64), 1, 8);
    REQUIRE(counts.size() == 4);
    REQUIRE(counts[0].boxSize == 1);
    REQUIRE(counts[0].boxes == Approx(64.0 * 64.0));
    REQUIRE(counts[3].boxSize == 8);
    // 8 x 8 aligned boxes, 9 x 9 when the grid is shifted half a box both ways
    REQUIRE(counts[3].boxes == Approx((64.0 + 72.0 + 72.0 + 81.0) / 4.0));
}

TEST_CASE("Fractal measures of nothing are 0", "[fractal]") {
    REQUIRE(fractal::countBoxes({}, 1, 8).empty());
    REQUIRE(fractal::boxCountingDimension({}, 1, 8) == 0.0);
    REQUIRE(fractal::colonyDimension({}) == 0.0);
    REQUIRE(fractal::massRadiusDimension({}, 0, 0, 4.0, 64.0) == 0.0);
    // one box size is not a slope
    REQUIRE(fractal::boxCountingDimension(filledSquare(8), 4, 4) == 0.0);
}

TEST_CASE("Mass against radius measures a filled disc as 2 and a line as 1", "[fractal]") {
    REQUIRE(fractal::massRadiusDimension(filledDisc(100), 0, 0, 4.0, 50.0) == Approx(2.0).margin(0.03));
    Cells centredLine;
    for (int x = -100; x <= 100; x++) {
        centredLine.emplace_back(x, 0);
    }
    REQUIRE(fractal::massRadiusDimension(centredLine, 0, 0, 4.0, 50.0) == Approx(1.0).margin(0.05));
}

TEST_CASE("The colony scenario is off by default", "[colony][appconfig]") {
    AppConfig config;
    REQUIRE_FALSE(config.isColonyScenarioEnabled());
    REQUIRE_FALSE(config.isColonyNutrientRich());
    REQUIRE(config.getColonyGridSize() == 201);

    config.setColonyScenarioEnabled(true);
    config.setColonyNutrientRich(true);
    config.setColonyGridSize(64);
    REQUIRE(config.isColonyScenarioEnabled());
    REQUIRE(config.isColonyNutrientRich());
    REQUIRE(config.getColonyGridSize() == 64);
}

TEST_CASE("A colony starts as one cell in the middle of the dish", "[colony]") {
    for (BranchingColony::Regime regime : {BranchingColony::Regime::NutrientPoor, BranchingColony::Regime::NutrientRich}) {
        BranchingColony colony(41, regime, 7);
        REQUIRE(colony.getNumCells() == 1);
        REQUIRE(colony.isOccupied(20, 20));
        REQUIRE(colony.getRadius() == 0.0);
        REQUIRE(colony.getPacketsReleased() == 0);
        REQUIRE(colony.getRegime() == regime);
    }
    REQUIRE_THROWS_AS(BranchingColony(15, BranchingColony::Regime::NutrientPoor, 1), std::invalid_argument);
}

TEST_CASE("A colony grows one connected cell at a time and stops short of the edge", "[colony]") {
    for (BranchingColony::Regime regime : {BranchingColony::Regime::NutrientPoor, BranchingColony::Regime::NutrientRich}) {
        BranchingColony colony(61, regime, 3);
        int grown = colony.growTo(1000000);
        REQUIRE(grown == colony.getNumCells());
        REQUIRE(grown > 50);
        REQUIRE_FALSE(colony.grow());
        REQUIRE(colony.getNumCells() == grown);

        // every cell is a daughter of a neighbour: the colony is one connected piece
        std::set<std::pair<int, int>> seen;
        for (const auto& cell : colony.getCells()) {
            REQUIRE(seen.count(cell) == 0);
            if (!seen.empty()) {
                bool touches = false;
                for (const auto& other : seen) {
                    if (std::abs(other.first - cell.first) + std::abs(other.second - cell.second) == 1) {
                        touches = true;
                        break;
                    }
                }
                REQUIRE(touches);
            }
            seen.insert(cell);
            // no cell on, or next to, the dish's edge
            REQUIRE(cell.first > 1);
            REQUIRE(cell.second > 1);
            REQUIRE(cell.first < 59);
            REQUIRE(cell.second < 59);
        }
        REQUIRE(colony.getRadius() + 7.0 >= 30.0);
    }
}

TEST_CASE("Nutrient-poor growth is fed only by released packets", "[colony]") {
    BranchingColony colony(61, BranchingColony::Regime::NutrientPoor, 5);
    colony.growTo(200);
    REQUIRE(colony.getNumCells() == 200);
    // each new cell ate one packet; some packets wander off instead
    REQUIRE(colony.getPacketsReleased() >= 199);

    BranchingColony rich(61, BranchingColony::Regime::NutrientRich, 5);
    rich.growTo(200);
    REQUIRE(rich.getPacketsReleased() == 0);
}

TEST_CASE("The same seed grows the same colony", "[colony]") {
    for (BranchingColony::Regime regime : {BranchingColony::Regime::NutrientPoor, BranchingColony::Regime::NutrientRich}) {
        BranchingColony first(81, regime, 42), second(81, regime, 42), other(81, regime, 43);
        first.growTo(500);
        second.growTo(500);
        other.growTo(500);
        REQUIRE(first.getCells() == second.getCells());
        REQUIRE(first.getCells() != other.getCells());
    }
}

// The point of the scenario. Calibrated on 50 seeds each (1-50) on this
// 201x201 dish, grown to the edge: nutrient-poor box counting 1.454-1.571
// (mean 1.523), mass-radius 1.567-1.812 (mean 1.675); nutrient-rich box counting
// 1.805-1.841 (mean 1.821), mass-radius 2.001-2.002. Lattice diffusion-limited
// aggregation is 1.71 by mass-radius in the literature and Matsushita &
// Fujikawa (1990) measured about 1.7 on B. subtilis; box counting reads both
// regimes low at this size (see massRadiusDimension), so it is compared
// between regimes rather than to 1.7.
TEST_CASE("Nutrient-poor colonies branch: lower fractal dimension than nutrient-rich ones", "[colony][fractal]") {
    BranchingColony rich(DISH, BranchingColony::Regime::NutrientRich, 1);
    rich.growTo(DISH * DISH);
    const double richBoxes = fractal::colonyDimension(rich.getCells());
    const double richMass = massRadius(rich);
    INFO("nutrient-rich: box counting " << richBoxes << ", mass-radius " << richMass);
    REQUIRE(richBoxes > 1.75);
    REQUIRE(richMass > 1.95);

    for (uint32_t seed = 1; seed <= 3; seed++) {
        BranchingColony poor(DISH, BranchingColony::Regime::NutrientPoor, seed);
        poor.growTo(DISH * DISH);
        const double poorBoxes = fractal::colonyDimension(poor.getCells());
        const double poorMass = massRadius(poor);
        INFO("seed " << seed << " nutrient-poor: " << poor.getNumCells() << " cells, box counting "
             << poorBoxes << ", mass-radius " << poorMass);
        REQUIRE(poorBoxes > 1.40);
        REQUIRE(poorBoxes < 1.65);
        REQUIRE(richBoxes - poorBoxes > 0.2);
        REQUIRE(poorMass > 1.55);
        REQUIRE(poorMass < 1.85);
        // and it is sparse: the same radius holds far fewer cells than a compact colony
        REQUIRE(poor.getNumCells() * 3 < rich.getNumCells());
    }
}
