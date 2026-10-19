// mb_strata: the Microbiome micro-environment, simulated and drawn with Strata.
//
//   mb_strata [--seed N] [--size N | --width N --height N] [--per-species N]
//             [--view composite|nutrient|waste|oxygen] [--zoom F] [--yaw DEG]
//             [--no-hud] [--hud-scale N] [--cascades N] [--shadow-map N]
//             [--headless --ticks N [--screenshot out.png] [--frames DIR --frame-every N]]
//             [--bench --ticks N]
//             [--window-width W --window-height H]
//             [--record out.usda [--record-every N] [--record-field N]]
//
// Without --headless or --bench it opens a window. The simulation runs on a
// fixed tick (strata::sim::FixedStep); the window's clock only decides how
// many ticks to run per frame, never what a tick does.
//
// The HUD (src/hud.*) draws live statistics with Strata's overlays; it only
// reads the simulation. Tab toggles it in the window, --no-hud turns it off.
//
// --record writes the run as a usd-replay/1 replay (src/replay.*): one sample
// every N ticks (default 10) plus the first and last, the field heatmap of
// --view at --record-field columns (default 32). Headless, the file is written
// after the last tick; in the window, on quit (or before R starts a new
// chamber, which ends the recording). Recording only reads the simulation.
//
// Built with Emscripten (docs/STRATA.md, "In a browser") the window is a
// WebGL 2 canvas and the browser runs the frame loop: main() schedules one
// App::frame per requestAnimationFrame and returns. The page
// (strata/web/shell.html) drives the same camera, view and clock through the
// mb_* functions below, for mouse and touch.
//
// mb_strata reports no usage and writes no log files.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "strata/sim/fixed_step.hpp"
#include "strata/strata.hpp"

#include "../src/hud.h"
#include "../src/micro_environment.h"
#include "../src/replay.h"
#include "../src/scene.h"

using namespace mbstrata;

namespace {

// SDL3 scancodes (USB HID usage ids), as StrataEvent.scancode carries them.
enum : int32_t {
    kA = 4, kD = 7, kE = 8, kH = 11, kQ = 20, kR = 21, kS = 22, kW = 26,
    k1 = 30, k2 = 31, k3 = 32, k4 = 33,
    kEscape = 41, kTab = 43, kSpace = 44, kMinus = 45, kEquals = 46, kComma = 54, kPeriod = 55,
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
    bool hud = true;
    float hudScale = 0.0f;  // glyph cell in pixels; 0 picks one from the window height
    int cascades = 2;       // sun shadow cascades, 0 (no shadows) to 4
    int shadowMap = 0;      // texels per cascade side; 0 keeps Strata's default
    std::string record;     // usd-replay/1 output (.usda); empty: no recording
    long recordEvery = 10;
    int recordField = 32;
    std::string commandLine;
};

void usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s [--seed N] [--size N | --width N --height N] [--per-species N]\n"
                 "          [--view composite|nutrient|waste|oxygen] [--tps N] [--zoom F] [--yaw DEG]\n"
                 "          [--no-hud] [--hud-scale N] [--cascades N] [--shadow-map N]\n"
                 "          [--headless --ticks N [--screenshot out.png] [--frames DIR --frame-every N]]\n"
                 "          [--bench --ticks N] [--window-width W --window-height H]\n"
                 "          [--record out.usda [--record-every N] [--record-field N]]\n",
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
        else if (k == "--no-hud") a.hud = false;
        else if (k == "--hud-scale") a.hudScale = static_cast<float>(8 * std::max(1, std::min(8, std::atoi(val().c_str()))));
        else if (k == "--record") a.record = val();
        else if (k == "--record-every") a.recordEvery = std::max(1L, std::atol(val().c_str()));
        else if (k == "--record-field") a.recordField = std::max(1, std::min(256, std::atoi(val().c_str())));
        else if (k == "--cascades") a.cascades = std::max(0, std::min(4, std::atoi(val().c_str())));
        else if (k == "--shadow-map") a.shadowMap = std::max(256, std::min(4096, std::atoi(val().c_str())));
        else if (k == "--tps") a.ticksPerSecond = std::max(1.0, std::atof(val().c_str()));
        else if (k == "--view") {
            if (!parseView(val(), a.view)) return false;
        } else return false;
    }
    if (a.config.width < 8 || a.config.height < 8 || a.config.width > 1025 || a.config.height > 1025) {
        std::fprintf(stderr, "the chamber must be between 8 and 1025 cells on each side\n");
        return false;
    }
    if (!a.record.empty() && (a.record.size() < 5 || a.record.compare(a.record.size() - 5, 5, ".usda") != 0)) {
        std::fprintf(stderr, "--record writes text USD: the path must end in .usda (convert with `usd-recorder replay convert`)\n");
        return false;
    }
    for (int i = 0; i < argc; ++i) a.commandLine += (i ? " " : "") + std::string(i ? argv[i] : "mb_strata");
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
        "          Tab statistics overlay, R new seed, H help, Esc quit\n");
}

