// usd_recorder/usda_writer.hpp: a header-only C++17 writer for time-sampled .usda recordings.
//
// For simulations written in C++ that want the same stage layout as the Python
// usd_recorder.Recorder (unchunked) without linking OpenUSD. No dependencies beyond the
// standard library. Samples are kept in memory and written as text .usda on write(), which
// replaces the file atomically (temporary file + std::rename in the same directory).
//
//   usd_recorder::UsdaRecorder rec(10.0);          // time codes per second
//   for (int tick = 0; tick < 100; ++tick) {
//       rec.begin_tick(tick);
//       rec.entity(7).position(x, y, 0).set("energy", 3.5).set("state", "hunting");
//       rec.extra("clock", tick * 0.1);
//       rec.event("log", "spawned");
//       rec.end_tick();
//   }
//   rec.write("run.usda");
//
// Semantics match the Python Recorder: one tick is one time code; an entity present this tick
// is visible, an absent one invisible (also before it first appears); values are sampled
// sparsely with hold keys and hold while not given; event channels read "" on ticks without an
// event. A key keeps the type of its first value (a different type throws). See
// ARCHITECTURE.md § C++ writer.
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel McCoy Stephenson
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace usd_recorder {

using Vec3 = std::array<double, 3>;
using Value = std::variant<bool, int32_t, double, std::string, Vec3>;

namespace detail {

inline const char* type_name(const Value& v) {
    switch (v.index()) {
        case 0: return "bool";
        case 1: return "int";
        case 2: return "double";
        case 3: return "string";
        default: return "double3";
    }
}

inline std::string fmt_double(double d) {
    if (!std::isfinite(d)) throw std::invalid_argument("usd_recorder: values must be finite");
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    return buf;
}

inline std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out + "\"";
}

inline std::string fmt(const Value& v) {
    switch (v.index()) {
        case 0: return std::get<bool>(v) ? "1" : "0";
        case 1: return std::to_string(std::get<int32_t>(v));
        case 2: return fmt_double(std::get<double>(v));
        case 3: return quote(std::get<std::string>(v));
        default: {
            const Vec3& a = std::get<Vec3>(v);
            return "(" + fmt_double(a[0]) + ", " + fmt_double(a[1]) + ", " + fmt_double(a[2]) + ")";
        }
    }
}

inline bool valid_name(const std::string& s) {
    // [A-Za-z_][A-Za-z0-9_]* segments joined by ':'
    if (s.empty()) return false;
    bool start = true;
    for (char c : s) {
        bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
        bool digit = c >= '0' && c <= '9';
        if (c == ':') {
            if (start) return false;
            start = true;
        } else if (start) {
            if (!alpha) return false;
            start = false;
        } else if (!alpha && !digit) {
            return false;
        }
    }
    return !start;
}

// Sparse sampling with hold keys (the same algorithm as usd_recorder.Track).
struct Track {
    std::vector<std::pair<double, Value>> samples;
    std::optional<Value> value;
    std::optional<double> pending;

    void observe(double t, const Value& v) {
        if (!value) {
            samples.emplace_back(t, v);
            value = v;
            pending.reset();
        } else if (*value == v) {
            pending = t;
        } else {
            if (pending) samples.emplace_back(*pending, *value);
            samples.emplace_back(t, v);
            value = v;
            pending.reset();
        }
    }
    void hold(double t) {
        if (value) pending = t;
    }
    void flush() {
        if (pending) {
            samples.emplace_back(*pending, *value);
            pending.reset();
        }
    }
};

}  // namespace detail

class UsdaRecorder;

class EntityRef {
  public:
    EntityRef& position(double x, double y, double z) {
        values_->insert_or_assign("position", Value(Vec3{x, y, z}));
        return *this;
    }
    EntityRef& set(const std::string& key, bool v) { return put(key, Value(v)); }
    EntityRef& set(const std::string& key, int32_t v) { return put(key, Value(v)); }
    EntityRef& set(const std::string& key, double v) { return put(key, Value(v)); }
    EntityRef& set(const std::string& key, const char* v) { return put(key, Value(std::string(v))); }
    EntityRef& set(const std::string& key, const std::string& v) { return put(key, Value(v)); }
    EntityRef& set(const std::string& key, const Vec3& v) { return put(key, Value(v)); }

  private:
    friend class UsdaRecorder;
    explicit EntityRef(std::map<std::string, Value>* values) : values_(values) {}
    EntityRef& put(const std::string& key, Value v) {
        if (key == "position" || key == "color" || !detail::valid_name(key))
            throw std::invalid_argument("usd_recorder: invalid key '" + key + "'");
        values_->insert_or_assign(key, std::move(v));
        return *this;
    }
    std::map<std::string, Value>* values_;
};

