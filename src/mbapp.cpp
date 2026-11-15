#include "header/mbapp.h"
#include "header/branchingColony.h"
#include "header/fractalDimension.h"
#include "header/result.h"
#include "header/usdRecording.h"

#include <cerrno>
#include <chrono>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <random>

MicrobiomeApplication::MicrobiomeApplication() : MicrobiomeApplication(AppConfig(), std::cout) {
}

MicrobiomeApplication::MicrobiomeApplication(AppConfig config, std::ostream& out) : config(config), out(&out) {
}

bool MicrobiomeApplication::run() {
    if (config.isColonyScenarioEnabled()) {
        runColonyScenario();
        return true;
    }

    // run simulations
    for (size_t i = 0; i < config.getNumSimulations(); i++) {
        *out << "Running simulation " << i + 1 << " of " << config.getNumSimulations() << std::endl;
        Simulation simulation(&config, i + 1, "Simulation " + std::to_string(i + 1));
        // --record captures the first simulation only, so one file holds one history.
        UsdRecording* recording = nullptr;
        if (i == 0 && !recordPath.empty()) {
            recording = new UsdRecording();
            simulation.setRecording(recording);
        }
        simulation.run();
        if (recording != nullptr) {
            recording->write(recordPath);
            *out << "Recorded " << recording->getTicksRecorded() << " ticks of simulation 1 to " << recordPath << std::endl;
            delete recording;
        }
        results.push_back(Result(&simulation));
    }
    
    // print results
    if (results.size() > 1) {
        printResultAverages();
    }
    else if (results.size() == 1) {
        results[0].print(*out);
    }
    else {
        *out << "No results to print." << std::endl;
    }
    
    return true;
}

void MicrobiomeApplication::setRecordPath(std::string recordPath) {
    this->recordPath = recordPath;
}

std::vector<Result> MicrobiomeApplication::getResults() const {
    return results;
}

void MicrobiomeApplication::printResultAverages() {
    // print out averages of values in results (rounded down to whole numbers)
    size_t totalSurvivingMicroorganisms = 0;
    size_t totalDeadMicroorganisms = 0;
    size_t totalEnergy = 0;
    size_t totalTicksElapsed = 0;
    for (size_t i = 0; i < results.size(); i++) {
        totalSurvivingMicroorganisms += results[i].getSurvivingMicroorganisms();
        totalDeadMicroorganisms += results[i].getDeadMicroorganisms();
        totalEnergy += results[i].getEnergy();
        totalTicksElapsed += results[i].getTicksElapsed();
    }
    *out << "=== Average Results of " << results.size() << " Simulations ===" << std::endl;
    *out << "Ticks elapsed: " << totalTicksElapsed / results.size() << std::endl;
    *out << "Dead Microorganisms: " << totalDeadMicroorganisms / results.size() << std::endl;
    *out << "Surviving Microorganisms: " << totalSurvivingMicroorganisms / results.size() << std::endl;
    *out << "Total Energy: " << totalEnergy / results.size() << std::endl;

}