// Everything a running mb_strata holds, on the heap: in a browser main()
// returns as soon as the first frame is scheduled, and the frames that follow
// (one per requestAnimationFrame) still need it.
struct App {
    Args a;
    std::unique_ptr<MicroEnvironment> env;
    std::unique_ptr<ReplayRecorder> recorder;
    std::unique_ptr<strata::Engine> engine;
    StrataLighting light{};
    int w = 0, h = 0;
    OrbitCamera camera;
    Scene scene;
    FieldView view = FieldView::Composite;
    Hud hud;
    StatsHistory history;
    HudState hudState;
    bool showHud = true;

    // The window's clock: decides how many ticks run per frame, never what a tick does.
    static constexpr double kSpeeds[6] = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
    int speed = 2;
    strata::sim::FixedStep clock{1.0 / 30.0, 8};
    bool running = true, paused = false, dragging = false;
    float dragX = 0, dragY = 0;
    std::chrono::steady_clock::time_point last, rateStart;
    long rateTicks = 0;
    uint64_t lastReport = 0;
    unsigned long frames = 0;

    float aspect() const { return static_cast<float>(w) / static_cast<float>(std::max(1, h)); }

    void stepOnce() {
        env->step();
        history.observe(*env);                  // reads only; kept while the HUD is hidden
        if (recorder) recorder->observe(*env);  // reads only, too
    }

