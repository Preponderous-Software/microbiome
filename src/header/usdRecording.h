#ifndef Usd_Recording_h
#define Usd_Recording_h

#include <string>

#include "microbiome.h"
#include "vendor/usd_recorder/usda_writer.hpp"

/**
 * The UsdRecording class records a Microbiome's history as a time-sampled
 * OpenUSD stage (.usda), one time code per tick, using the vendored
 * usd-recorder C++ writer (src/header/vendor/usd_recorder/). It is only used
 * when the console app is started with `--record <path>`.
 *
 * Every living microorganism and every piece of biomatter is an entity at its
 * grid coordinates (x, y, 0) with `kind` and `energy` values; microorganisms
 * also carry `metabolicRate` and `timesEaten`. The stage carries the alive and
 * dead counts and the total energy per tick.
 * @author Daniel McCoy Stephenson
 */
class UsdRecording {
    public:
        static constexpr double TICKS_PER_SECOND = 10.0;

        UsdRecording();
        void recordTick(Microbiome& microbiome, size_t tick);
        size_t getTicksRecorded();
        std::string toUsda();
        void write(const std::string& path);
    private:
        usd_recorder::UsdaRecorder recorder;
        size_t ticksRecorded = 0;
};

#endif
