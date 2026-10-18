#include "scene.h"

#include <algorithm>
#include <cmath>

#include "strata/sim/colormap.hpp"

namespace mbstrata {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kGrainHeight = 2.4f;
const float kGrainRgb[3] = {0.30f, 0.25f, 0.19f};

// Saturating map of a concentration onto [0, 1): v / (v + half).
float saturate(float v, float half) { return v <= 0.0f ? 0.0f : v / (v + half); }

}  // namespace

const char* fieldViewName(FieldView view) {
    switch (view) {
        case FieldView::Nutrient: return "nutrient";
        case FieldView::Waste: return "waste";
        case FieldView::Oxygen: return "oxygen";
        case FieldView::Composite:
        default: return "composite";
    }
}

StrataCamera OrbitCamera::toStrata(float aspect) const {
    const float yaw = yawDegrees * kPi / 180.0f;
    const float pitch = pitchDegrees * kPi / 180.0f;
    const float eye[3] = {targetX + distance * std::cos(pitch) * std::sin(yaw), distance * std::sin(pitch),
                          targetZ + distance * std::cos(pitch) * std::cos(yaw)};
    const float target[3] = {targetX, 0.0f, targetZ};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    StrataCamera cam{};
    strata_camera_look_at(eye, target, up, 40.0f, aspect, std::max(0.5f, distance * 0.05f), distance * 4.0f, &cam);
    return cam;
}

void OrbitCamera::pan(float dx, float dz) {
    const float yaw = yawDegrees * kPi / 180.0f;
    // Right = (cos yaw, -sin yaw); forward (away from the eye) = (-sin yaw, -cos yaw).
    targetX += dx * std::cos(yaw) - dz * std::sin(yaw);
    targetZ += -dx * std::sin(yaw) - dz * std::cos(yaw);
}

void OrbitCamera::zoom(float factor) { distance = std::min(std::max(distance * factor, 8.0f), 2000.0f); }

OrbitCamera fitCamera(const MicroEnvironment& env, float aspect) {
    OrbitCamera cam;
    const float w = static_cast<float>(env.config().width);
    const float h = static_cast<float>(env.config().height);
    // Fit the larger of the chamber's width (scaled by aspect) and depth into a 40 degree view.
    const float extent = std::max(w / std::max(aspect, 0.1f), h);
    cam.distance = extent * 1.45f;
    cam.targetZ = h * 0.06f;
    return cam;
}

const StrataTerrainTile& Scene::buildTerrain(const MicroEnvironment& env, FieldView view) {
    const int w = env.config().width;
    const int h = env.config().height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    heights_.resize(n);
    colors_.resize(n * 3);
    const auto& grains = env.grains();
    const auto& nut = env.nutrient().values();
    const auto& waste = env.waste().values();
    const auto& oxy = env.oxygen().values();
    const float oxyScale = std::max(env.config().oxygenSurface, 1e-3f);

    for (size_t i = 0; i < n; ++i) {
        float* rgb = &colors_[i * 3];
        if (grains.cells()[i] != 0) {
            heights_[i] = kGrainHeight;
            rgb[0] = kGrainRgb[0];
            rgb[1] = kGrainRgb[1];
            rgb[2] = kGrainRgb[2];
            continue;
        }
        const float tn = saturate(nut[i], 4.0f);
        const float tw = saturate(waste[i], 3.0f);
        const float to = std::min(oxy[i] / oxyScale, 1.0f);
        // A little relief where nutrient is rich, so the field reads as 2.5D.
        heights_[i] = 0.35f * tn;
        switch (view) {
            case FieldView::Nutrient: strata::sim::colormapRgb(strata::sim::Colormap::Viridis, tn, rgb); break;
            case FieldView::Waste: strata::sim::colormapRgb(strata::sim::Colormap::Magma, tw, rgb); break;
            case FieldView::Oxygen: strata::sim::colormapRgb(strata::sim::Colormap::Ice, to, rgb); break;
            case FieldView::Composite:
            default:
                rgb[0] = 0.015f + 0.030f * to + 0.090f * tn + 0.420f * tw;
                rgb[1] = 0.020f + 0.120f * to + 0.520f * tn + 0.060f * tw;
                rgb[2] = 0.040f + 0.380f * to + 0.060f * tn + 0.300f * tw;
                break;
        }
    }

    tile_ = StrataTerrainTile{};
    tile_.origin[0] = -static_cast<float>(w) * 0.5f + 0.5f;
    tile_.origin[1] = 0.0f;
    tile_.origin[2] = -static_cast<float>(h) * 0.5f + 0.5f;
    tile_.cell_size = 1.0f;
    tile_.samples_x = w;
    tile_.samples_z = h;
    tile_.heights = heights_.data();
    tile_.colors = colors_.data();
    tile_.color[0] = tile_.color[1] = tile_.color[2] = tile_.color[3] = 1.0f;
    return tile_;
}

const std::vector<StrataRenderItem>& Scene::buildItems(const MicroEnvironment& env) {
    items_.clear();
    const float halfW = static_cast<float>(env.config().width) * 0.5f;
    const float halfH = static_cast<float>(env.config().height) * 0.5f;
    const float threshold = env.config().reproductionEnergyThreshold;
    env.world().each<Body, Cell>([&](strata::sim::Entity, const Body& b, const Cell& c) {
        const SpeciesTraits& t = traitsOf(c.species);
        const float e = std::min(std::max(c.energy / threshold, 0.0f), 1.0f);
        StrataRenderItem it{};
        it.position[0] = b.x - halfW;
        it.position[2] = b.y - halfH;
        // Brighter with more energy; dim cells are close to starving.
        const float glow = 0.35f + 0.65f * e;
        it.color[0] = t.color[0] * glow;
        it.color[1] = t.color[1] * glow;
        it.color[2] = t.color[2] * glow;
        it.color[3] = 1.0f;
        if (b.attached) {
            // Biofilm: flat, matted cells.
            it.kind = STRATA_PRIMITIVE_CUBE;
            it.position[1] = 0.2f;
            it.scale[0] = 0.85f;
            it.scale[1] = 0.4f;
            it.scale[2] = 0.85f;
        } else {
            it.kind = STRATA_PRIMITIVE_SPHERE;
            const float s = 0.55f + 0.5f * e;
            it.position[1] = 0.3f + s * 0.5f;
            it.scale[0] = it.scale[1] = it.scale[2] = s;
        }
        items_.push_back(it);
    });
    return items_;
}

const std::vector<StrataPointLight>& Scene::buildSeepLights(const MicroEnvironment& env) {
    lights_.clear();
    const float halfW = static_cast<float>(env.config().width) * 0.5f;
    const float halfH = static_cast<float>(env.config().height) * 0.5f;
    for (const auto& s : env.sources()) {
        if (lights_.size() >= static_cast<size_t>(STRATA_MAX_POINT_LIGHTS)) break;
        StrataPointLight l{};
        l.position[0] = static_cast<float>(s[0]) + 0.5f - halfW;
        l.position[1] = 3.0f;
        l.position[2] = static_cast<float>(s[1]) + 0.5f - halfH;
        l.range = 14.0f;
        l.color[0] = 0.55f;
        l.color[1] = 1.0f;
        l.color[2] = 0.45f;
        l.intensity = 0.9f;
        lights_.push_back(l);
    }
    return lights_;
}

}  // namespace mbstrata
