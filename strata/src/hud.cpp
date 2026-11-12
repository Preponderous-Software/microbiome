#include "hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "strata/strata.hpp"

namespace mbstrata {

namespace {

const float kWhite[4] = {0.92f, 0.95f, 1.00f, 1.0f};
const float kDim[4] = {0.50f, 0.56f, 0.64f, 1.0f};
const float kTitle[4] = {0.45f, 1.00f, 0.55f, 1.0f};
const float kPaused[4] = {1.00f, 0.38f, 0.30f, 1.0f};
const float kPanel[4] = {0.0f, 0.0f, 0.0f, 0.58f};
const float kFrame[4] = {0.50f, 0.56f, 0.64f, 0.55f};
const float kDanger[4] = {1.00f, 0.42f, 0.36f, 1.0f};
const float kWarn[4] = {1.00f, 0.80f, 0.30f, 1.0f};
const float kMutation[4] = {0.80f, 0.65f, 1.00f, 1.0f};  // mutations: a soft violet
const float kGood[4] = {0.45f, 1.00f, 0.55f, 1.0f};

// Joins items into rows of at most `perRow`, two spaces apart.
std::vector<std::string> joinRows(const std::vector<std::string>& items, size_t perRow) {
    std::vector<std::string> rows;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i % perRow == 0) rows.emplace_back();
        else rows.back() += "  ";
        rows.back() += items[i];
    }
    return rows;
}

std::string format(const char* fmt, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, fmt, v);
    return buf;
}

// 1234.5 -> "1234.5", 123456 -> "123.5k", 1.2e7 -> "12.00M".
std::string quantity(double v) {
    const double a = std::fabs(v);
    if (a < 1e4) return format("%.1f", v);
    if (a < 1e6) return format("%.1fk", v / 1e3);
    if (a < 1e9) return format("%.2fM", v / 1e6);
    return format("%.2fG", v / 1e9);
}

std::string signedQuantity(double v) { return (v >= 0 ? "+" : "") + quantity(v); }

std::string padRight(std::string s, size_t width) {
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

std::string padLeft(const std::string& s, size_t width) {
    return s.size() >= width ? s : std::string(width - s.size(), ' ') + s;
}

// A species colour bright enough to read as text.
void speciesTextColor(Species sp, float out[4]) {
    const SpeciesTraits& t = traitsOf(sp);
    const float m = std::max({t.color[0], t.color[1], t.color[2], 1e-3f});
    for (int i = 0; i < 3; ++i) out[i] = 0.25f + 0.75f * t.color[i] / m;
    out[3] = 1.0f;
}

}  // namespace

int StatsHistory::Sample::population() const {
    int n = 0;
    for (int a : alive) n += a;
    return n;
}

StatsHistory::StatsHistory(int capacity, int everyTicks)
    : capacity_(std::max(2, capacity)), everyTicks_(std::max(1, everyTicks)) {}

void StatsHistory::observe(const MicroEnvironment& env) {
    const uint64_t tick = env.tick();
    if (tick % static_cast<uint64_t>(everyTicks_) != 0) return;
    if (!samples_.empty() && samples_.back().tick >= tick) {
        if (samples_.back().tick == tick) return;
        samples_.clear();  // the chamber was replaced: start over
    }
    const Stats st = env.stats();
    Sample s;
    s.tick = tick;
    s.alive = st.alive;
    s.totals = {st.nutrientTotal, st.wasteTotal, st.oxygenTotal, st.organismEnergy};
    samples_.push_back(s);
    while (static_cast<int>(samples_.size()) > capacity_) samples_.pop_front();
}

ScreenProjector::ScreenProjector(const StrataCamera& c, int width, int height)
    : width_(std::max(1, width)), height_(std::max(1, height)) {
    // The view matrix is column-major; its rotation rows are the camera's
    // right, up and backward axes in world space.
    for (int i = 0; i < 3; ++i) {
        right_[i] = c.view[i * 4 + 0];
        up_[i] = c.view[i * 4 + 1];
        forward_[i] = -c.view[i * 4 + 2];
        eye_[i] = c.eye[i];
    }
    // Just past the near plane: nothing in the scene is in front of it.
    depth_ = c.near_plane * 1.5f;
    // A view-space point (x, y, -d) lands at ndc (P00 x / d, P11 y / d).
    sx_ = depth_ / std::max(std::fabs(c.projection[0]), 1e-6f);
    sy_ = depth_ / std::max(std::fabs(c.projection[5]), 1e-6f);
}

