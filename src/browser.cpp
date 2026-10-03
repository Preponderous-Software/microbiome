// Browser (Emscripten) entry point: runs the same Microbiome simulation the
// console app and the Ulfius web server run, entirely client-side, and hands
// each tick's state to the page (web/shell.html) to draw. Built only by
// web/build.sh; the native targets in the Makefile never compile this file,
// so mb_app, mb_webapp and the test suites are unchanged by it.
//
// No threads, no sockets, no usage reporting: the vendored trace client
// (header/usageReporting.h) is simply not included here.
#ifdef __EMSCRIPTEN__

#include <cstdlib>
#include <sstream>
#include <string>

#include <emscripten/emscripten.h>

#include "header/appConfig.h"
#include "header/microbiome.h"

namespace {
    // Mirrors WebServer: same config, same 200 ms tick, and a new generation
    // starts automatically once the population goes extinct.
    const double TICK_INTERVAL_MS = 200.0;

    AppConfig config;
    Microbiome* microbiome = nullptr;
    int generation = 0;
    int tickCount = 0;
    double lastTickAt = 0.0;

    void startGeneration() {
        delete microbiome;
        tickCount = 0;
        microbiome = new Microbiome(generation++, "Browser Simulation", config.getEnvironmentSize(), config.getEntityFactor());
    }

    std::string stateJson() {
        std::ostringstream out;
        out << "{\"generation\":" << generation
            << ",\"tick\":" << tickCount
            << ",\"gridSize\":" << microbiome->getGrid()->getSize()
            << ",\"aliveCount\":" << microbiome->getNumAliveMicroorganisms()
            << ",\"deadCount\":" << microbiome->getNumDeadMicroorganisms()
            << ",\"totalEnergy\":" << microbiome->getTotalEnergy()
            << ",\"microorganisms\":[";
        bool first = true;
        for (Microorganism* organism : microbiome->getMicroorganisms()) {
            Location& location = microbiome->getGrid()->getLocation(organism->getLocationId());
            out << (first ? "" : ",") << "[" << location.getX() << "," << location.getY() << ","
                << organism->getEnergy() << "," << organism->getTimesEaten() << "]";
            first = false;
        }
        out << "],\"biomatter\":[";
        first = true;
        for (Biomatter* b : microbiome->getBiomatter()) {
            Location& location = microbiome->getGrid()->getLocation(b->getLocationId());
            out << (first ? "" : ",") << "[" << location.getX() << "," << location.getY() << "]";
            first = false;
        }
        out << "]}";
        return out.str();
    }
}

EM_JS(void, mb_draw, (const char* json), {
    if (typeof Module.onMicrobiomeState === "function") {
        Module.onMicrobiomeState(JSON.parse(UTF8ToString(json)));
    }
});

namespace {
    void publishState() {
        std::string json = stateJson();
        mb_draw(json.c_str());
    }

    void frame() {
        double now = emscripten_get_now();
        if (now - lastTickAt < TICK_INTERVAL_MS) {
            return;
        }
        lastTickAt = now;

        if (microbiome->getNumAliveMicroorganisms() == 0) {
            startGeneration();
        }
        else {
            microbiome->initiateMicroorganismMovement();
            tickCount++;
        }
        publishState();
    }
}

extern "C" {
    // Called by the page's Restart button: a fresh community from generation 1.
    EMSCRIPTEN_KEEPALIVE void mb_restart() {
        srand(static_cast<unsigned>(emscripten_get_now() * 1000.0));
        generation = 0;
        startGeneration();
        lastTickAt = emscripten_get_now();
        publishState();
    }
}

int main() {
    config.setSimulationOutputEnabled(false);
    mb_restart();
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}

#endif
