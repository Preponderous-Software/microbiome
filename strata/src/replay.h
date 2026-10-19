#ifndef MicrobiomeStrataReplay_h
#define MicrobiomeStrataReplay_h

#include <cstdint>
#include <memory>
#include <string>

#include "field_view.h"
#include "micro_environment.h"

namespace usd_recorder {
class ReplayWriter;
}

/**
 * Records a run of the micro-environment as a usd-replay/1 replay (the
 * contract in usd-recorder's docs/REPLAY.md), written by usd-recorder's
 * vendored, dependency-free C++ writer (vendor/usd_recorder/). One sample every
 * `every` ticks, plus the first and the last:
 *
 *   - one Points set per species (aerobes, fermenters, cross-feeders), ids
 *     stable for an entity's life ((generation << 32) | slot);
 *   - one field, the chamber's heatmap in `view`, block-averaged down to
 *     `fieldColumns` columns;
 *   - the grains as static cubes on that coarse grid (the Scene);
 *   - stats: population per species, biofilm cells, births, deaths and the
 *     chamber totals of nutrient, waste and oxygen;
 *   - the state hash is MicroEnvironment::stateHash() at each sample tick.
 *
 * It only reads the simulation (every method takes it const), so a run gives
 * the same state hash with and without recording; a test checks that tick by
 * tick. Replay x/y are chamber cells with y flipped (the air interface, row 0,
 * is at the far edge, +y), z is up.
 * @author Daniel McCoy Stephenson
 */
namespace mbstrata {

struct ReplayOptions {
    long every = 10;            // ticks between samples
    int fieldColumns = 32;      // field resolution across the chamber (rows follow the aspect)
    FieldView view = FieldView::Composite;
    double ticksPerSecond = 30; // the sim's clock (usdReplay.tickRate)
    double playbackRate = 60;   // suggested ticks per second of playback
    std::string simVersion;     // version (+ commit) of this build
    std::string source;         // the command line, for the record
};

class ReplayRecorder {
    public:
        ReplayRecorder(const MicroEnvironment& env, const ReplayOptions& options);
        ~ReplayRecorder();
        ReplayRecorder(const ReplayRecorder&) = delete;
        ReplayRecorder& operator=(const ReplayRecorder&) = delete;

        // Call after every tick (and once before the first): records a sample
        // when the tick is a multiple of `every`.
        void observe(const MicroEnvironment& env);
        // Records the current tick if it was not recorded yet (the last one).
        void finish(const MicroEnvironment& env);
        // Writes the replay (.usda) atomically.
        void write(const std::string& path) const;
        // The replay as .usda text.
        std::string usda() const;

        size_t samples() const { return samples_; }
        int fieldColumns() const { return cols_; }
        int fieldRows() const { return rows_; }

    private:
        void record(const MicroEnvironment& env);

        ReplayOptions options_;
        std::unique_ptr<usd_recorder::ReplayWriter> writer_;
        int width_ = 0, height_ = 0, cols_ = 0, rows_ = 0;
        float cell_ = 1.0f;
        size_t samples_ = 0;
        bool any_ = false;
        uint64_t last_ = 0;
};

}  // namespace mbstrata

#endif
