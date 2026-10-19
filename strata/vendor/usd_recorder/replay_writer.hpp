// usd_recorder/replay_writer.hpp: a header-only C++17 writer for usd-replay/1 replays (.usda).
//
// The replay contract (docs/REPLAY.md) for simulations written in C++, without linking OpenUSD.
// Standard library only. Samples are formatted into memory as they arrive and written as one
// text .usda on write(), which replaces the file atomically (temporary file + std::rename).
// `usd-recorder replay convert run.usda run.usdc` makes the compact binary file to publish.
//
//   usd_recorder::ReplayMeta meta;
//   meta.sim = "mysim"; meta.sim_version = "1.2.0"; meta.seed = "7"; meta.tick_rate = 10;
//   meta.state_hash_algorithm = "mysim fingerprint (FNV-1a 64)";
//   usd_recorder::ReplayWriter w(meta);
//   w.entity_set("prey", "Prey", usd_recorder::Rgb8{90, 158, 75}, 1.0f);   // constant colour/width
//   w.field("food", "Food", 32, 32, 0.0, 0.0, 4.0);
//   for (int tick = 0; tick < 3000; tick += 10) {
//       sim.run_until(tick);
//       w.begin_sample(tick, hex16(sim.hash()));
//       w.entities("prey", ids.data(), xyz.data(), ids.size());          // xyz: 3 floats a point
//       w.field_colors("food", rgb.data());                              // cols*rows Rgb8, sRGB
//       w.stat("population", double(ids.size()));
//       w.end_sample();
//   }
//   w.write("run.usda");
//
// Colours are given as sRGB bytes and written linear, like the Python ReplayWriter; a field
// whose colours equal its previous sample is not resampled (players hold it).
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel McCoy Stephenson
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace usd_recorder {

struct Rgb8 {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb8& o) const { return r == o.r && g == o.g && b == o.b; }
};

struct ReplayMeta {
    std::string sim, sim_version, seed = "0", state_hash_algorithm;
    double tick_rate = 1.0;       // sim ticks per simulated second
    double playback_rate = 30.0;  // suggested ticks per second of playback (timeCodesPerSecond)
    double meters_per_unit = 1.0;
    std::string title, description, source;  // optional
    std::map<std::string, std::string> string_params;  // usdReplay.params
    std::map<std::string, double> number_params;
};

namespace replay_detail {

inline std::string fmt_double(double d) {
    if (!std::isfinite(d)) throw std::invalid_argument("non-finite value");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
}

inline void put_float(std::string& o, float f) {
    if (!std::isfinite(f)) throw std::invalid_argument("non-finite value");
    char buf[24];
    std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(f));
    o += buf;
}

inline std::string quote(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else o += c;
    }
    return o + "\"";
}

inline bool valid_name(const std::string& n) {
    if (n.empty() || !(std::isalpha(static_cast<unsigned char>(n[0])) || n[0] == '_')) return false;
    for (char c : n)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

// sRGB byte -> linear float, computed exactly like the Python writer (double, then float).
inline const std::array<float, 256>& linear_table() {
    static const std::array<float, 256> t = [] {
        std::array<float, 256> a{};
        for (int i = 0; i < 256; ++i) {
            const double c = i / 255.0;
            a[i] = static_cast<float>(c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4));
        }
        return a;
    }();
    return t;
}

inline void put_rgb(std::string& o, const Rgb8& c) {
    const auto& t = linear_table();
    o += '(';
    put_float(o, t[c.r]);
    o += ", ";
    put_float(o, t[c.g]);
    o += ", ";
    put_float(o, t[c.b]);
    o += ')';
}

struct Set {
    std::string name, label, shape;
    std::optional<Rgb8> color;
    std::optional<float> width;
    std::string ids, points, colors, widths;  // "tick: [...],\n" lines
    bool given = false;
};

struct Field {
    std::string name, label;
    int cols = 0, rows = 0;
    double ox = 0, oy = 0, cell = 1, z = 0;
    std::vector<Rgb8> last;
    std::string samples;
    bool given = false;
};

struct ScenePts {
    std::string name, label, shape;
    std::vector<std::array<float, 3>> points;
    std::vector<float> widths;
    std::vector<Rgb8> colors;
};

}  // namespace replay_detail