class UsdaRecorder {
  public:
    explicit UsdaRecorder(double time_codes_per_second = 24.0, char up_axis = 'Z')
        : tcps_(time_codes_per_second), up_axis_(up_axis) {
        if (!(tcps_ > 0) || !std::isfinite(tcps_))
            throw std::invalid_argument("usd_recorder: time_codes_per_second must be positive");
        if (up_axis_ != 'Y' && up_axis_ != 'Z') throw std::invalid_argument("usd_recorder: up_axis must be Y or Z");
    }

    void begin_tick(double tick) {
        if (in_tick_) throw std::logic_error("usd_recorder: begin_tick without end_tick");
        if (!std::isfinite(tick) || (last_ && tick <= *last_))
            throw std::invalid_argument("usd_recorder: ticks must be finite and increasing");
        in_tick_ = true;
        tick_ = tick;
        present_.clear();
        extras_now_.clear();
        events_now_.clear();
    }

    // Values for entity `id` at this tick; calling it marks the entity present.
    EntityRef entity(int64_t id) {
        require_tick();
        return EntityRef(&present_[id]);
    }

    template <typename T>
    void extra(const std::string& key, T v) {
        require_tick();
        if (!detail::valid_name(key)) throw std::invalid_argument("usd_recorder: invalid key '" + key + "'");
        extras_now_.insert_or_assign(key, to_value(v));
    }

    void event(const std::string& channel, const std::string& what) {
        require_tick();
        if (!detail::valid_name(channel)) throw std::invalid_argument("usd_recorder: invalid channel '" + channel + "'");
        auto it = events_now_.find(channel);
        if (it == events_now_.end()) events_now_[channel] = what;
        else it->second += "\n" + what;
    }

    void end_tick() {
        require_tick();
        // Validate types before writing anything, so a throwing tick changes nothing.
        for (auto& [id, values] : present_)
            for (auto& [key, v] : values) check_type(entity_types_[id], key, v);
        for (auto& [key, v] : extras_now_) check_type(extra_types_, key, v);

        std::optional<double> previous = last_;
        if (!first_) first_ = tick_;
        last_ = tick_;
        in_tick_ = false;
        for (auto& [id, values] : present_) {
            auto [it, created] = entities_.try_emplace(id);
            Entity& e = it->second;
            if (created && previous) e.visibility.observe(*previous, Value(std::string("invisible")));
            e.visibility.observe(tick_, Value(std::string("inherited")));
            for (auto& [key, v] : values) {
                e.attrs[key].observe(tick_, v);
                entity_types_[id][key] = detail::type_name(v);
            }
            for (auto& [key, track] : e.attrs)
                if (!values.count(key)) track.hold(tick_);
        }
        for (auto& [id, e] : entities_) {
            if (present_.count(id)) continue;
            e.visibility.observe(tick_, Value(std::string("invisible")));
            for (auto& [key, track] : e.attrs) track.hold(tick_);
        }
        for (auto& [key, v] : extras_now_) {
            extras_[key].observe(tick_, v);
            extra_types_[key] = detail::type_name(v);
        }
        for (auto& [key, track] : extras_)
            if (!extras_now_.count(key)) track.hold(tick_);
        for (auto& [channel, what] : events_now_) {
            if (!events_.count(channel) && previous) events_[channel].observe(*previous, Value(std::string()));
            channels_.insert(channel);
        }
        for (const auto& channel : channels_) {
            auto it = events_now_.find(channel);
            events_[channel].observe(tick_, Value(it == events_now_.end() ? std::string() : it->second));
        }
    }