void ScreenProjector::toWorld(float px, float py, float out[3]) const {
    const float nx = 2.0f * px / static_cast<float>(width_) - 1.0f;
    const float ny = 1.0f - 2.0f * py / static_cast<float>(height_);
    for (int i = 0; i < 3; ++i)
        out[i] = eye_[i] + forward_[i] * depth_ + right_[i] * (nx * sx_) + up_[i] * (ny * sy_);
}

float Hud::pixelSizeFor(int height) {
    const int k = static_cast<int>(std::lround(static_cast<double>(height) / 450.0));
    return 8.0f * static_cast<float>(std::min(std::max(k, 1), 4));
}

void Hud::text(float x, float y, const std::string& s, const float color[4], float size) {
    PendingText t{x, y, size > 0 ? size : s_, {color[0], color[1], color[2], color[3]}, s};
    pending_.push_back(std::move(t));
}

void Hud::line(float x0, float y0, float x1, float y1, const float c0[4], const float c1[4]) {
    StrataLineVertex a{}, b{};
    projector_->toWorld(x0, y0, a.position);
    projector_->toWorld(x1, y1, b.position);
    std::copy(c0, c0 + 4, a.color);
    std::copy(c1, c1 + 4, b.color);
    lines_.push_back(a);
    lines_.push_back(b);
}

void Hud::fillRect(float x0, float y0, float x1, float y1, const float color[4]) {
    // One horizontal line through the centre of every pixel row.
    for (float y = std::floor(y0) + 0.5f; y < y1; y += 1.0f) line(x0, y, x1, y, color, color);
}

void Hud::horizontalGradient(float x0, float y0, float x1, float y1, FieldView view, int channel) {
    const int segments = 32;
    for (float y = std::floor(y0) + 0.5f; y < y1; y += 1.0f) {
        for (int i = 0; i < segments; ++i) {
            float c0[4] = {0, 0, 0, 1}, c1[4] = {0, 0, 0, 1};
            float t[3] = {0, 0, 0};
            t[channel] = static_cast<float>(i) / segments;
            fieldRgb(view, t[0], t[1], t[2], c0);
            t[channel] = static_cast<float>(i + 1) / segments;
            fieldRgb(view, t[0], t[1], t[2], c1);
            const float xa = x0 + (x1 - x0) * static_cast<float>(i) / segments;
            const float xb = x0 + (x1 - x0) * static_cast<float>(i + 1) / segments;
            line(xa, y, xb, y, c0, c1);
        }
    }
}

void Hud::disc(float x, float y, float diameter, const float color[4]) {
    StrataPoint p{};
    projector_->toWorld(x, y, p.position);
    p.size_pixels = diameter;
    p.world_diameter = 0.0f;
    std::copy(color, color + 4, p.color);
    points_.push_back(p);
}

