#include "header/mbapp.h"
#include "header/result.h"

MicrobiomeApplication::MicrobiomeApplication() {
    config = AppConfig();
}

bool MicrobiomeApplication::run() {
    // run simulations
    for (int i = 0; i < config.getNumSimulations(); i++) {
        std::cout << "Running simulation " << i + 1 << " of " << config.getNumSimulations() << std::endl;
        simulation = new Simulation(&config, i + 1, "Simulation " + std::to_string(i + 1));
        simulation->run();
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

void MicrobiomeApplication::printResults() {
    // print out result summaries
    for (int i = 0; i < results.size(); i++) {
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
    for (int i = 0; i < results.size(); i++) {
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

int main() {
    MicrobiomeApplication microbiomeApplication;
    microbiomeApplication.run();
    return 0;
}