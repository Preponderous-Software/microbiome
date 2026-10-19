#include "replay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "usd_recorder/replay_writer.hpp"

namespace mbstrata {

namespace {

const char* const kSetNames[kSpeciesCount] = {"aerobes", "fermenters", "cross_feeders"};
const char* const kSetLabels[kSpeciesCount] = {"Aerobes", "Fermenters (biofilm)", "Cross-feeders"};
constexpr float kSwimZ = 0.7f;
constexpr float kAttachedZ = 0.35f;

uint8_t toSrgb8(float linear) {
    const float c = std::min(std::max(linear, 0.0f), 1.0f);
    const float s = c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(std::lround(s * 255.0f));
}

usd_recorder::Rgb8 rgb8(const float rgb[3]) { return {toSrgb8(rgb[0]), toSrgb8(rgb[1]), toSrgb8(rgb[2])}; }

// Cells [begin, end) of the full grid that coarse cell k of n covers.
std::array<int, 2> span(int k, int n, int full) { return {k * full / n, (k + 1) * full / n}; }

}  // namespace

ReplayRecorder::ReplayRecorder(const MicroEnvironment& env, const ReplayOptions& options) : options_(options) {
    if (options_.every < 1) throw std::invalid_argument("the replay interval must be at least 1 tick");
    const Config& cfg = env.config();
    width_ = cfg.width;
    height_ = cfg.height;
    cols_ = std::max(1, std::min(options_.fieldColumns, width_));
    cell_ = static_cast<float>(width_) / static_cast<float>(cols_);
    rows_ = std::max(1, std::min(height_, static_cast<int>(std::lround(static_cast<float>(height_) / cell_))));

    usd_recorder::ReplayMeta meta;
    meta.sim = "microbiome";
    meta.sim_version = options_.simVersion.empty() ? "unknown" : options_.simVersion;
    meta.seed = std::to_string(cfg.seed);
    meta.tick_rate = options_.ticksPerSecond;
    meta.playback_rate = options_.playbackRate;
    meta.state_hash_algorithm = "MicroEnvironment::stateHash (strata::sim::StateHasher, FNV-1a 64)";
    meta.title = "Microbiome micro-environment";
    meta.description = "Aerobes, fermenters and cross-feeders in a 2D chamber with diffusing nutrient, waste and oxygen "
                       "(mb_strata, Microbiome on Strata).";
    meta.source = options_.source;
    meta.number_params["width"] = cfg.width;
    meta.number_params["height"] = cfg.height;
    meta.number_params["initialPerSpecies"] = cfg.initialPerSpecies;
    meta.number_params["every"] = static_cast<double>(options_.every);
    meta.string_params["fieldView"] = fieldViewName(options_.view);
    writer_ = std::make_unique<usd_recorder::ReplayWriter>(meta);

    for (int s = 0; s < kSpeciesCount; ++s) {
        writer_->entity_set(kSetNames[s], kSetLabels[s], rgb8(traitsOf(static_cast<Species>(s)).color), 0.9f, "sphere");
    }
    writer_->field("chamber", std::string("Chamber (") + fieldViewName(options_.view) + ")", cols_, rows_, 0.0, 0.0,
                   static_cast<double>(cell_), 0.0);

    // Grains: a cube on every coarse cell that is mostly rock.
    std::vector<std::array<float, 3>> rocks;
    const auto& mask = env.grains();
    for (int r = 0; r < rows_; ++r) {
        const auto ys = span(r, rows_, height_);
        for (int c = 0; c < cols_; ++c) {
            const auto xs = span(c, cols_, width_);
            int blocked = 0, total = 0;
            for (int y = ys[0]; y < ys[1]; ++y)
                for (int x = xs[0]; x < xs[1]; ++x) {
                    ++total;
                    blocked += mask.blocked(x, y) ? 1 : 0;
                }
            if (total && blocked * 2 > total) {
                // Coarse row r counts from the air interface (y = 0); replay y is flipped.
                rocks.push_back({(static_cast<float>(c) + 0.5f) * cell_, static_cast<float>(height_) - (static_cast<float>(r) + 0.5f) * cell_,
                                 0.5f * cell_});
            }
        }
    }
    if (!rocks.empty()) writer_->scene_points("grains", "Grains", rocks, {cell_}, {rgb8(kGrainRgb)}, "cube");
}

ReplayRecorder::~ReplayRecorder() = default;

void ReplayRecorder::observe(const MicroEnvironment& env) {
    if (env.tick() % static_cast<uint64_t>(options_.every) == 0) record(env);
}

void ReplayRecorder::finish(const MicroEnvironment& env) { record(env); }

void ReplayRecorder::record(const MicroEnvironment& env) {
    const uint64_t tick = env.tick();
    if (any_ && tick <= last_) return;
    char hash[17];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(env.stateHash()));
    writer_->begin_sample(static_cast<int64_t>(tick), hash);