void Hud::build(const MicroEnvironment& env, const StatsHistory& history, const HudState& state,
                const StrataCamera& camera, int width, int height, float pixelSize) {
    pending_.clear();
    texts_.clear();
    lines_.clear();
    points_.clear();
    const ScreenProjector projector(camera, width, height);
    projector_ = &projector;
    s_ = pixelSize > 0 ? pixelSize : pixelSizeFor(height);
    const float s = s_;
    const float lh = std::round(s * 1.25f);  // line height
    const float m = s;                       // margin
    const float pad = std::round(s * 0.5f);
    const float W = static_cast<float>(width);
    const float H = static_cast<float>(height);
    const Stats st = env.stats();
    const Config& cfg = env.config();

    // While a cell is embodied on a narrow screen, its own panel takes the
    // top and the chamber's statistics step aside.
    const bool compact = env.player().embodied && W < 64.0f * s;
    const auto& samples = history.samples();

    // ---- top left: clock, populations and chamber totals
    if (!compact) {
        const float colW = 30.0f * s;
        float y = m + pad;
        const float x = m + pad;
        std::vector<std::pair<float, float>> speciesRows;
        auto row = [&](const std::string& t, const float* c) {
            text(x, y, t, c);
            y += lh;
        };
        row("MICROBIOME on Strata", kTitle);
        row("tick " + std::to_string(st.tick) + "  t " + format("%.1f", static_cast<double>(st.tick) / state.ticksPerSecond) +
                " s  seed " + std::to_string(state.seed),
            kWhite);
        if (state.headless) {
            row(format("%.0f ticks/s of sim time", state.ticksPerSecond), kDim);
        } else if (state.paused) {
            row("PAUSED  (space resumes)", kPaused);
        } else {
            std::string r = format("speed x%.2g", state.speed) + format("  %.4g tps", state.ticksPerSecond * state.speed);
            if (state.measuredTicksPerSecond >= 0) r += format("  ~%.0f/s", state.measuredTicksPerSecond);
            row(r, kWhite);
        }
        y += lh * 0.5f;
        row("population " + std::to_string(st.population()) + "  biofilm " + std::to_string(st.attached), kWhite);
        for (int i = 0; i < kSpeciesCount; ++i) {
            const Species sp = static_cast<Species>(i);
            float c[4];
            speciesTextColor(sp, c);
            speciesRows.emplace_back(x + s * 0.75f, y + s * 0.5f);
            text(x + s * 2.0f, y, padRight(traitsOf(sp).name, 14) + padLeft(std::to_string(st.alive[i]), 6), c);
            y += lh;
        }
        row("births " + std::to_string(st.births) + "  deaths " + std::to_string(st.deaths), kDim);
        y += lh * 0.5f;
        row(padRight("chamber totals", 15) + padLeft("sum", 8) + padLeft("/tick", 8), kDim);
        const bool haveRate = samples.size() >= 2 && samples.back().tick == st.tick;
        const char* names[StatsHistory::TotalCount] = {"nutrient", "waste", "oxygen", "cell energy"};
        const double totals[StatsHistory::TotalCount] = {st.nutrientTotal, st.wasteTotal, st.oxygenTotal,
                                                         st.organismEnergy};
        for (int i = 0; i < StatsHistory::TotalCount; ++i) {
            std::string rate = "--";
            if (haveRate) {
                const auto& a = samples[samples.size() - 2];
                const auto& b = samples.back();
                rate = signedQuantity((b.totals[i] - a.totals[i]) / static_cast<double>(b.tick - a.tick));
            }
            row(padRight(names[i], 15) + padLeft(quantity(totals[i]), 8) + padLeft(rate, 8), kWhite);
        }
        row("seeps add " + format("%g", static_cast<double>(cfg.nutrientSources) * cfg.nutrientSourceRate) + " nutrient/tick",
            kDim);
        const float panelBottom = y + pad - (lh - s);
        fillRect(m, m, m + colW + 2 * pad, panelBottom, kPanel);
        for (int i = 0; i < kSpeciesCount; ++i) {
            const SpeciesTraits& t = traitsOf(static_cast<Species>(i));
            const float c[4] = {t.color[0], t.color[1], t.color[2], 1.0f};
            disc(speciesRows[i].first, speciesRows[i].second, s * 0.9f, c);
        }
    }

    // ---- bottom left: what the colours mean
    const float legW = 30.0f * s;
    const float legH = lh * 3.0f + s * 2.0f + pad * 2;
    const float lx = m, ly = H - m - legH;
    fillRect(lx, ly, lx + legW + 2 * pad, ly + legH, kPanel);
    const float tx = lx + pad;
    float ty = ly + pad;
    const int viewIndex = static_cast<int>(state.view);
    const char* mapName[] = {"composite", "nutrient (viridis)", "waste (magma)", "oxygen (ice)"};
    text(tx, ty, std::string("view ") + std::to_string(viewIndex + 1) + "/4 " + mapName[viewIndex], kWhite);
    ty += lh;
    if (state.view == FieldView::Composite) {
        struct Swatch {
            const char* name;
            float rgb[3];
        };
        Swatch sw[4] = {{"nutrient", {}}, {"waste", {}}, {"oxygen", {}}, {"grain", {kGrainRgb[0], kGrainRgb[1], kGrainRgb[2]}}};
        fieldRgb(FieldView::Composite, 1, 0, 0, sw[0].rgb);
        fieldRgb(FieldView::Composite, 0, 1, 0, sw[1].rgb);
        fieldRgb(FieldView::Composite, 0, 0, 1, sw[2].rgb);
        for (int i = 0; i < 4; ++i) {
            const float cx = tx + static_cast<float>(i % 2) * 15.0f * s;
            const float cy = ty + static_cast<float>(i / 2) * lh;
            // Brightened so the swatch reads against the dark panel.
            const float k = 1.0f / std::max({sw[i].rgb[0], sw[i].rgb[1], sw[i].rgb[2], 1e-3f});
            const float c[4] = {sw[i].rgb[0] * k, sw[i].rgb[1] * k, sw[i].rgb[2] * k, 1.0f};
            fillRect(cx, cy, cx + s * 1.5f, cy + s, c);
            text(cx + s * 2.0f, cy, sw[i].name, kWhite);
        }
        text(tx, ty + 2 * lh, "brighter cells have more energy", kDim);
    } else {
        const int channel = viewIndex - 1;
        const float bx0 = tx, bx1 = tx + 28.0f * s;
        const float by0 = ty, by1 = ty + s * 1.25f;
        horizontalGradient(bx0, by0, bx1, by1, state.view, channel);
        // Tick labels at concentrations, placed where the map puts them.
        std::vector<std::pair<float, std::string>> ticks;
        if (state.view == FieldView::Oxygen) {
            const float top = cfg.oxygenSurface;
            for (float v : {0.0f, top * 0.5f, top}) ticks.emplace_back(v / top, format("%g", v));
        } else {
            const float half = state.view == FieldView::Nutrient ? kNutrientHalf : kWasteHalf;
            for (float v : {0.0f, half, half * 3, half * 9}) ticks.emplace_back(saturateConcentration(v, half), format("%g", v));
        }
        for (const auto& t : ticks) {
            const float px = bx0 + (bx1 - bx0) * t.first;
            line(px + 0.5f, by1, px + 0.5f, by1 + s * 0.5f, kWhite, kWhite);
            const float tw = static_cast<float>(t.second.size()) * s;
            text(std::min(std::max(px - tw * 0.5f, bx0), bx1 - tw), by1 + s * 0.75f, t.second, kWhite);
        }
        text(tx, by1 + s * 0.75f + lh, state.view == FieldView::Oxygen ? "per cell, linear to the air"
                                                                        : "per cell, saturating",
             kDim);
    }

    // ---- bottom right: population sparkline. While a cell is embodied its own
    // panel matters more, and on the web page the Dash and Toxin buttons sit here.
    if (!env.player().embodied) {
        const float spW = std::min(28.0f * s, W * 0.4f);
        const float spH = s * 6.0f;
        const float sx0 = W - m - spW - 2 * pad, sy0 = H - m - spH - 2 * lh - 2 * pad;
        fillRect(sx0, sy0, W - m, H - m, kPanel);
        const float gx0 = sx0 + pad, gx1 = W - m - pad;
        const float gy0 = sy0 + pad + lh, gy1 = H - m - pad - lh;
        const int span = history.capacity() * history.everyTicks();
        text(gx0, sy0 + pad, "population", kWhite);
        // The time axis: the window's span on the left, the present on the right.
        text(gx0, gy1 + (lh - s), "-" + std::to_string(span) + " ticks", kDim);
        text(gx1 - 3.0f * s, gy1 + (lh - s), "now", kDim);
        line(gx0, gy1 - 0.5f, gx1, gy1 - 0.5f, kFrame, kFrame);
        line(gx0 + 0.5f, gy0, gx0 + 0.5f, gy1, kFrame, kFrame);
        if (samples.size() >= 2) {
            int peak = 1;
            for (const auto& smp : samples) peak = std::max(peak, smp.population());
            const std::string peakLabel = "max " + std::to_string(peak);
            text(gx1 - static_cast<float>(peakLabel.size()) * s, sy0 + pad, peakLabel, kDim);
            const float dx = (gx1 - gx0) / static_cast<float>(history.capacity() - 1);
            // Newest sample at the right edge.
            const float xStart = gx1 - dx * static_cast<float>(samples.size() - 1);
            auto yOf = [&](int v) { return gy1 - 1.0f - (gy1 - gy0 - 2.0f) * static_cast<float>(v) / static_cast<float>(peak); };
            for (int series = -1; series < kSpeciesCount; ++series) {
                float c[4];
                if (series < 0) std::copy(kWhite, kWhite + 4, c);
                else speciesTextColor(static_cast<Species>(series), c);
                for (size_t i = 1; i < samples.size(); ++i) {
                    const int a = series < 0 ? samples[i - 1].population() : samples[i - 1].alive[series];
                    const int b = series < 0 ? samples[i].population() : samples[i].alive[series];
                    line(xStart + dx * static_cast<float>(i - 1), yOf(a), xStart + dx * static_cast<float>(i), yOf(b), c, c);
                }
            }
        }
    }

    // ---- top right: keys (window only)
    const Player& pl = env.player();
    if (!state.headless) {
        const std::string keys = pl.embodied ? "Tab HUD  F leave" : "F become a cell  Tab HUD  H help";
        text(W - m - static_cast<float>(keys.size()) * s, m, keys, pl.embodied ? kDim : kTitle);
    }
    if (pl.started) survival(env, state, W, H);
#if !defined(__EMSCRIPTEN__)
    if (pl.offerCount > 0) mutationOffer(env, W, H);  // the page shows its own buttons instead
#endif

    // Strings are final now, so their c_str() pointers stay valid.
    texts_.reserve(pending_.size());
    for (const auto& p : pending_) {
        StrataText t{};
        t.x = p.x;
        t.y = p.y;
        t.pixel_size = p.size;
        t.flags = STRATA_TEXT_SHADOW;
        std::copy(p.color, p.color + 4, t.color);
        t.utf8 = p.text.c_str();
        texts_.push_back(t);
    }
    projector_ = nullptr;
}

