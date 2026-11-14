#ifndef Branching_Colony_h
#define Branching_Colony_h

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

/**
 * The BranchingColony class is an opt-in scenario: one colony grown from a
 * single cell in the middle of a square dish, under one of two nutrient
 * regimes.
 *
 * - NutrientPoor: food is scarce, so it reaches the colony only by diffusing
 *   in from the surrounding agar. Each unit of nutrient is a packet that is
 *   released on a circle just outside the colony and random-walks until it
 *   steps onto an empty location next to a colony cell; the cell next to it
 *   eats it and divides into that location. Growth therefore happens only at
 *   the colony's edge, and only where food arrives. Tips that stick out catch
 *   most of the packets before they can reach the fjords between them, so the
 *   colony branches (diffusion-limited aggregation, Witten & Sander 1981), as
 *   Bacillus subtilis does on nutrient-poor agar (Matsushita & Fujikawa 1990,
 *   box-counting dimension about 1.7).
 * - NutrientRich: food is everywhere, so any colony cell with an empty
 *   neighbour may divide into it, each edge location equally likely (Eden
 *   growth). The colony grows compact (dimension about 2).
 *
 * It does not use the Microbiome/Microorganism entities: a colony of
 * thousands of cells needs constant-time "is this location occupied?"
 * lookups, so it keeps its own occupancy grid. Its random numbers come from
 * its own generator, seeded at construction, so the same seed grows the same
 * colony.
 */
class BranchingColony {
    public:
        enum class Regime { NutrientPoor, NutrientRich };

        // A gridSize x gridSize dish (at least 16) with one cell at its centre.
        BranchingColony(int gridSize, Regime regime, uint32_t seed);

        // Adds one cell. Returns false, adding nothing, once the colony has
        // grown to within a margin of the dish's edge (or filled it).
        bool grow();
        // Grows until the colony has `cells` cells or reaches the edge; returns
        // the number of cells it has.
        int growTo(int cells);

        int getGridSize() const;
        Regime getRegime() const;
        int getNumCells() const;
        bool isOccupied(int x, int y) const;
        // Distance from the centre to the farthest cell, in locations.
        double getRadius() const;
        std::vector<std::pair<int, int>> getCells() const;
        // Nutrient packets released so far (NutrientPoor only; 0 otherwise).
        long long getPacketsReleased() const;

    private:
        bool growNutrientPoor();
        bool growNutrientRich();
        void occupy(int x, int y);
        bool hasOccupiedNeighbour(int x, int y) const;
        bool inside(int x, int y) const;
        bool nearEdge() const;
        uint32_t next(uint32_t bound);

        int gridSize;
        int centre;
        Regime regime;
        std::mt19937 random;
        std::vector<char> occupied;
        std::vector<std::pair<int, int>> cells;
        double radius = 0.0;
        long long packetsReleased = 0;

        // NutrientRich: empty locations next to the colony, each listed once.
        std::vector<int> frontier;
        std::vector<char> onFrontier;
};

#endif
