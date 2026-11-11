#include "header/mbapp.h"
#include "header/result.h"
#include "header/usdRecording.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <random>

MicrobiomeApplication::MicrobiomeApplication() : MicrobiomeApplication(AppConfig(), std::cout) {
}

MicrobiomeApplication::MicrobiomeApplication(AppConfig config, std::ostream& out) : config(config), out(&out) {
}

bool MicrobiomeApplication::run() {
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
    // Both options are optional; with neither the app runs its default simulations.
    std::string recordPath;
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
        else {
            err << "usage: " << argv[0] << " [--record <path.usda>] [--seed <N>]" << std::endl;
            return 2;
        }
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
