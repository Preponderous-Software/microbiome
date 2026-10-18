// mb_strata: the Microbiome micro-environment, simulated and drawn with Strata.
//
//   mb_strata [--seed N] [--size N | --width N --height N] [--per-species N]
//             [--view composite|nutrient|waste|oxygen] [--zoom F] [--yaw DEG]
//             [--headless --ticks N [--screenshot out.png] [--frames DIR --frame-every N]]
//             [--bench --ticks N]
//             [--window-width W --window-height H]
//
// Without --headless or --bench it opens a window. The simulation runs on a
// fixed tick (strata::sim::FixedStep); the window's clock only decides how
// many ticks to run per frame, never what a tick does.
//
// mb_strata reports no usage and writes no log files.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "strata/sim/fixed_step.hpp"
#include "strata/strata.hpp"

#include "../src/micro_environment.h"
#include "../src/scene.h"

using namespace mbstrata;

namespace {

// SDL3 scancodes (USB HID usage ids), as StrataEvent.scancode carries them.
enum : int32_t {
    kA = 4, kD = 7, kE = 8, kH = 11, kQ = 20, kR = 21, kS = 22, kW = 26,
    k1 = 30, k2 = 31, k3 = 32, k4 = 33,
    kEscape = 41, kSpace = 44, kMinus = 45, kEquals = 46, kComma = 54, kPeriod = 55,
    kRight = 79, kLeft = 80, kDown = 81, kUp = 82, kKpMinus = 86, kKpPlus = 87
};

struct Args {
    Config config;
    bool headless = false;
    bool bench = false;
    long ticks = 600;
    std::string screenshot;
    std::string framesDir;
    long frameEvery = 10;
    int windowWidth = 1280;
    int windowHeight = 800;
    FieldView view = FieldView::Composite;
    double ticksPerSecond = 30.0;
    float zoom = 1.0f;
    float yaw = 0.0f;
};

void usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s [--seed N] [--size N | --width N --height N] [--per-species N]\n"
                 "          [--view composite|nutrient|waste|oxygen] [--tps N] [--zoom F] [--yaw DEG]\n"
                 "          [--headless --ticks N [--screenshot out.png] [--frames DIR --frame-every N]]\n"
                 "          [--bench --ticks N] [--window-width W --window-height H]\n",
                 argv0);
}

bool parseView(const std::string& s, FieldView& out) {
    if (s == "composite") out = FieldView::Composite;
    else if (s == "nutrient") out = FieldView::Nutrient;
    else if (s == "waste") out = FieldView::Waste;
    else if (s == "oxygen") out = FieldView::Oxygen;
    else return false;
    return true;
}

bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(k + " needs a value");
            return argv[++i];
        };
        if (k == "--seed") a.config.seed = std::strtoull(val().c_str(), nullptr, 10);
        else if (k == "--size") a.config.width = a.config.height = std::atoi(val().c_str());
        else if (k == "--width") a.config.width = std::atoi(val().c_str());
        else if (k == "--height") a.config.height = std::atoi(val().c_str());
        else if (k == "--per-species") a.config.initialPerSpecies = std::atoi(val().c_str());
        else if (k == "--max-population") a.config.maxPopulation = std::atoi(val().c_str());
        else if (k == "--ticks") a.ticks = std::atol(val().c_str());
        else if (k == "--headless") a.headless = true;
        else if (k == "--bench") a.bench = true;
        else if (k == "--screenshot") a.screenshot = val();
        else if (k == "--frames") a.framesDir = val();
        else if (k == "--frame-every") a.frameEvery = std::max(1L, std::atol(val().c_str()));
        else if (k == "--window-width") a.windowWidth = std::atoi(val().c_str());
        else if (k == "--window-height") a.windowHeight = std::atoi(val().c_str());
        else if (k == "--zoom") a.zoom = std::max(0.1f, static_cast<float>(std::atof(val().c_str())));
        else if (k == "--yaw") a.yaw = static_cast<float>(std::atof(val().c_str()));
        else if (k == "--tps") a.ticksPerSecond = std::max(1.0, std::atof(val().c_str()));
        else if (k == "--view") {
            if (!parseView(val(), a.view)) return false;
        } else return false;
    }
    if (a.config.width < 8 || a.config.height < 8 || a.config.width > 1025 || a.config.height > 1025) {
        std::fprintf(stderr, "the chamber must be between 8 and 1025 cells on each side\n");
        return false;
    }
    return true;
}