void Hud::survival(const MicroEnvironment& env, const HudState& state, float W, float H) {
    const Player& pl = env.player();
    const Config& cfg = env.config();
    const float s = s_;
    const float lh = std::round(s * 1.25f);
    const float m = s;
    const float pad = std::round(s * 0.5f);
    const double tps = state.ticksPerSecond;
    const int lineage = env.lineageAlive();
    float c[4];
    speciesTextColor(pl.species, c);

    if (pl.embodied) {
        // ---- top right, under the keys: the embodied cell's vital signs.
        const float panelW = 32.0f * s;
        const float x0 = W - m - panelW - 2 * pad, x = x0 + pad;
        float y = m + lh + pad;
        const float top = y - pad;
        std::vector<std::pair<const float*, std::string>> rows;
        text(x, y, std::string("YOU: ") + traitsOf(pl.species).name, c);
        const std::string alive = format("alive %.1f s", static_cast<double>(env.playerAgeTicks()) / tps);
        text(x + panelW - static_cast<float>(alive.size()) * s, y, alive, kWhite);
        y += lh;
        // Energy bar: empty is death, the tick mark is where it divides.
        const float barX0 = x, barX1 = x + panelW, barY0 = y, barY1 = y + s;
        const float full = cfg.reproductionEnergyThreshold;
        const float frac = std::min(std::max(pl.energy / full, 0.0f), 1.0f);
        const float* barColor = frac < 0.2f ? kDanger : frac < 0.45f ? kWarn : kGood;
        fillRect(barX0, barY0, barX1, barY1, kPanel);
        fillRect(barX0, barY0, barX0 + (barX1 - barX0) * frac, barY1, barColor);
        line(barX0, barY0 - 0.5f, barX1, barY0 - 0.5f, kFrame, kFrame);
        line(barX0, barY1 + 0.5f, barX1, barY1 + 0.5f, kFrame, kFrame);
        y += s + pad;
        text(x, y, format("energy %.0f", static_cast<double>(pl.energy)) + format(" / divide at %.0f", full), kDim);
        y += lh;
        // What is hurting it right now, worst first.
        float nearest = 1e9f;
        for (const Predator& p : env.predators()) {
            const float dx = p.x - pl.lastX, dy = p.y - pl.lastY;
            nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy));
        }
        if (nearest < cfg.predatorSense) {
            text(x, y, format("AMOEBA %.0f cells away!", static_cast<double>(nearest)), kDanger);
            y += lh;
        }
        if (pl.oxygenDrain > 0.0f) {
            text(x, y, format("OXYGEN BURNS -%.1f/tick: go deeper", static_cast<double>(pl.oxygenDrain)), kDanger);
            y += lh;
        }
        if (pl.wasteDrain > 0.0f) {
            text(x, y, format("WASTE BURNS -%.1f/tick: move on", static_cast<double>(pl.wasteDrain)), kDanger);
            y += lh;
        }
        if (pl.energy < full * 0.2f) {
            text(x, y, traitsOf(pl.species).food == Food::Waste ? "STARVING: find waste" : "STARVING: find nutrient",
                 kWarn);
            y += lh;
        } else if (pl.meal > 0.0f) {
            text(x, y, format("feeding +%.1f/tick", static_cast<double>(pl.meal)), kGood);
            y += lh;
        }
        text(x, y, pl.toxinReady == 0 ? "toxin READY (space)" : format("toxin in %.1f s", pl.toxinReady / tps),
             pl.toxinReady == 0 ? kGood : kDim);
        text(x + 22.0f * s, y, pl.dashing ? "DASHING" : "shift dash", pl.dashing ? kWarn : kDim);
        y += lh;
        text(x, y, "amoebae " + std::to_string(env.predators().size()) + "  killed " + std::to_string(pl.amoebaeKilled) +
                       "  rivals " + std::to_string(pl.rivalsKilled),
             kWhite);
        y += lh;
        text(x, y, "divisions " + std::to_string(pl.divisions) + "  lineage " + std::to_string(lineage) + " alive", kWhite);
        y += lh;
        // Mutations so far, two to a row: "flagella 2  hardy 1".
        for (const auto& row : joinRows(mutationItems(pl), 2)) {
            text(x, y, row, kMutation);
            y += lh;
        }
        if (pl.level(Mutation::Membrane) > 0) {
            text(x, y, "membrane " + std::to_string(pl.membrane) + "/" + std::to_string(pl.level(Mutation::Membrane)),
                 pl.membrane > 0 ? kGood : kWarn);
            y += lh;
        }
        fillRect(x0, top, W - m, y - (lh - s) + pad, kPanel);
        return;
    }

    if (pl.cause == DeathCause::None) return;  // left the body on purpose
    // ---- centre: how the last life ended, what the lineage carried (the
    // recap) and what to do next. On a narrow screen the same lines are
    // broken up, and the glyphs shrink only if that is still not enough.
    const std::string head = std::string("YOU WERE ") + deathCauseText(pl.cause);
    std::string upper;
    for (char ch : head) upper += static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
    const std::string survived = format("survived %.1f s", static_cast<double>(pl.diedTick - pl.sinceTick) / tps) +
                                 format(" (best %.1f s)", static_cast<double>(pl.bestTicks) / tps);
    const std::string tally =
        "divisions " + std::to_string(pl.divisions) + "  amoebae killed " + std::to_string(pl.amoebaeKilled);
    const auto items = mutationItems(pl);
    const std::string run = "run: " + std::to_string(pl.lives) + (pl.lives == 1 ? " life" : " lives") +
                            "  rivals killed " + std::to_string(pl.rivalsKilled);

    struct Line {
        std::string text;
        const float* color;
        bool block;        // left-aligned with the other block lines (the recap), else centred
        bool gap = false;  // a little air before it (the call to action)
    };
    const auto layout = [&](bool narrow) {
        std::vector<Line> lines;
        if (narrow) {
            lines.push_back({survived, kWhite, false});
            lines.push_back({tally, kWhite, false});
        } else {
            lines.push_back({survived + "  " + tally, kWhite, false});
        }
        if (items.empty()) {
            lines.push_back({"no mutations yet", kMutation, true});
        } else {
            const auto rows = joinRows(items, narrow ? 1 : 3);
            for (size_t i = 0; i < rows.size(); ++i)
                lines.push_back({(i == 0 ? "mutations: " : "           ") + rows[i], kMutation, true});
        }
        if (lineage == 0) lines.push_back({run, kWhite, true});
        if (lineage > 1) {
            lines.push_back({"F: live on as one of your " + std::to_string(lineage) + " descendants", kGood, false, true});
        } else if (lineage == 1) {
            lines.push_back({"F: live on as your last descendant", kGood, false, true});
        } else if (narrow) {
            lines.push_back({"Your lineage is gone.", kWarn, false, true});
            lines.push_back({"F: another cell  R: new chamber", kWarn, false});
        } else {
            lines.push_back({"Your lineage is gone. F: embody another cell, R: new chamber", kWarn, false, true});
        }
        return lines;
    };
    const auto widestOf = [](const std::vector<Line>& lines) {
        size_t w = 0;
        for (const auto& l : lines) w = std::max(w, l.text.size());
        return w;
    };
    const float room = W - 2 * m - 4 * pad;
    std::vector<Line> lines = layout(false);
    if (static_cast<float>(std::max(widestOf(lines), upper.size() * 2)) * s > room) lines = layout(true);
    // Glyph size: the HUD's own, or the largest multiple of 8 that fits.
    float fs = s;
    while (fs > 8.0f && static_cast<float>(widestOf(lines)) * fs > room) fs -= 8.0f;
    float big = fs * 2.0f;
    while (big > fs && static_cast<float>(upper.size()) * big > room) big -= 8.0f;
    const float flh = std::round(fs * 1.25f);

    const float wHead = static_cast<float>(upper.size()) * big;
    const float wBody = static_cast<float>(widestOf(lines)) * fs;
    const float boxW = std::min(W - 2 * m, std::max(wHead, wBody) + 4 * pad);
    const float boxH = big + static_cast<float>(lines.size()) * flh + 5 * pad;
    const float bx = (W - boxW) * 0.5f, by = H * 0.5f - boxH * 0.5f;
    fillRect(bx, by, bx + boxW, by + boxH, kPanel);
    fillRect(bx, by, bx + boxW, by + boxH, kPanel);  // twice: nearly opaque over the stats behind it
    float y = by + 2 * pad;
    text((W - wHead) * 0.5f, y, upper, kDanger, big);
    y += big + pad;
    // Block lines share one left edge, so continuation rows line up.
    size_t blockW = 0;
    for (const auto& l : lines)
        if (l.block) blockW = std::max(blockW, l.text.size());
    const float blockX = (W - static_cast<float>(blockW) * fs) * 0.5f;
    for (const Line& l : lines) {
        if (l.gap) y += pad;
        const float x = l.block ? blockX : (W - static_cast<float>(l.text.size()) * fs) * 0.5f;
        text(x, y, l.text, l.color, fs);
        y += flh;
    }
}

