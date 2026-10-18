// Headless render smoke test: the micro-environment drawn through Strata's
// EGL surfaceless surface. Needs a GL 4.5 driver at run time (CI uses Mesa
// llvmpipe: LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe).
#include "catch2.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

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
