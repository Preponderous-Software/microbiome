#ifndef Fractal_Dimension_h
#define Fractal_Dimension_h

#include <utility>
#include <vector>

/**
 * Box counting: how "space-filling" a set of grid locations is. The plane is
 * covered with square boxes of side s, and N(s) is the number of boxes that
 * hold at least one location. For a fractal, N(s) ~ s^-D; D is the slope of
 * log N(s) against log(1/s). A filled disc measures close to 2, a line 1,
 * and a diffusion-limited aggregate about 1.7.
 */
namespace fractal {

    struct BoxCount {
        int boxSize;
        double boxes; // averaged over four placements of the box grid
    };

    // N(s) for s = smallestBox, 2 * smallestBox, ... up to largestBox, each
    // the mean over the box grid aligned to the set's lowest x and y and that
    // grid shifted by s/2 along x, y and both. Empty for an empty set.
    std::vector<BoxCount> countBoxes(const std::vector<std::pair<int, int>>& locations,
                                     int smallestBox, int largestBox);

    // The least-squares slope of log N(s) against log(1/s) over those box
    // sizes; 0 when fewer than two sizes are counted.
    double boxCountingDimension(const std::vector<std::pair<int, int>>& locations,
                                int smallestBox, int largestBox);

    // boxCountingDimension over the scales a colony's shape is visible at:
    // from boxes of 4 locations up to a quarter of the colony's width (powers
    // of two). Smaller boxes see the one-cell-wide branches themselves, which
    // are lines (slope near 1), not the branching pattern; larger ones would
    // split the colony only a few ways. 0 if the colony is under 32 wide.
    double colonyDimension(const std::vector<std::pair<int, int>>& locations);

    // A second estimate, from mass against radius: M(r), the number of
    // locations within r of (centreX, centreY), grows as r^D. Returns the
    // least-squares slope of log M(r) against log r for r from minRadius to
    // maxRadius in steps of 25%; 0 when that range holds fewer than two radii.
    // Box counting on a colony a few hundred locations across reads low (a
    // filled disc measures about 1.8 rather than 2, since the boxes along its
    // edge are only partly filled); this one does not, so the two together
    // bracket the true value.
    double massRadiusDimension(const std::vector<std::pair<int, int>>& locations,
                               int centreX, int centreY, double minRadius, double maxRadius);
}

#endif