void Hud::mutationOffer(const MicroEnvironment& env, float W, float H) {
    // ---- centre: the cell divided; one of the offered mutations is kept.
    const Player& pl = env.player();
    const float s = s_;
    const float lh = std::round(s * 1.25f);
    const float pad = std::round(s * 0.5f);
    const float big = s * 2.0f;
    const std::string head = "YOUR CELL DIVIDED";
    const std::string sub = "keep one mutation for your lineage: press 1-" + std::to_string(pl.offerCount);
    std::vector<std::string> rows;
    for (int i = 0; i < pl.offerCount; ++i) {
        const Mutation m = pl.offer[static_cast<size_t>(i)];
        const int lv = pl.level(m);
        rows.push_back(std::to_string(i + 1) + "  " + mutationName(m) + (lv > 0 ? " " + std::to_string(lv + 1) : "") +
                       ": " + mutationEffect(m));
    }
    size_t widest = sub.size();
    for (const auto& r : rows) widest = std::max(widest, r.size());
    const float wHead = static_cast<float>(head.size()) * big;
    const float boxW = std::min(W - 2 * s, std::max(wHead, static_cast<float>(widest) * s) + 4 * pad);
    const float boxH = big + lh * static_cast<float>(1 + rows.size()) + 5 * pad;
    // In the lower third: the camera keeps the player in the middle.
    const float bx = (W - boxW) * 0.5f, by = std::min(H * 0.72f - boxH * 0.5f, H - boxH - s);
    fillRect(bx, by, bx + boxW, by + boxH, kPanel);
    fillRect(bx, by, bx + boxW, by + boxH, kPanel);
    float y = by + 2 * pad;
    text((W - wHead) * 0.5f, y, head, kMutation, big);
    y += big + pad;
    text((W - static_cast<float>(sub.size()) * s) * 0.5f, y, sub, kDim);
    y += lh + pad;
    for (const auto& r : rows) {
        text(bx + 2 * pad, y, r, kWhite);
        y += lh;
    }
}

void Hud::submit(strata::Engine& engine) const {
    if (!lines_.empty()) engine.submitLines(lines_.data(), lines_.size());
    if (!points_.empty()) engine.submitPoints(points_.data(), points_.size());
    if (!texts_.empty()) engine.submitText(texts_.data(), texts_.size());
}

}  // namespace mbstrata