class ReplayWriter {
public:
    explicit ReplayWriter(ReplayMeta meta) : meta_(std::move(meta)) {
        if (meta_.sim.empty()) throw std::invalid_argument("sim must not be empty");
        if (meta_.seed.empty() || meta_.seed.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("seed must be decimal digits");
        if (!(meta_.tick_rate > 0) || !(meta_.playback_rate > 0))
            throw std::invalid_argument("tick_rate and playback_rate must be positive");
    }

    // Constant colour and width when given; std::nullopt = per-point values in every sample.
    void entity_set(const std::string& name, const std::string& label, std::optional<Rgb8> color,
                    std::optional<float> width = 1.0f, const std::string& shape = "sphere") {
        declare(name);
        if (shape != "sphere" && shape != "cube") throw std::invalid_argument("shape must be sphere or cube");
        replay_detail::Set s;
        s.name = name;
        s.label = label.empty() ? name : label;
        s.shape = shape;
        s.color = color;
        s.width = width;
        set_index_[name] = sets_.size();
        sets_.push_back(std::move(s));
    }

    void field(const std::string& name, const std::string& label, int cols, int rows, double origin_x, double origin_y,
               double cell_size, double z = 0.0) {
        declare(name);
        if (cols < 1 || rows < 1) throw std::invalid_argument("a field needs at least one column and one row");
        if (!(cell_size > 0)) throw std::invalid_argument("cell_size must be positive");
        replay_detail::Field f;
        f.name = name;
        f.label = label.empty() ? name : label;
        f.cols = cols;
        f.rows = rows;
        f.ox = origin_x;
        f.oy = origin_y;
        f.cell = cell_size;
        f.z = z;
        field_index_[name] = fields_.size();
        fields_.push_back(std::move(f));
    }

    // Static points. `widths` and `colors` hold one value (constant) or one per point.
    void scene_points(const std::string& name, const std::string& label, std::vector<std::array<float, 3>> points,
                      std::vector<float> widths, std::vector<Rgb8> colors, const std::string& shape = "sphere") {
        declare(name);
        if (shape != "sphere" && shape != "cube") throw std::invalid_argument("shape must be sphere or cube");
        if (widths.empty() || colors.empty()) throw std::invalid_argument("scene points need widths and colours");
        if ((widths.size() != 1 && widths.size() != points.size()) || (colors.size() != 1 && colors.size() != points.size()))
            throw std::invalid_argument("widths/colours must be one value or one per point");
        scene_.push_back({name, label.empty() ? name : label, shape, std::move(points), std::move(widths), std::move(colors)});
    }

    void begin_sample(int64_t tick, const std::string& state_hash) {
        if (open_) throw std::logic_error("begin_sample inside a sample");
        if (!ticks_.empty() && tick <= ticks_.back()) throw std::invalid_argument("ticks must strictly increase");
        if (state_hash.size() < 8 || state_hash.size() > 64 ||
            state_hash.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("state_hash must be 8-64 lowercase hex digits");
        open_ = true;
        tick_ = tick;
        hash_ = state_hash;
        sample_stats_.clear();
        for (auto& s : sets_) s.given = false;
        for (auto& f : fields_) f.given = false;
    }

    // `xyz` holds 3 floats per point. `colors` (sRGB) / `widths` only for per-point sets.
    void entities(const std::string& name, const int64_t* ids, const float* xyz, size_t n, const Rgb8* colors = nullptr,
                  const float* widths = nullptr) {
        if (!open_) throw std::logic_error("entities outside a sample");
        auto it = set_index_.find(name);
        if (it == set_index_.end()) throw std::invalid_argument("undeclared set: " + name);
        auto& s = sets_[it->second];
        if (s.given) throw std::invalid_argument("set given twice in one sample: " + name);
        // An empty sample may pass null pointers (std::vector::data() of an empty vector can be null).
        if (n && s.color.has_value() == (colors != nullptr)) throw std::invalid_argument(name + ": colours per point iff no constant colour");
        if (n && s.width.has_value() == (widths != nullptr)) throw std::invalid_argument(name + ": widths per point iff no constant width");
        std::set<int64_t> seen;
        for (size_t i = 0; i < n; ++i)
            if (!seen.insert(ids[i]).second) throw std::invalid_argument(name + ": ids must be unique within a sample");
        const std::string head = "                " + std::to_string(tick_) + ": [";
        s.ids += head;
        s.points += head;
        for (size_t i = 0; i < n; ++i) {
            if (i) { s.ids += ", "; s.points += ", "; }
            s.ids += std::to_string(ids[i]);
            s.points += '(';
            replay_detail::put_float(s.points, xyz[3 * i]);
            s.points += ", ";
            replay_detail::put_float(s.points, xyz[3 * i + 1]);
            s.points += ", ";
            replay_detail::put_float(s.points, xyz[3 * i + 2]);
            s.points += ')';
        }
        s.ids += "],\n";
        s.points += "],\n";
        if (!s.color) {
            s.colors += head;
            for (size_t i = 0; i < n; ++i) {
                if (i) s.colors += ", ";
                replay_detail::put_rgb(s.colors, colors[i]);
            }
            s.colors += "],\n";
        }
        if (!s.width) {
            s.widths += head;
            for (size_t i = 0; i < n; ++i) {
                if (i) s.widths += ", ";
                replay_detail::put_float(s.widths, widths[i]);
            }
            s.widths += "],\n";
        }
        s.given = true;
    }

    // cols*rows sRGB colours, row-major from the minimum corner. Required in the first sample.
    void field_colors(const std::string& name, const Rgb8* colors) {
        if (!open_) throw std::logic_error("field_colors outside a sample");
        auto it = field_index_.find(name);
        if (it == field_index_.end()) throw std::invalid_argument("undeclared field: " + name);
        auto& f = fields_[it->second];
        if (f.given) throw std::invalid_argument("field given twice in one sample: " + name);
        f.given = true;
        const size_t n = static_cast<size_t>(f.cols) * static_cast<size_t>(f.rows);
        if (!f.last.empty() && std::equal(colors, colors + n, f.last.begin())) return;
        f.last.assign(colors, colors + n);
        f.samples += "                " + std::to_string(tick_) + ": [";
        for (size_t i = 0; i < n; ++i) {
            if (i) f.samples += ", ";
            replay_detail::put_rgb(f.samples, colors[i]);
        }
        f.samples += "],\n";
    }

    void stat(const std::string& name, double value) {
        if (!open_) throw std::logic_error("stat outside a sample");
        if (!replay_detail::valid_name(name)) throw std::invalid_argument("bad stat name: " + name);
        if (!sample_stats_.insert(name).second) throw std::invalid_argument("stat given twice: " + name);
        stats_[name] += "        " + std::to_string(tick_) + ": " + replay_detail::fmt_double(value) + ",\n";
    }

    void end_sample() {
        if (!open_) throw std::logic_error("end_sample without begin_sample");
        for (auto& s : sets_)
            if (!s.given) throw std::invalid_argument("every set needs a sample; missing " + s.name);
        if (ticks_.empty())
            for (auto& f : fields_)
                if (!f.given) throw std::invalid_argument("the first sample must give every field; missing " + f.name);
        hashes_ += "        " + std::to_string(tick_) + ": " + replay_detail::quote(hash_) + ",\n";
        ticks_.push_back(tick_);
        open_ = false;
    }

    size_t samples() const { return ticks_.size(); }

    std::string usda() const {
        using replay_detail::fmt_double;
        using replay_detail::quote;
        if (ticks_.empty()) throw std::logic_error("a replay needs at least one sample");
        std::string o;
        o += "#usda 1.0\n(\n    customLayerData = {\n        dictionary usdReplay = {\n";
        if (!meta_.description.empty()) o += "            string description = " + quote(meta_.description) + "\n";
        if (!meta_.string_params.empty() || !meta_.number_params.empty()) {
            o += "            dictionary params = {\n";
            std::map<std::string, std::string> lines;
            for (auto& [k, v] : meta_.number_params) lines[k] = "double " + k + " = " + fmt_double(v);
            for (auto& [k, v] : meta_.string_params) lines[k] = "string " + k + " = " + quote(v);
            for (auto& [k, line] : lines) {
                if (!replay_detail::valid_name(k)) throw std::invalid_argument("bad param name: " + k);
                o += "                " + line + "\n";
            }
            o += "            }\n";
        }
        o += "            string schema = \"usd-replay/1\"\n";
        o += "            string seed = " + quote(meta_.seed) + "\n";
        o += "            string sim = " + quote(meta_.sim) + "\n";
        o += "            string simVersion = " + quote(meta_.sim_version) + "\n";
        if (!meta_.source.empty()) o += "            string source = " + quote(meta_.source) + "\n";
        o += "            string stateHashAlgorithm = " + quote(meta_.state_hash_algorithm) + "\n";
        o += "            double tickRate = " + fmt_double(meta_.tick_rate) + "\n";
        if (!meta_.title.empty()) o += "            string title = " + quote(meta_.title) + "\n";
        o += "        }\n    }\n    defaultPrim = \"Replay\"\n";
        o += "    endTimeCode = " + std::to_string(ticks_.back()) + "\n";
        o += "    framesPerSecond = " + fmt_double(meta_.playback_rate) + "\n";
        o += "    metersPerUnit = " + fmt_double(meta_.meters_per_unit) + "\n";
        o += "    startTimeCode = " + std::to_string(ticks_.front()) + "\n";
        o += "    timeCodesPerSecond = " + fmt_double(meta_.playback_rate) + "\n";
        o += "    upAxis = \"Z\"\n)\n\ndef Xform \"Replay\"\n{\n";
        for (auto& [k, samples] : stats_)
            o += "    custom double replay:stat:" + k + "\n    double replay:stat:" + k + ".timeSamples = {\n" + samples + "    }\n";
        o += "    custom string replay:stateHash\n    string replay:stateHash.timeSamples = {\n" + hashes_ + "    }\n";

        o += "\n    def Scope \"Entities\"\n    {\n";
        for (size_t i = 0; i < sets_.size(); ++i) {
            const auto& s = sets_[i];
            if (i) o += "\n";
            o += "        def Points " + quote(s.name) + "\n        {\n";
            o += "            int64[] ids.timeSamples = {\n" + s.ids + "            }\n";
            o += "            point3f[] points.timeSamples = {\n" + s.points + "            }\n";
            if (s.color) {
                o += "            color3f[] primvars:displayColor = [";
                replay_detail::put_rgb(o, *s.color);
                o += "] (\n                interpolation = \"constant\"\n            )\n";
            } else {
                o += "            color3f[] primvars:displayColor (\n                interpolation = \"vertex\"\n            )\n";
                o += "            color3f[] primvars:displayColor.timeSamples = {\n" + s.colors + "            }\n";
            }
            o += "            custom string replay:label = " + quote(s.label) + "\n";
            o += "            custom token replay:shape = " + quote(s.shape) + "\n";
            if (s.width) {
                o += "            float[] widths = [";
                replay_detail::put_float(o, *s.width);
                o += "] (\n                interpolation = \"constant\"\n            )\n";
            } else {
                o += "            float[] widths (\n                interpolation = \"vertex\"\n            )\n";
                o += "            float[] widths.timeSamples = {\n" + s.widths + "            }\n";
            }
            o += "        }\n";
        }
        o += "    }\n";

        if (!fields_.empty()) {
            o += "\n    def Scope \"Fields\"\n    {\n";
            for (size_t i = 0; i < fields_.size(); ++i) {
                const auto& f = fields_[i];
                if (i) o += "\n";
                const int c1 = f.cols + 1;
                o += "        def Mesh " + quote(f.name) + "\n        {\n";
                o += "            uniform bool doubleSided = 1\n";
                o += "            float3[] extent = [(";
                replay_detail::put_float(o, static_cast<float>(f.ox));
                o += ", ";
                replay_detail::put_float(o, static_cast<float>(f.oy));
                o += ", ";
                replay_detail::put_float(o, static_cast<float>(f.z));
                o += "), (";
                replay_detail::put_float(o, static_cast<float>(f.ox + f.cols * f.cell));
                o += ", ";
                replay_detail::put_float(o, static_cast<float>(f.oy + f.rows * f.cell));
                o += ", ";
                replay_detail::put_float(o, static_cast<float>(f.z));
                o += ")]\n";
                o += "            int[] faceVertexCounts = [";
                for (int k = 0; k < f.cols * f.rows; ++k) o += k ? ", 4" : "4";
                o += "]\n            int[] faceVertexIndices = [";
                bool first = true;
                for (int j = 0; j < f.rows; ++j)
                    for (int ii = 0; ii < f.cols; ++ii) {
                        const int a = j * c1 + ii;
                        if (!first) o += ", ";
                        first = false;
                        o += std::to_string(a) + ", " + std::to_string(a + 1) + ", " + std::to_string(a + c1 + 1) + ", " +
                             std::to_string(a + c1);
                    }
                o += "]\n            point3f[] points = [";
                first = true;
                for (int j = 0; j <= f.rows; ++j)
                    for (int ii = 0; ii <= f.cols; ++ii) {
                        if (!first) o += ", ";
                        first = false;
                        o += '(';
                        replay_detail::put_float(o, static_cast<float>(f.ox + ii * f.cell));
                        o += ", ";
                        replay_detail::put_float(o, static_cast<float>(f.oy + j * f.cell));
                        o += ", ";
                        replay_detail::put_float(o, static_cast<float>(f.z));
                        o += ')';
                    }
                o += "]\n";
                o += "            color3f[] primvars:displayColor (\n                interpolation = \"uniform\"\n            )\n";
                o += "            color3f[] primvars:displayColor.timeSamples = {\n" + f.samples + "            }\n";
                o += "            custom double replay:field:cellSize = " + fmt_double(f.cell) + "\n";
                o += "            custom double2 replay:field:origin = (" + fmt_double(f.ox) + ", " + fmt_double(f.oy) + ")\n";
                o += "            custom int2 replay:field:size = (" + std::to_string(f.cols) + ", " + std::to_string(f.rows) + ")\n";
                o += "            custom string replay:label = " + quote(f.label) + "\n";
                o += "            uniform token subdivisionScheme = \"none\"\n";
                o += "        }\n";
            }
            o += "    }\n";
        }

        if (!scene_.empty()) {
            o += "\n    def Scope \"Scene\"\n    {\n";
            for (size_t i = 0; i < scene_.size(); ++i) {
                const auto& s = scene_[i];
                if (i) o += "\n";
                o += "        def Points " + quote(s.name) + "\n        {\n";
                o += "            point3f[] points = [";
                for (size_t k = 0; k < s.points.size(); ++k) {
                    if (k) o += ", ";
                    o += '(';
                    replay_detail::put_float(o, s.points[k][0]);
                    o += ", ";
                    replay_detail::put_float(o, s.points[k][1]);
                    o += ", ";
                    replay_detail::put_float(o, s.points[k][2]);
                    o += ')';
                }
                o += "]\n            color3f[] primvars:displayColor = [";
                for (size_t k = 0; k < s.colors.size(); ++k) {
                    if (k) o += ", ";
                    replay_detail::put_rgb(o, s.colors[k]);
                }
                o += std::string("] (\n                interpolation = \"") + (s.colors.size() == 1 ? "constant" : "vertex") +
                     "\"\n            )\n";
                o += "            custom string replay:label = " + quote(s.label) + "\n";
                o += "            custom token replay:shape = " + quote(s.shape) + "\n";
                o += "            float[] widths = [";
                for (size_t k = 0; k < s.widths.size(); ++k) {
                    if (k) o += ", ";
                    replay_detail::put_float(o, s.widths[k]);
                }
                o += std::string("] (\n                interpolation = \"") + (s.widths.size() == 1 ? "constant" : "vertex") +
                     "\"\n            )\n";
                o += "        }\n";
            }
            o += "    }\n";
        }
        o += "}\n";
        return o;
    }

    // Write the replay to `path` (.usda) atomically: a temporary file next to it, then rename.
    void write(const std::string& path) const {
        if (open_) throw std::logic_error("write inside a sample");
        const std::string text = usda();
        const std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) throw std::runtime_error("cannot write " + tmp);
            f.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (!f) throw std::runtime_error("cannot write " + tmp);
        }
        std::remove(path.c_str());  // std::rename does not replace on Windows
        if (std::rename(tmp.c_str(), path.c_str()) != 0) {
            std::remove(tmp.c_str());
            throw std::runtime_error("cannot rename " + tmp + " to " + path);
        }
    }

private:
    void declare(const std::string& name) {
        if (!ticks_.empty() || open_) throw std::logic_error("declare sets, fields and scene points before the first sample");
        if (!replay_detail::valid_name(name)) throw std::invalid_argument("bad prim name: " + name);
        if (!names_.insert(name).second) throw std::invalid_argument("already declared: " + name);
    }

    ReplayMeta meta_;
    std::vector<replay_detail::Set> sets_;
    std::map<std::string, size_t> set_index_;
    std::vector<replay_detail::Field> fields_;
    std::map<std::string, size_t> field_index_;
    std::vector<replay_detail::ScenePts> scene_;
    std::set<std::string> names_;
    std::map<std::string, std::string> stats_;
    std::set<std::string> sample_stats_;
    std::vector<int64_t> ticks_;
    std::string hashes_;
    std::string hash_;
    int64_t tick_ = 0;
    bool open_ = false;
};

}  // namespace usd_recorder