    // Writes the recording (once) and ends it. False when the file could not be written.
    bool saveRecording() {
        if (!recorder) return true;
        recorder->finish(*env);
        try {
            recorder->write(a.record);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "mb_strata: cannot write the replay: %s\n", e.what());
            recorder.reset();
            return false;
        }
        std::printf("replay: %zu samples to %s\n", recorder->samples(), a.record.c_str());
        recorder.reset();
        return true;
    }

    void draw() {
        light.shadow_distance = camera.distance * 2.5f;
        engine->setLighting(light);
        const StrataCamera cam = camera.toStrata(aspect());
        engine->setCamera(cam);
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
        if (showHud) {
            hudState.view = view;
            hudState.seed = a.config.seed;
            hud.build(*env, history, hudState, cam, w, h, a.hudScale);
            hud.submit(*engine);
        }
        engine->endFrame();
    }

    // Pans by a drag of (dx, dy) drawable pixels: the floor follows the pointer.
    void panPixels(float dx, float dy) {
        const float perPixel = camera.distance / static_cast<float>(std::max(1, h)) * 0.75f;
        camera.pan(-dx * perPixel, dy * perPixel);
    }

    void setView(FieldView v) {
        view = v;
        std::printf("view: %s\n", fieldViewName(view));
    }

    void newChamber() {
        saveRecording();  // a new chamber ends the recording
        a.config.seed += 1;
        env = std::make_unique<MicroEnvironment>(a.config);
        history.clear();
        history.observe(*env);
        std::printf("new chamber, seed %llu\n", static_cast<unsigned long long>(a.config.seed));
    }

    void key(const StrataEvent& e) {
        const float step = camera.distance * 0.04f;
        switch (e.scancode) {
            case kEscape:
#if !defined(__EMSCRIPTEN__)
                running = false;  // a page cannot be quit
#endif
                break;
            case kW: case kUp: camera.pan(0, step); break;
            case kS: case kDown: camera.pan(0, -step); break;
            case kA: case kLeft: camera.pan(-step, 0); break;
            case kD: case kRight: camera.pan(step, 0); break;
            case kQ: camera.yawDegrees -= 5.0f; break;
            case kE: camera.yawDegrees += 5.0f; break;
            case kEquals: case kKpPlus: camera.zoom(0.9f); break;
            case kMinus: case kKpMinus: camera.zoom(1.0f / 0.9f); break;
            case k1: setView(FieldView::Composite); break;
            case k2: setView(FieldView::Nutrient); break;
            case k3: setView(FieldView::Waste); break;
            case k4: setView(FieldView::Oxygen); break;
            case kSpace: if (!e.repeat) paused = !paused; break;
            case kComma: speed = std::max(speed - 1, 0); break;
            case kPeriod: speed = std::min(speed + 1, 5); break;
            case kH: help(); break;
            case kTab: if (!e.repeat) showHud = !showHud; break;
            case kR: if (!e.repeat) newChamber(); break;
            default: break;
        }
    }

    // One window frame: events, the ticks this frame's time is worth, the draw.
    void frame() {
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
                panPixels(e.x - dragX, e.y - dragY);
                dragX = e.x;
                dragY = e.y;
            } else if (e.type == STRATA_EVENT_KEY_DOWN) key(e);
        }
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - last).count();
        last = now;
        clock.setTimeScale(paused ? 0.0 : kSpeeds[speed]);
        const int ticks = clock.advance(elapsed);
        for (int i = 0; i < ticks; ++i) stepOnce();
        rateTicks += ticks;
        const double rateSecs = std::chrono::duration<double>(now - rateStart).count();
        if (rateSecs >= 1.0) {
            hudState.measuredTicksPerSecond = static_cast<double>(rateTicks) / rateSecs;
            rateStart = now;
            rateTicks = 0;
        }
        hudState.paused = paused;
        hudState.speed = kSpeeds[speed];
        if (env->tick() >= lastReport + 100) {
            lastReport = env->tick();
            std::printf("%s\n", env->summary().c_str());
        }
        draw();
        engine->present();
        ++frames;
    }
};

App* g_app = nullptr;

#if defined(__EMSCRIPTEN__)
void browserFrame(void*) {
    g_app->frame();
    if (!g_app->running) emscripten_cancel_main_loop();
}
#endif

}  // namespace

#if defined(__EMSCRIPTEN__)
// The page's on-screen buttons and its pointer handling (mouse and touch,
// strata/web/shell.html) call these. Each does what the matching key or mouse
// gesture does in a window; none changes what a tick does.
extern "C" {
EMSCRIPTEN_KEEPALIVE void mb_pan(float dx, float dy) { if (g_app) g_app->panPixels(dx, dy); }
EMSCRIPTEN_KEEPALIVE void mb_zoom(float factor) {
    if (g_app && factor > 0.0f) g_app->camera.zoom(factor);
}
EMSCRIPTEN_KEEPALIVE void mb_rotate(float degrees) { if (g_app) g_app->camera.yawDegrees += degrees; }
EMSCRIPTEN_KEEPALIVE void mb_set_view(int v) {
    if (!g_app) return;
    static const FieldView views[] = {FieldView::Composite, FieldView::Nutrient, FieldView::Waste, FieldView::Oxygen};
    if (v >= 0 && v < 4) g_app->setView(views[v]);
}
EMSCRIPTEN_KEEPALIVE int mb_toggle_pause() { return g_app ? (g_app->paused = !g_app->paused) : 0; }
EMSCRIPTEN_KEEPALIVE double mb_change_speed(int delta) {
    if (!g_app) return 0;
    g_app->speed = std::max(0, std::min(5, g_app->speed + delta));
    return App::kSpeeds[g_app->speed];
}
EMSCRIPTEN_KEEPALIVE int mb_toggle_hud() { return g_app ? (g_app->showHud = !g_app->showHud) : 0; }
EMSCRIPTEN_KEEPALIVE void mb_new_chamber() { if (g_app) g_app->newChamber(); }
EMSCRIPTEN_KEEPALIVE void mb_reset_camera() {
    if (!g_app) return;
    g_app->camera = fitCamera(*g_app->env, g_app->aspect());
}
// For the smoke test: frames drawn, ticks run and the state hash so far.
EMSCRIPTEN_KEEPALIVE double mb_frames() { return g_app ? static_cast<double>(g_app->frames) : 0; }
EMSCRIPTEN_KEEPALIVE double mb_tick() { return g_app ? static_cast<double>(g_app->env->tick()) : 0; }
}
#endif