    // Serialise everything recorded so far as .usda text.
    std::string to_usda() {
        for (auto& [id, e] : entities_) {
            e.visibility.flush();
            for (auto& [k, t] : e.attrs) t.flush();
        }
        for (auto& [k, t] : extras_) t.flush();
        for (auto& [k, t] : events_) t.flush();
        std::ostringstream o;
        o << "#usda 1.0\n(\n    customLayerData = {\n        string \"usdRecorder:writer\" = \"cpp\"\n    }\n"
          << "    defaultPrim = \"World\"\n";
        if (first_) o << "    endTimeCode = " << detail::fmt_double(*last_) << "\n";
        o << "    framesPerSecond = " << detail::fmt_double(tcps_) << "\n    metersPerUnit = 1\n";
        if (first_) o << "    startTimeCode = " << detail::fmt_double(*first_) << "\n";
        o << "    timeCodesPerSecond = " << detail::fmt_double(tcps_) << "\n    upAxis = \"" << up_axis_
          << "\"\n)\n\ndef Xform \"World\"\n{\n";
        for (auto& [key, t] : events_) attribute(o, "    ", "string", "events:" + key, t);
        for (auto& [key, t] : extras_) attribute(o, "    ", usd_type(extra_types_[key]), "extras:" + key, t);
        o << "\n    def Scope \"Entities\"\n    {\n";
        for (auto& [id, e] : entities_) {
            o << "        def Xform \"" << (id < 0 ? "e_n" + std::to_string(-id) : "e_" + std::to_string(id))
              << "\"\n        {\n";
            for (auto& [key, t] : e.attrs)
                if (key != "position") attribute(o, "            ", usd_type(entity_types_[id][key]), "extras:" + key, t);
            o << "            custom int64 recorder:id = " << id << "\n";
            sampled(o, "            ", "token visibility", e.visibility);
            auto pos = e.attrs.find("position");
            if (pos != e.attrs.end()) {
                sampled(o, "            ", "double3 xformOp:translate", pos->second);
                o << "            uniform token[] xformOpOrder = [\"xformOp:translate\"]\n";
            }
            o << "\n            def Cube \"geom\"\n            {\n"
              << "                float3[] extent = [(-0.5, -0.5, -0.5), (0.5, 0.5, 0.5)]\n"
              << "                color3f[] primvars:displayColor = [(0.6, 0.6, 0.6)]\n"
              << "                double size = 1\n            }\n        }\n";
        }
        o << "    }\n}\n";
        return o.str();
    }

    // Write the recording to `path` atomically (temporary file next to it, then rename).
    void write(const std::string& path) {
        std::string text = to_usda();
        std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) throw std::runtime_error("usd_recorder: cannot write " + tmp);
            f << text;
            if (!f.flush()) throw std::runtime_error("usd_recorder: cannot write " + tmp);
        }
        if (std::rename(tmp.c_str(), path.c_str()) != 0) {
            std::remove(tmp.c_str());
            throw std::runtime_error("usd_recorder: cannot replace " + path);
        }
    }

  private:
    struct Entity {
        detail::Track visibility;
        std::map<std::string, detail::Track> attrs;
    };

    static Value to_value(bool v) { return Value(v); }
    static Value to_value(int32_t v) { return Value(v); }
    static Value to_value(double v) { return Value(v); }
    static Value to_value(const char* v) { return Value(std::string(v)); }
    static Value to_value(const std::string& v) { return Value(v); }
    static Value to_value(const Vec3& v) { return Value(v); }

    static std::string usd_type(const std::string& t) { return t; }

    void require_tick() const {
        if (!in_tick_) throw std::logic_error("usd_recorder: call begin_tick first");
    }

    static void check_type(std::map<std::string, std::string>& types, const std::string& key, const Value& v) {
        auto it = types.find(key);
        if (it != types.end() && it->second != detail::type_name(v))
            throw std::invalid_argument("usd_recorder: key '" + key + "' changed type from " + it->second + " to " +
                                        detail::type_name(v));
    }

    static void sampled(std::ostringstream& o, const std::string& indent, const std::string& decl,
                        const detail::Track& t) {
        o << indent << decl << ".timeSamples = {\n";
        for (auto& [time, v] : t.samples) o << indent << "    " << detail::fmt_double(time) << ": " << detail::fmt(v) << ",\n";
        o << indent << "}\n";
    }

    static void attribute(std::ostringstream& o, const std::string& indent, const std::string& type,
                          const std::string& name, const detail::Track& t) {
        o << indent << "custom " << type << " " << name << "\n";
        sampled(o, indent, type + " " + name, t);
    }

    double tcps_;
    char up_axis_;
    bool in_tick_ = false;
    double tick_ = 0;
    std::optional<double> first_, last_;
    std::map<int64_t, std::map<std::string, Value>> present_;
    std::map<std::string, Value> extras_now_;
    std::map<std::string, std::string> events_now_;
    std::map<int64_t, Entity> entities_;
    std::map<int64_t, std::map<std::string, std::string>> entity_types_;
    std::map<std::string, std::string> extra_types_;
    std::map<std::string, detail::Track> extras_;
    std::map<std::string, detail::Track> events_;
    std::set<std::string> channels_;
};

}  // namespace usd_recorder
