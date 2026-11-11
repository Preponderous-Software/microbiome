#ifndef Microbiome_Application_h
#define Microbiome_Application_h

#include "appConfig.h"
#include "simulation.h"
#include "result.h"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

/**
 * The MicrobiomeApplication class is the main class for the Microbiome application.
 * @author Daniel McCoy Stephenson
 */
class MicrobiomeApplication {
    public:
        MicrobiomeApplication();
        // For tests: a smaller config, and somewhere other than stdout to print to.
        MicrobiomeApplication(AppConfig config, std::ostream& out);
        // Runs the configured simulations and prints their result(s); true on success.
        bool run();
        void setRecordPath(std::string recordPath);
        std::vector<Result> getResults() const;
    private:
        std::string recordPath; // empty unless started with --record <path>
        AppConfig config;
        std::ostream* out;
        std::vector<Result> results;
        void printResultAverages();
};

/**
 * The console app (mb_app) without its main(), so the test suite can run it:
 * parses argc/argv (`--record <path.usda>`, `--seed <N>`), seeds rand(), runs
 * a MicrobiomeApplication with `config` and returns the exit status (2 with a
 * usage message on `err` for any other arguments). `onStarted` is called once
 * the arguments are accepted and before any simulation runs; main() sends its
 * usage report from there, so the tests, which pass none, report nothing.
 */
int runConsoleApp(int argc, char** argv, std::ostream& out, std::ostream& err,
                  AppConfig config = AppConfig(), const std::function<void()>& onStarted = nullptr);

#endif
