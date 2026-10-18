#ifndef MicrobiomeStrataHud_h
#define MicrobiomeStrataHud_h

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "strata/strata.h"

#include "micro_environment.h"
#include "scene.h"

namespace strata {
class Engine;
}

/**
 * The on-screen statistics drawn over the micro-environment with Strata's
 * unlit overlays (ABI 1.5): 8x8 bitmap text, lines and points. It reads the
 * simulation and never changes it, so turning it on or off cannot change a
 * run's state hash.
 * @author Daniel McCoy Stephenson
 */
namespace mbstrata {

// Population per species and the chamber totals every `everyTicks` ticks,
// the most recent `capacity` samples. Feeds the HUD's sparkline and its
// per-tick changes.
class StatsHistory {
    public:
        enum Total { Nutrient = 0, Waste = 1, Oxygen = 2, CellEnergy = 3, TotalCount = 4 };
        struct Sample {
            uint64_t tick = 0;
            std::array<int, kSpeciesCount> alive{};
            std::array<double, TotalCount> totals{};
            int population() const;
        };

        explicit StatsHistory(int capacity = 240, int everyTicks = 4);
        // Records a sample when env.tick() is a multiple of everyTicks and
        // newer than the last sample; otherwise does nothing.
        void observe(const MicroEnvironment& env);
        void clear() { samples_.clear(); }
        const std::deque<Sample>& samples() const { return samples_; }
        int capacity() const { return capacity_; }
        int everyTicks() const { return everyTicks_; }

    private:
        int capacity_;
        int everyTicks_;
        std::deque<Sample> samples_;
};

// Maps a pixel of the drawable onto a world point just in front of the
// camera, where a world-space overlay line or point lands on that pixel.
// Works for any perspective StrataCamera.
class ScreenProjector {
    public:
        ScreenProjector(const StrataCamera& camera, int width, int height);
        void toWorld(float px, float py, float out[3]) const;

    private:
        float eye_[3]{}, right_[3]{}, up_[3]{}, forward_[3]{};
        float depth_ = 1.0f, sx_ = 1.0f, sy_ = 1.0f;
        int width_, height_;
};

// What the HUD shows besides the simulation itself.
struct HudState {
    double ticksPerSecond = 30.0;  // the base rate (--tps)
    double speed = 1.0;            // the time scale on top of it
    bool paused = false;
    bool headless = false;
    double measuredTicksPerSecond = -1.0;  // < 0: not measured
    FieldView view = FieldView::Composite;
    uint64_t seed = 1;
};

class Hud {
    public:
        // Glyph cell size for a drawable `height` pixels tall: 8 or a
        // multiple of 8, so the bitmap font stays crisp.
        static float pixelSizeFor(int height);

        // Lays out this frame's overlays. `camera` is the one the frame is
        // drawn with. pixelSize <= 0 picks pixelSizeFor(height).
        void build(const MicroEnvironment& env, const StatsHistory& history, const HudState& state,
                   const StrataCamera& camera, int width, int height, float pixelSize = 0.0f);
        // Submits what build() laid out; call between beginFrame and endFrame.
        void submit(strata::Engine& engine) const;
        const std::vector<StrataText>& texts() const { return texts_; }
        const std::vector<StrataLineVertex>& lines() const { return lines_; }
        const std::vector<StrataPoint>& points() const { return points_; }

    private:
        struct PendingText {
            float x, y, size;
            float color[4];
            std::string text;
        };
        void text(float x, float y, const std::string& s, const float color[4], float size = 0.0f);
        void line(float x0, float y0, float x1, float y1, const float c0[4], const float c1[4]);
        void fillRect(float x0, float y0, float x1, float y1, const float color[4]);
        void horizontalGradient(float x0, float y0, float x1, float y1, FieldView view, int channel);
        void disc(float x, float y, float diameter, const float color[4]);

        const ScreenProjector* projector_ = nullptr;
        float s_ = 8.0f;
        std::vector<PendingText> pending_;
        std::vector<StrataText> texts_;
        std::vector<StrataLineVertex> lines_;
        std::vector<StrataPoint> points_;
};

}  // namespace mbstrata

#endif