int bench(const Args& a) {
    MicroEnvironment env(a.config);
    const auto start = std::chrono::steady_clock::now();
    long populationSum = 0;
    for (long t = 0; t < a.ticks; ++t) {
        env.step();
        populationSum += static_cast<long>(env.world().size());
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("%s\n", env.summary().c_str());
    std::printf("bench: %ld ticks on %dx%d, mean population %.0f, %.3f s, %.1f ticks/s, state %016llx\n", a.ticks,
                a.config.width, a.config.height, static_cast<double>(populationSum) / static_cast<double>(std::max(1L, a.ticks)),
                secs, static_cast<double>(a.ticks) / std::max(secs, 1e-9),
                static_cast<unsigned long long>(env.stateHash()));
    return 0;
}

void help() {
    std::printf(
        "controls: WASD/arrows pan, Q/E rotate, mouse wheel or +/- zoom, 1-4 field view\n"
        "          (composite, nutrient, waste, oxygen), space pause, ,/. slower/faster,\n"
        "          R new seed, H help, Esc quit\n");
}

}  // namespace

int main(int argc, char** argv) {
    Args a;
    try {
        if (!parse(argc, argv, a)) {
            usage(argv[0]);
            return 2;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        usage(argv[0]);
        return 2;
    }
    if (a.bench) return bench(a);

    std::unique_ptr<MicroEnvironment> env = std::make_unique<MicroEnvironment>(a.config);

    strata::EngineDesc desc;
    desc.surface = a.headless ? STRATA_SURFACE_HEADLESS : STRATA_SURFACE_WINDOW;
    desc.width = a.windowWidth;
    desc.height = a.windowHeight;
    desc.title = "Microbiome on Strata";
    desc.flags = a.headless ? 0 : STRATA_FLAG_VSYNC;
    std::unique_ptr<strata::Engine> engine;
    try {
        engine = strata::Engine::create(desc);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "mb_strata: cannot start the renderer: %s\n", e.what());
        return 1;
    }

    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    light.time_of_day = 0.40f;
    light.latitude_degrees = 30.0f;
    light.fog = 0;
    light.cascade_count = 2;
    engine->setLighting(light);

    int w = desc.width, h = desc.height;
    auto aspect = [&] { return static_cast<float>(w) / static_cast<float>(std::max(1, h)); };
    OrbitCamera camera = fitCamera(*env, aspect());
    camera.distance /= a.zoom;
    camera.yawDegrees = a.yaw;
    Scene scene;
    FieldView view = a.view;

    auto draw = [&] {
        light.shadow_distance = camera.distance * 2.5f;
        engine->setLighting(light);
        engine->setCamera(camera.toStrata(aspect()));
        engine->setTerrain(&scene.buildTerrain(*env, view));
        const auto& lights = scene.buildSeepLights(*env);
        engine->setPointLights(lights.data(), lights.size());
        StrataFrameDesc fd{};
        fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
        fd.clear_color[0] = 0.010f;
        fd.clear_color[1] = 0.012f;
        fd.clear_color[2] = 0.020f;
        fd.clear_color[3] = 1.0f;
        engine->beginFrame(fd);
        const auto& items = scene.buildItems(*env);
        engine->submitPrimitives(items.data(), items.size());
        engine->endFrame();
    };

    if (a.headless) {
        int frame = 0;
        auto shoot = [&] {
            char name[64];
            std::snprintf(name, sizeof name, "/frame-%05d.png", frame++);
            draw();
            engine->saveScreenshotPng(a.framesDir + name);
        };
        for (long t = 0; t < a.ticks; ++t) {
            if (!a.framesDir.empty() && t % a.frameEvery == 0) shoot();
            env->step();
            if ((t + 1) % 200 == 0) std::printf("%s\n", env->summary().c_str());
        }
        if (!a.framesDir.empty()) shoot();
        draw();
        if (!a.screenshot.empty()) engine->saveScreenshotPng(a.screenshot);
        std::printf("%s\nstate %016llx\n", env->summary().c_str(), static_cast<unsigned long long>(env->stateHash()));
        return 0;
    }

    help();
    static const double kSpeeds[] = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
    int speed = 2;
    strata::sim::FixedStep clock(1.0 / a.ticksPerSecond, 8);
    bool running = true, paused = false, dragging = false;
    float dragX = 0, dragY = 0;
    auto last = std::chrono::steady_clock::now();
    uint64_t lastReport = 0;
    while (running) {
        StrataEvent e;
        while (engine->pollEvent(e)) {
            if (e.type == STRATA_EVENT_QUIT) running = false;
            else if (e.type == STRATA_EVENT_RESIZED) engine->drawableSize(w, h);
            else if (e.type == STRATA_EVENT_MOUSE_WHEEL) camera.zoom(e.y > 0 ? 0.88f : 1.0f / 0.88f);
            else if (e.type == STRATA_EVENT_MOUSE_BUTTON_DOWN && e.button == 1) {
                dragging = true;
                dragX = e.x;
                dragY = e.y;
            } else if (e.type == STRATA_EVENT_MOUSE_BUTTON_UP && e.button == 1) dragging = false;
            else if (e.type == STRATA_EVENT_MOUSE_MOTION && dragging) {
                const float perPixel = camera.distance / static_cast<float>(std::max(1, h)) * 0.75f;
                camera.pan(-(e.x - dragX) * perPixel, (e.y - dragY) * perPixel);
                dragX = e.x;
                dragY = e.y;
            } else if (e.type == STRATA_EVENT_KEY_DOWN) {
                const float step = camera.distance * 0.04f;
                switch (e.scancode) {
                    case kEscape: running = false; break;
                    case kW: case kUp: camera.pan(0, step); break;
                    case kS: case kDown: camera.pan(0, -step); break;
                    case kA: case kLeft: camera.pan(-step, 0); break;
                    case kD: case kRight: camera.pan(step, 0); break;
                    case kQ: camera.yawDegrees -= 5.0f; break;
                    case kE: camera.yawDegrees += 5.0f; break;
                    case kEquals: case kKpPlus: camera.zoom(0.9f); break;
                    case kMinus: case kKpMinus: camera.zoom(1.0f / 0.9f); break;
                    case k1: view = FieldView::Composite; break;
                    case k2: view = FieldView::Nutrient; break;
                    case k3: view = FieldView::Waste; break;
                    case k4: view = FieldView::Oxygen; break;
                    case kSpace: if (!e.repeat) paused = !paused; break;
                    case kComma: speed = std::max(speed - 1, 0); break;
                    case kPeriod: speed = std::min(speed + 1, 5); break;
                    case kH: help(); break;
                    case kR:
                        if (!e.repeat) {
                            a.config.seed += 1;
                            env = std::make_unique<MicroEnvironment>(a.config);
                            std::printf("new chamber, seed %llu\n", static_cast<unsigned long long>(a.config.seed));
                        }
                        break;
                    default: break;
                }
                if (e.scancode >= k1 && e.scancode <= k4) std::printf("view: %s\n", fieldViewName(view));
            }
        }
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - last).count();
        last = now;
        clock.setTimeScale(paused ? 0.0 : kSpeeds[speed]);
        const int ticks = clock.advance(elapsed);
        for (int i = 0; i < ticks; ++i) env->step();
        if (env->tick() >= lastReport + 100) {
            lastReport = env->tick();
            std::printf("%s\n", env->summary().c_str());
        }
        draw();
        engine->present();
    }
    return 0;
}
