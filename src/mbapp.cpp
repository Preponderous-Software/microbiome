#include "header/mbapp.h"
#include "header/result.h"
#include "header/usageReporting.h"
#include "header/usdRecording.h"

#include <cstring>

MicrobiomeApplication::MicrobiomeApplication() {
    config = AppConfig();
}

bool MicrobiomeApplication::run() {
    // run simulations
    for (size_t i = 0; i < config.getNumSimulations(); i++) {
        std::cout << "Running simulation " << i + 1 << " of " << config.getNumSimulations() << std::endl;
        simulation = new Simulation(&config, i + 1, "Simulation " + std::to_string(i + 1));
        // --record captures the first simulation only, so one file holds one history.
        UsdRecording* recording = nullptr;
        if (i == 0 && !recordPath.empty()) {
            recording = new UsdRecording();
            simulation->setRecording(recording);
        }
        simulation->run();
        if (recording != nullptr) {
            recording->write(recordPath);
            std::cout << "Recorded " << recording->getTicksRecorded() << " ticks of simulation 1 to " << recordPath << std::endl;
            delete recording;
        }
        Result result = Result(simulation);
        results.push_back(result);
        delete simulation;
    }
    
    // print results
    if (results.size() > 1) {
        printResultAverages();
    }
    else if (results.size() == 1) {
        results[0].print();
    }
    else {
        std::cout << "No results to print." << std::endl;
    }
    
    return 0;
}

void MicrobiomeApplication::setRecordPath(std::string recordPath) {
    this->recordPath = recordPath;
}

void MicrobiomeApplication::printResults() {
    // print out result summaries
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "=== Simulation " << i << " Result ===" << std::endl;
        std::cout << "Surviving Microorganisms: " << results[i].getSurvivingMicroorganisms() << std::endl;
        std::cout << "Dead Microorganisms: " << results[i].getDeadMicroorganisms() << std::endl;
        std::cout << "Total Energy: " << results[i].getEnergy() << std::endl;
        std::cout << "Ticks elapsed: " << results[i].getTicksElapsed() << std::endl;
    }
}

void MicrobiomeApplication::printResultAverages() {
    // print out averages of values in results
    int totalSurvivingMicroorganisms = 0;
    int totalDeadMicroorganisms = 0;
    int totalEnergy = 0;
    int totalTicksElapsed = 0;
    for (size_t i = 0; i < results.size(); i++) {
        totalSurvivingMicroorganisms += results[i].getSurvivingMicroorganisms();
        totalDeadMicroorganisms += results[i].getDeadMicroorganisms();
        totalEnergy += results[i].getEnergy();
        totalTicksElapsed += results[i].getTicksElapsed();
    }
    std::cout << "=== Average Results of " << results.size() << " Simulations ===" << std::endl;
    std::cout << "Ticks elapsed: " << totalTicksElapsed / results.size() << std::endl;
    std::cout << "Dead Microorganisms: " << totalDeadMicroorganisms / results.size() << std::endl;
    std::cout << "Surviving Microorganisms: " << totalSurvivingMicroorganisms / results.size() << std::endl;
    std::cout << "Total Energy: " << totalEnergy / results.size() << std::endl;

}

int main(int argc, char* argv[]) {
    // The only option is --record <path>; with no arguments the app runs exactly as before.
    std::string recordPath;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            recordPath = argv[++i];
        }
        else {
            std::cerr << "usage: " << argv[0] << " [--record <path.usda>]" << std::endl;
            return 2;
        }
    }

    // One startup event to trace, sent in the background; see
    // header/usageReporting.h and the README's "Usage reporting" section.
    // The destructor gives it a moment to leave when the program ends.
    usage_reporting::UsageReporter usageReporting;
    usageReporting.reportStartup();

    MicrobiomeApplication microbiomeApplication;
    microbiomeApplication.setRecordPath(recordPath);
    microbiomeApplication.run();
    return 0;
}