int main(int argc, char** argv) {
    Args args;
    try {
        if (!parse(argc, argv, args)) {
            usage(argv[0]);
            return 2;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        usage(argv[0]);
        return 2;
    }
    if (args.bench) return bench(args);

    g_app = new App();
    App& app = *g_app;
    app.a = args;
    const Args& a = app.a;
    app.env = std::make_unique<MicroEnvironment>(a.config);
    if (!a.record.empty()) {
        ReplayOptions ro;
        ro.every = a.recordEvery;
        ro.fieldColumns = a.recordField;
        ro.view = a.view;
        ro.ticksPerSecond = a.ticksPerSecond;
        ro.simVersion = MB_STRATA_SIM_VERSION;
        ro.source = a.commandLine;
        app.recorder = std::make_unique<ReplayRecorder>(*app.env, ro);
        app.recorder->observe(*app.env);
    }

    strata::EngineDesc desc;
    desc.surface = a.headless ? STRATA_SURFACE_HEADLESS : STRATA_SURFACE_WINDOW;
    desc.width = a.windowWidth;
    desc.height = a.windowHeight;
    desc.title = "Microbiome on Strata";
    desc.flags = a.headless ? 0 : STRATA_FLAG_VSYNC;
    try {
        app.engine = strata::Engine::create(desc);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "mb_strata: cannot start the renderer: %s\n", e.what());
        return 1;
    }

    app.light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    app.light.time_of_day = 0.40f;
    app.light.latitude_degrees = 30.0f;
    app.light.fog = 0;
    app.light.cascade_count = a.cascades;
    if (a.shadowMap > 0) app.light.shadow_map_size = a.shadowMap;
    app.engine->setLighting(app.light);

    app.w = desc.width;
    app.h = desc.height;
    app.engine->drawableSize(app.w, app.h);
    app.camera = fitCamera(*app.env, app.aspect());
    app.camera.distance /= a.zoom;
    app.camera.yawDegrees = a.yaw;
    app.view = a.view;
    app.hudState.ticksPerSecond = a.ticksPerSecond;
    app.hudState.headless = a.headless;
    app.showHud = a.hud;
    app.history.observe(*app.env);

    if (a.headless) {
        int frame = 0;
        auto shoot = [&] {
            char name[64];
            std::snprintf(name, sizeof name, "/frame-%05d.png", frame++);
            app.draw();
            app.engine->saveScreenshotPng(a.framesDir + name);
        };
        for (long t = 0; t < a.ticks; ++t) {
            if (!a.framesDir.empty() && t % a.frameEvery == 0) shoot();
            app.stepOnce();
            if ((t + 1) % 200 == 0) std::printf("%s\n", app.env->summary().c_str());
        }
        if (!a.framesDir.empty()) shoot();
        app.draw();
        if (!a.screenshot.empty()) app.engine->saveScreenshotPng(a.screenshot);
        std::printf("%s\nstate %016llx\n", app.env->summary().c_str(),
                    static_cast<unsigned long long>(app.env->stateHash()));
        const bool saved = app.saveRecording();
        app.engine.reset();
        delete g_app;
        g_app = nullptr;
        return saved ? 0 : 1;
    }

    help();
    app.clock = strata::sim::FixedStep(1.0 / a.ticksPerSecond, 8);
    app.last = app.rateStart = std::chrono::steady_clock::now();
#if defined(__EMSCRIPTEN__)
    // The browser runs the loop (requestAnimationFrame); main returns at once
    // and the app lives on in g_app.
    emscripten_set_main_loop_arg(browserFrame, nullptr, 0, false);
    return 0;
#else
    while (app.running) app.frame();
    const bool saved = app.saveRecording();
    app.engine.reset();
    delete g_app;
    g_app = nullptr;
    return saved ? 0 : 1;
#endif
}