    std::array<std::vector<int64_t>, kSpeciesCount> ids;
    std::array<std::vector<float>, kSpeciesCount> xyz;
    const float h = static_cast<float>(height_);
    env.world().each<Body, Cell>([&](strata::sim::Entity e, const Body& b, const Cell& c) {
        const int s = static_cast<int>(c.species);
        ids[s].push_back(static_cast<int64_t>((static_cast<uint64_t>(e.generation) << 32) | e.index));
        xyz[s].insert(xyz[s].end(), {b.x, h - b.y, b.attached ? kAttachedZ : kSwimZ});
    });
    for (int s = 0; s < kSpeciesCount; ++s) writer_->entities(kSetNames[s], ids[s].data(), xyz[s].data(), ids[s].size());

    // The field: block averages of the mapped levels, grains in rock colour.
    std::vector<usd_recorder::Rgb8> colors(static_cast<size_t>(cols_) * static_cast<size_t>(rows_));
    const auto& mask = env.grains();
    const auto& nut = env.nutrient().values();
    const auto& waste = env.waste().values();
    const auto& oxy = env.oxygen().values();
    const float oxyScale = std::max(env.config().oxygenSurface, 1e-3f);
    for (int r = 0; r < rows_; ++r) {
        const auto ys = span(r, rows_, height_);
        for (int c = 0; c < cols_; ++c) {
            const auto xs = span(c, cols_, width_);
            float tn = 0, tw = 0, to = 0;
            int open = 0, total = 0;
            for (int y = ys[0]; y < ys[1]; ++y)
                for (int x = xs[0]; x < xs[1]; ++x) {
                    ++total;
                    if (mask.blocked(x, y)) continue;
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
                    tn += saturateConcentration(nut[i], kNutrientHalf);
                    tw += saturateConcentration(waste[i], kWasteHalf);
                    to += std::min(oxy[i] / oxyScale, 1.0f);
                    ++open;
                }
            float rgb[3] = {kGrainRgb[0], kGrainRgb[1], kGrainRgb[2]};
            if (open * 2 >= total && open > 0) {
                const float k = 1.0f / static_cast<float>(open);
                fieldRgb(options_.view, tn * k, tw * k, to * k, rgb);
            }
            // Field row 0 is the replay's minimum y: the chamber's bottom row.
            colors[static_cast<size_t>(rows_ - 1 - r) * static_cast<size_t>(cols_) + static_cast<size_t>(c)] = rgb8(rgb);
        }
    }
    writer_->field_colors("chamber", colors.data());

    const Stats st = env.stats();
    writer_->stat("aerobes", st.alive[0]);
    writer_->stat("fermenters", st.alive[1]);
    writer_->stat("cross_feeders", st.alive[2]);
    writer_->stat("biofilm", st.attached);
    writer_->stat("births", static_cast<double>(st.births));
    writer_->stat("deaths", static_cast<double>(st.deaths));
    writer_->stat("nutrient", st.nutrientTotal);
    writer_->stat("waste", st.wasteTotal);
    writer_->stat("oxygen", st.oxygenTotal);
    writer_->end_sample();
    ++samples_;
    any_ = true;
    last_ = tick;
}

void ReplayRecorder::write(const std::string& path) const { writer_->write(path); }

std::string ReplayRecorder::usda() const { return writer_->usda(); }

}  // namespace mbstrata
