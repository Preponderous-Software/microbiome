#include "scene.h"

#include <algorithm>
#include <cmath>


namespace mbstrata {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kGrainHeight = 2.4f;

}  // namespace

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
        const float tn = saturateConcentration(nut[i], kNutrientHalf);
        const float tw = saturateConcentration(waste[i], kWasteHalf);
        const float to = std::min(oxy[i] / oxyScale, 1.0f);
        // A little relief where nutrient is rich, so the field reads as 2.5D.
        heights_[i] = 0.35f * tn;
        fieldRgb(view, tn, tw, to, rgb);
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
    const Player& player = env.player();
    env.world().each<Body, Cell>([&](strata::sim::Entity ent, const Body& b, const Cell& c) {
        const bool you = player.embodied && ent == player.entity;
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
            // The embodied cell is drawn a little larger than its neighbours.
            const float s = (0.55f + 0.5f * e) * (you ? 1.35f : 1.0f);
            it.position[1] = 0.3f + s * 0.5f;
            it.scale[0] = it.scale[1] = it.scale[2] = s;
        }
        items_.push_back(it);
    });

    // Survival mode: the amoebae, the embodied cell's beacon and its toxin burst.
    const float divide = env.config().predatorDivide;
    for (const Predator& p : env.predators()) {
        const float e = std::min(std::max(p.energy / divide, 0.0f), 1.0f);
        StrataRenderItem it{};
        it.kind = STRATA_PRIMITIVE_SPHERE;
        const float s = 2.0f + 1.4f * e;  // swells as it feeds
        it.position[0] = p.x - halfW;
        it.position[1] = 0.25f + s * 0.35f;
        it.position[2] = p.y - halfH;
        it.scale[0] = s;
        it.scale[1] = s * 0.7f;  // a flattened, crawling blob
        it.scale[2] = s;
        const bool stunned = p.stunTicks > 0;
        it.color[0] = stunned ? 0.45f : 0.95f;
        it.color[1] = stunned ? 0.50f : 0.28f;
        it.color[2] = stunned ? 0.55f : 0.30f;
        it.color[3] = 1.0f;
        items_.push_back(it);
        // A darker nucleus riding on top.
        StrataRenderItem n = it;
        n.scale[0] = n.scale[1] = n.scale[2] = s * 0.32f;
        n.position[1] = it.position[1] + s * 0.28f;
        n.color[0] = stunned ? 0.3f : 0.45f;
        n.color[1] = 0.08f;
        n.color[2] = stunned ? 0.3f : 0.12f;
        items_.push_back(n);
    }
    if (player.embodied) {
        if (const Body* b = env.world().get<Body>(player.entity)) {
            // A white disc under the player and a column over it, so it never
            // gets lost in a crowd.
            StrataRenderItem disc{};
            disc.kind = STRATA_PRIMITIVE_CYLINDER;
            disc.position[0] = b->x - halfW;
            disc.position[2] = b->y - halfH;
            disc.position[1] = 0.42f;  // above the terrain's nutrient relief (at most 0.35)
            disc.scale[0] = disc.scale[2] = 2.4f;
            disc.scale[1] = 0.04f;
            disc.color[0] = disc.color[1] = disc.color[2] = 0.95f;
            disc.color[3] = 1.0f;
            items_.push_back(disc);
            StrataRenderItem it = disc;
            it.scale[0] = it.scale[2] = 0.4f;
            it.scale[1] = 2.4f;
            it.position[1] = 2.0f + it.scale[1];
            items_.push_back(it);
            // A bright cap on the column, the first thing the eye finds.
            StrataRenderItem cap{};
            cap.kind = STRATA_PRIMITIVE_SPHERE;
            cap.position[0] = it.position[0];
            cap.position[1] = 2.0f + 2.0f * it.scale[1] + 0.4f;
            cap.position[2] = it.position[2];
            cap.scale[0] = cap.scale[1] = cap.scale[2] = 1.1f;
            cap.color[0] = 1.0f;
            cap.color[1] = 0.95f;
            cap.color[2] = 0.55f;
            cap.color[3] = 1.0f;
            items_.push_back(cap);
            // A heading marker just ahead of it.
            StrataRenderItem h{};
            h.kind = STRATA_PRIMITIVE_SPHERE;
            h.position[0] = b->x + b->hx * 1.1f - halfW;
            h.position[1] = 0.7f;
            h.position[2] = b->y + b->hy * 1.1f - halfH;
            h.scale[0] = h.scale[1] = h.scale[2] = 0.38f;
            h.color[0] = 1.0f;
            h.color[1] = 1.0f;
            h.color[2] = player.dashing ? 0.3f : 1.0f;
            h.color[3] = 1.0f;
            items_.push_back(h);
            // A slowly pulsing ring of beads held above the tallest cells
            // (a free cell tops out near 1.4), so the player stays visible in
            // the middle of a colony.
            const int beads = 16;
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(env.tick() % 40) / 40.0f * 2.0f * kPi);
            const float ring = 2.8f + 0.45f * pulse;
            for (int i = 0; i < beads; ++i) {
                const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(beads);
                StrataRenderItem bead{};
                bead.kind = STRATA_PRIMITIVE_SPHERE;
                bead.position[0] = b->x + ring * std::cos(a) - halfW;
                bead.position[1] = 1.6f;
                bead.position[2] = b->y + ring * std::sin(a) - halfH;
                bead.scale[0] = bead.scale[1] = bead.scale[2] = 0.5f;
                bead.color[0] = bead.color[1] = bead.color[2] = 0.85f + 0.15f * pulse;
                bead.color[3] = 1.0f;
                items_.push_back(bead);
            }
        }
    }
    const uint64_t age = env.tick() - player.burstTick;
    if (player.burst && age < 8) {
        // The bacteriocin burst: a green ring that spreads to the toxin's radius.
        const float r = (env.config().toxinRadius + static_cast<float>(player.level(Mutation::PotentToxin))) * (0.45f + 0.55f * static_cast<float>(age + 1) / 8.0f);
        const int segments = 24;
        for (int i = 0; i < segments; ++i) {
            const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
            StrataRenderItem it{};
            it.kind = STRATA_PRIMITIVE_SPHERE;
            it.position[0] = player.burstX + r * std::cos(a) - halfW;
            it.position[1] = 0.6f;
            it.position[2] = player.burstY + r * std::sin(a) - halfH;
            it.scale[0] = it.scale[1] = it.scale[2] = 0.55f;
            it.color[0] = 0.35f;
            it.color[1] = 1.0f;
            it.color[2] = 0.45f;
            it.color[3] = 1.0f;
            items_.push_back(it);
        }
    }
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
    // A cool white light over the embodied cell lifts it out of the crowd.
    const Player& player = env.player();
    if (player.embodied && lights_.size() < static_cast<size_t>(STRATA_MAX_POINT_LIGHTS)) {
        if (const Body* b = env.world().get<Body>(player.entity)) {
            StrataPointLight l{};
            l.position[0] = b->x - halfW;
            l.position[1] = 3.5f;
            l.position[2] = b->y - halfH;
            l.range = 10.0f;
            l.color[0] = 0.85f;
            l.color[1] = 0.92f;
            l.color[2] = 1.0f;
            l.intensity = 3.0f;
            lights_.push_back(l);
        }
    }
    return lights_;
}

}  // namespace mbstrata