void MicrobiomeApplication::runColonyScenario() {
    const bool rich = config.isColonyNutrientRich();
    const int gridSize = static_cast<int>(config.getColonyGridSize());
    // Seeded from rand(), so --seed repeats the colony too.
    BranchingColony colony(gridSize, rich ? BranchingColony::Regime::NutrientRich : BranchingColony::Regime::NutrientPoor,
                           static_cast<uint32_t>(std::rand()));
    *out << "Growing a colony on " << (rich ? "nutrient-rich" : "nutrient-poor") << " agar ("
         << gridSize << "x" << gridSize << ")..." << std::endl;
    colony.growTo(gridSize * gridSize);

    if (config.isSimulationOutputEnabled()) {
        // At most 100 characters wide: each character stands for a square
        // block of locations, '#' at least half occupied, '+' less, ' ' empty.
        const int block = std::max(1, (gridSize + 99) / 100);
        std::vector<std::string> lines;
        for (int top = 0; top < gridSize; top += block) {
            std::string line;
            for (int left = 0; left < gridSize; left += block) {
                int filled = 0, total = 0;
                for (int y = top; y < std::min(top + block, gridSize); y++) {
                    for (int x = left; x < std::min(left + block, gridSize); x++) {
                        filled += colony.isOccupied(x, y) ? 1 : 0;
                        total++;
                    }
                }
                line += filled == 0 ? ' ' : (filled * 2 >= total ? '#' : '+');
            }
            line.erase(line.find_last_not_of(' ') + 1);
            lines.push_back(line);
        }
        // Only the rows the colony reaches.
        size_t first = 0, last = lines.size();
        while (first < last && lines[first].empty()) {
            first++;
        }
        while (last > first && lines[last - 1].empty()) {
            last--;
        }
        for (size_t i = first; i < last; i++) {
            *out << lines[i] << "\n";
        }
    }

    const std::vector<std::pair<int, int>> cells = colony.getCells();
    *out << "=== Colony Complete ===" << std::endl;
    *out << "Cells: " << colony.getNumCells() << std::endl;
    *out << "Radius: " << static_cast<int>(colony.getRadius()) << std::endl;
    if (!rich) {
        *out << "Nutrient packets released: " << colony.getPacketsReleased() << std::endl;
    }
    *out << "Box-counting dimension: " << fractal::colonyDimension(cells) << std::endl;
    *out << "Mass-radius dimension: "
         << fractal::massRadiusDimension(cells, gridSize / 2, gridSize / 2, 4.0, colony.getRadius() / 2) << std::endl;
}

namespace {

// A whole number from 0 to UINT_MAX, digits only (no sign, spaces or suffix).
bool parseSeed(const char* text, unsigned& seed) {
    if (text[0] < '0' || text[0] > '9') {
        return false;
    }
    errno = 0;
    char* end;
    unsigned long long value = std::strtoull(text, &end, 10);
    if (*end != '\0' || errno == ERANGE || value > UINT_MAX) {
        return false;
    }
    seed = static_cast<unsigned>(value);
    return true;
}

// A different seed for every launch when --seed is not given.
unsigned randomSeed() {
    unsigned seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
    try {
        seed ^= std::random_device()();
    } catch (const std::exception&) {
        // no random device: the clock alone still differs between launches
    }
    return seed;
}

}

int runConsoleApp(int argc, char** argv, std::ostream& out, std::ostream& err,
                  AppConfig config, const std::function<void()>& onStarted) {
    // Every option is optional; with none the app runs its default simulations.
    const char* usage = " [--record <path.usda>] [--seed <N>] [--colony <poor|rich>]";
    std::string recordPath;
    bool colonyGiven = false;
    bool colonyRich = false;
    bool seedGiven = false;
    unsigned seed = 0;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            recordPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc && parseSeed(argv[i + 1], seed)) {
            seedGiven = true;
            i++;
        }
        else if (std::strcmp(argv[i], "--colony") == 0 && i + 1 < argc
                 && (std::strcmp(argv[i + 1], "poor") == 0 || std::strcmp(argv[i + 1], "rich") == 0)) {
            colonyGiven = true;
            colonyRich = std::strcmp(argv[++i], "rich") == 0;
        }
        else {
            err << "usage: " << argv[0] << usage << std::endl;
            return 2;
        }
    }
    // A colony is not a Microbiome simulation, so there is nothing to record.
    if (colonyGiven && !recordPath.empty()) {
        err << "usage: " << argv[0] << usage << std::endl;
        err << "--record cannot be combined with --colony" << std::endl;
        return 2;
    }
    if (colonyGiven) {
        config.setColonyScenarioEnabled(true);
        config.setColonyNutrientRich(colonyRich);
    }

    if (onStarted) {
        onStarted();
    }

    // Every launch runs different simulations unless --seed repeats an earlier one.
    if (!seedGiven) {
        seed = randomSeed();
    }
    std::srand(seed);
    out << "Seed: " << seed << " (repeat this run with --seed " << seed << ")" << std::endl;

    MicrobiomeApplication microbiomeApplication(config, out);
    microbiomeApplication.setRecordPath(recordPath);
    return microbiomeApplication.run() ? 0 : 1;
}
