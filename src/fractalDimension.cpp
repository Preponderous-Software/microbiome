#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "header/fractalDimension.h"

namespace fractal {

std::vector<BoxCount> countBoxes(const std::vector<std::pair<int, int>>& locations,
                                 int smallestBox, int largestBox) {
    std::vector<BoxCount> counts;
    if (locations.empty() || smallestBox < 1) {
        return counts;
    }
    int minX = locations[0].first;
    int minY = locations[0].second;
    for (const auto& location : locations) {
        minX = std::min(minX, location.first);
        minY = std::min(minY, location.second);
    }
    for (int size = smallestBox; size <= largestBox; size *= 2) {
        // Where the box edges fall changes the count a little, so it is
        // averaged over the grid shifted by half a box on each axis.
        const int half = size / 2;
        const int shifts[2] = {0, half};
        double total = 0.0;
        int grids = 0;
        for (int sx = 0; sx < (half > 0 ? 2 : 1); sx++) {
            for (int sy = 0; sy < (half > 0 ? 2 : 1); sy++) {
                std::unordered_set<long long> boxes;
                for (const auto& location : locations) {
                    const long long bx = (location.first - minX + shifts[sx]) / size;
                    const long long by = (location.second - minY + shifts[sy]) / size;
                    boxes.insert((bx << 32) | by);
                }
                total += static_cast<double>(boxes.size());
                grids++;
            }
        }
        counts.push_back({size, total / grids});
    }
    return counts;
}

double boxCountingDimension(const std::vector<std::pair<int, int>>& locations,
                            int smallestBox, int largestBox) {
    const std::vector<BoxCount> counts = countBoxes(locations, smallestBox, largestBox);
    if (counts.size() < 2) {
        return 0.0;
    }
    double sumX = 0, sumY = 0, sumXX = 0, sumXY = 0;
    const double n = static_cast<double>(counts.size());
    for (const BoxCount& count : counts) {
        const double x = std::log(1.0 / count.boxSize);
        const double y = std::log(count.boxes);
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }
    return (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
}

double colonyDimension(const std::vector<std::pair<int, int>>& locations) {
    if (locations.empty()) {
        return 0.0;
    }
    int minX = locations[0].first, maxX = minX;
    int minY = locations[0].second, maxY = minY;
    for (const auto& location : locations) {
        minX = std::min(minX, location.first);
        maxX = std::max(maxX, location.first);
        minY = std::min(minY, location.second);
        maxY = std::max(maxY, location.second);
    }
    const int width = std::max(maxX - minX, maxY - minY) + 1;
    int largest = 4;
    while (largest * 2 <= width / 4) {
        largest *= 2;
    }
    return boxCountingDimension(locations, 4, largest);
}

double massRadiusDimension(const std::vector<std::pair<int, int>>& locations,
                           int centreX, int centreY, double minRadius, double maxRadius) {
    std::vector<double> squaredDistances;
    squaredDistances.reserve(locations.size());
    for (const auto& location : locations) {
        const double dx = location.first - centreX;
        const double dy = location.second - centreY;
        squaredDistances.push_back(dx * dx + dy * dy);
    }
    std::sort(squaredDistances.begin(), squaredDistances.end());

    double sumX = 0, sumY = 0, sumXX = 0, sumXY = 0;
    int n = 0;
    for (double r = minRadius; r > 0 && r <= maxRadius; r *= 1.25) {
        const size_t mass = std::upper_bound(squaredDistances.begin(), squaredDistances.end(), r * r)
                            - squaredDistances.begin();
        if (mass == 0) {
            continue;
        }
        const double x = std::log(r);
        const double y = std::log(static_cast<double>(mass));
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
        n++;
    }
    if (n < 2) {
        return 0.0;
    }
    return (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
}

}
