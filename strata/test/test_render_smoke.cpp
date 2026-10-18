// Headless render smoke test: the micro-environment drawn through Strata's
// EGL surfaceless surface. Needs a GL 4.5 driver at run time (CI uses Mesa
// llvmpipe: LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe).
#include "catch2.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "hud.h"
#include "micro_environment.h"
#include "scene.h"
#include "strata/strata.hpp"

using namespace mbstrata;

TEST_CASE("The micro-environment renders headlessly through Strata", "[strata][render]") {
    Config c;
    c.width = 64;
    c.height = 64;
    c.initialPerSpecies = 60;
    MicroEnvironment env(c);
    env.run(50);

    strata::EngineDesc desc;
    desc.surface = STRATA_SURFACE_HEADLESS;
    desc.width = 320;
    desc.height = 200;
    std::unique_ptr<strata::Engine> engine;
    try {
        engine = strata::Engine::create(desc);
    } catch (const std::exception& e) {
        FAIL("cannot create a headless Strata engine: " << e.what());
    }
    engine->setLighting(strata::lightingDefaults(STRATA_LIGHTING_SUN));

    Scene scene;
    const float aspect = 320.0f / 200.0f;
    engine->setCamera(fitCamera(env, aspect).toStrata(aspect));

    for (FieldView view : {FieldView::Composite, FieldView::Nutrient, FieldView::Waste, FieldView::Oxygen}) {
        engine->setTerrain(&scene.buildTerrain(env, view));
        StrataFrameDesc fd{};
        fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
        engine->beginFrame(fd);
        const auto& items = scene.buildItems(env);
        REQUIRE(items.size() == env.world().size());
        engine->submitPrimitives(items.data(), items.size());
        engine->endFrame();

        const StrataFrameStats& stats = engine->frameStats();
        CHECK(stats.items_rejected == 0);
        CHECK(stats.instances == static_cast<int32_t>(items.size()));

        std::vector<uint8_t> rgba;
        int32_t w = 0, h = 0;
        engine->readPixels(rgba, w, h);
        REQUIRE(w == 320);
        REQUIRE(h == 200);
        // Not a blank frame: the centre differs from the clear colour, and
        // the frame holds many distinct colours (field gradient + organisms).
        std::vector<uint32_t> seen;
        bool centreDrawn = false;
        for (int y = 0; y < h; y += 4)
            for (int x = 0; x < w; x += 4) {
                const uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
                const uint32_t key = (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
                if (seen.size() < 4096) seen.push_back(key);
                if (x == 160 && y == 100) centreDrawn = (p[0] | p[1] | p[2]) != 0;
            }
        std::sort(seen.begin(), seen.end());
        seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
        INFO("view " << fieldViewName(view) << ": " << seen.size() << " distinct colours");
        CHECK(centreDrawn);
        CHECK(seen.size() > 50);
    }

    const char* out = std::getenv("MB_STRATA_SMOKE_PNG");
    if (out != nullptr && *out != '\0') engine->saveScreenshotPng(out);
}

namespace {

// Projects a world point with the camera's view and projection to pixels.
void projectToPixel(const StrataCamera& c, const float p[3], int w, int h, float& px, float& py) {
    float v[4], clip[4];
    for (int r = 0; r < 4; ++r) v[r] = c.view[0 * 4 + r] * p[0] + c.view[1 * 4 + r] * p[1] + c.view[2 * 4 + r] * p[2] + c.view[3 * 4 + r];
    for (int r = 0; r < 4; ++r)
        clip[r] = c.projection[0 * 4 + r] * v[0] + c.projection[1 * 4 + r] * v[1] + c.projection[2 * 4 + r] * v[2] +
                  c.projection[3 * 4 + r] * v[3];
    px = (clip[0] / clip[3] * 0.5f + 0.5f) * static_cast<float>(w);
    py = (0.5f - clip[1] / clip[3] * 0.5f) * static_cast<float>(h);
}

}  // namespace

TEST_CASE("ScreenProjector puts a world point on the pixel it was made from", "[hud]") {
    Config c;
    c.width = 64;
    c.height = 48;
    MicroEnvironment env(c);
    for (float yaw : {0.0f, 37.0f, -120.0f}) {
        OrbitCamera oc = fitCamera(env, 1.6f);
        oc.yawDegrees = yaw;
        const StrataCamera cam = oc.toStrata(1.6f);
        const ScreenProjector proj(cam, 800, 500);
        for (float px : {0.5f, 400.0f, 799.5f})
            for (float py : {0.5f, 250.0f, 499.5f}) {
                float world[3];
                proj.toWorld(px, py, world);
                float qx = 0, qy = 0;
                projectToPixel(cam, world, 800, 500, qx, qy);
                CHECK(qx == Approx(px).margin(0.01));
                CHECK(qy == Approx(py).margin(0.01));
            }
    }
}

TEST_CASE("StatsHistory samples on its interval and keeps a bounded window", "[hud]") {
    Config c;
    c.width = 32;
    c.height = 32;
    c.initialPerSpecies = 20;
    MicroEnvironment env(c);
    StatsHistory history(5, 3);
    history.observe(env);
    for (int i = 0; i < 30; ++i) {
        env.step();
        history.observe(env);
        history.observe(env);  // a second look at the same tick adds nothing
    }
    const auto& s = history.samples();
    REQUIRE(s.size() == 5);
    CHECK(s.back().tick == 30);
    CHECK(s.front().tick == 18);
    const Stats st = env.stats();
    CHECK(s.back().population() == st.population());
    CHECK(s.back().totals[StatsHistory::Nutrient] == st.nutrientTotal);

    MicroEnvironment fresh(c);  // a new chamber starts the history over
    history.observe(fresh);
    REQUIRE(history.samples().size() == 1);
    CHECK(history.samples().back().tick == 0);
}

TEST_CASE("The HUD only reads the simulation", "[hud]") {
    Config c;
    c.width = 48;
    c.height = 48;
    c.initialPerSpecies = 40;
    MicroEnvironment watched(c), plain(c);
    StatsHistory history(16, 1);
    Hud hud;
    HudState state;
    const StrataCamera cam = fitCamera(watched, 1.5f).toStrata(1.5f);
    for (int i = 0; i < 120; ++i) {
        watched.step();
        history.observe(watched);
        hud.build(watched, history, state, cam, 480, 320);
        plain.step();
        REQUIRE(watched.stateHash() == plain.stateHash());
    }
}

TEST_CASE("The HUD draws through Strata's overlays", "[strata][render][hud]") {
    Config c;
    c.width = 64;
    c.height = 64;
    c.initialPerSpecies = 60;
    MicroEnvironment env(c);
    StatsHistory history(40, 2);
    for (int i = 0; i < 80; ++i) {
        env.step();
        history.observe(env);
    }

    strata::EngineDesc desc;
    desc.surface = STRATA_SURFACE_HEADLESS;
    desc.width = 640;
    desc.height = 400;
    std::unique_ptr<strata::Engine> engine;
    try {
        engine = strata::Engine::create(desc);
    } catch (const std::exception& e) {
        FAIL("cannot create a headless Strata engine: " << e.what());
    }
    REQUIRE(strata_abi_minor() >= 5u);
    engine->setLighting(strata::lightingDefaults(STRATA_LIGHTING_SUN));
    const float aspect = 640.0f / 400.0f;
    const StrataCamera cam = fitCamera(env, aspect).toStrata(aspect);
    engine->setCamera(cam);
    Scene scene;
    engine->setTerrain(&scene.buildTerrain(env, FieldView::Nutrient));

    auto render = [&](bool withHud, FieldView view) {
        StrataFrameDesc fd{};
        fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
        engine->beginFrame(fd);
        const auto& items = scene.buildItems(env);
        engine->submitPrimitives(items.data(), items.size());
        if (withHud) {
            Hud hud;
            HudState state;
            state.headless = true;
            state.view = view;
            hud.build(env, history, state, cam, 640, 400);
            CHECK(Hud::pixelSizeFor(400) == 8.0f);
            CHECK(hud.texts().size() > 10);
            CHECK(hud.lines().size() % 2 == 0);
            CHECK(hud.points().size() == static_cast<size_t>(kSpeciesCount));
            REQUIRE_NOTHROW(hud.submit(*engine));
        }
        engine->endFrame();
        std::vector<uint8_t> rgba;
        int32_t w = 0, h = 0;
        engine->readPixels(rgba, w, h);
        return rgba;
    };

    const std::vector<uint8_t> bare = render(false, FieldView::Nutrient);
    for (FieldView view : {FieldView::Composite, FieldView::Nutrient, FieldView::Waste, FieldView::Oxygen}) {
        const std::vector<uint8_t> withHud = render(true, view);
        REQUIRE(withHud.size() == bare.size());
        // The text panel in the top-left corner changes many pixels there,
        // and the title is drawn in bright green.
        int changed = 0, title = 0;
        for (int y = 8; y < 120; ++y)
            for (int x = 8; x < 240; ++x) {
                const size_t i = (static_cast<size_t>(y) * 640 + x) * 4;
                if (std::abs(int(withHud[i]) - int(bare[i])) + std::abs(int(withHud[i + 1]) - int(bare[i + 1])) > 24) ++changed;
                if (y < 24 && withHud[i + 1] > 200 && withHud[i] < 200) ++title;
            }
        INFO("view " << fieldViewName(view) << ": " << changed << " changed, " << title << " title pixels");
        CHECK(changed > 500);
        CHECK(title > 40);
    }

    const char* out = std::getenv("MB_STRATA_HUD_PNG");
    if (out != nullptr && *out != '\0') engine->saveScreenshotPng(out);
}
