# Microbiome on Strata

This note describes how Microbiome's micro-environment is simulated and drawn
with [Strata](https://github.com/Preponderous-Software/strata), the shared C++
engine, and what stays on the existing code path. It is the design for the
opt-in `mb_strata` build.

## Goal

Move from a 10×10 ASCII grid of abstract organisms to a richer
micro-environment:

- a 128×128 (or larger) chamber with **diffusing fields**: nutrient, waste
  and oxygen, updated every tick by diffusion and decay;
- **solid grains** (obstacles) whose surfaces biofilms can form on;
- microorganisms as **Strata entities** that keep the console app's traits
  (energy, metabolic rate, binary fission, death returning biomass to the
  environment, chemotaxis) and add run-and-tumble swimming up gradients,
  consumption and excretion into the fields, and species differences;
- **rendering through Strata**: the fields as a heatmap on a terrain tile,
  organisms as instanced primitives, a camera that pans and zooms, and a
  headless screenshot/frame mode.

## What moves to Strata and what stays

| Piece | Where it lives |
|---|---|
| Seeded RNG, entity registry, 2D fields with diffusion/decay, fixed-step clock, state hash, colormaps | Strata `modules/sim` (`strata::sim`). Generic: nothing microbiome-specific. |
| Rendering: window/headless surface, camera, terrain tile, instanced primitives, point lights, screenshots | Strata engine (`strata::static`, `strata.hpp`) |
| Micro-environment rules: species, chemotaxis, feeding, excretion, biofilm attachment, fission, death | Microbiome, `strata/src/micro_environment.*` |
| Drawing the environment (heatmap colours, organism shapes, camera rig) | Microbiome, `strata/src/scene.*`, `strata/app/main.cpp` |
| Console app `mb_app`, live viewer `mb_webapp` (microbiome.preponderous.org), classic browser build (`src/browser.cpp`), `env-lib-cpp`, both test suites, Docker image | **Unchanged.** They keep the existing `Microbiome`/`Environment` simulation and the Makefile. |

Since Strata gained a WebGL 2 backend, `mb_strata` is also the arcade page
(`microbiome.play.danielstephenson.dev`, see [In a browser](#in-a-browser));
`src/browser.cpp` stays reachable there at `/classic/`.

## The model

Three species share one chamber whose top edge is an air interface (oxygen
held at saturation) and whose nutrient comes from seeps below it, so nutrient
and oxygen form opposing gradients, as in a Winogradsky column (RESEARCH.md
§6):

| Species | Eats | Notes |
|---|---|---|
| Aerobe | nutrient | Fast swimmer. With oxygen it gets a much higher yield (respiration); without it, a poor one. |
| Fermenter | nutrient | Anaerobe: oxygen above its tolerance costs energy. Excretes more waste. Settles into **biofilms** when it touches a grain or the chamber wall, or when crowded (a quorum, RESEARCH.md §4.3); a starving biofilm cell disperses. |
| Cross-feeder | waste | Syntrophy (RESEARCH.md §4.2): lives on the other species' waste and swims up the waste gradient. |

Kept from the console simulation (same constants): initial energy in
(100, 1000], metabolic rate 1–4 per tick, fission at 900 energy with a cost
of 100 split between two daughters that inherit the metabolic rate, 60
energy of biomass returned on death (now into the nutrient field rather than
as a `Biomatter` entity), and `chemotaxisBiasPercent` = 80, which now biases
the direction a tumble picks towards the food gradient.

Per tick, in this order: every organism (in registry order) swims (run and
tumble with temporal sensing), may attach or detach, pays its metabolic cost
plus any oxygen or waste toxicity, eats from its cell (taking oxygen if it
respires) and excretes waste, dies (releasing nutrient) or divides; births
are created after the pass. Then the fields update: seeps add nutrient, the
air interface is reset to saturation, and each field diffuses (flux form,
closed walls) and decays.

## Statistics overlay

`strata/src/hud.*` draws live statistics over the chamber with Strata's
unlit overlays (ABI 1.5, `strata_submit_text`/`_lines`/`_points`):

- top left: tick, simulated time (tick / `--tps`), seed, speed or pause
  state and the measured tick rate in the window; population per species
  (with a point in the species colour), the biofilm count, births and
  deaths; the chamber totals of nutrient, waste, oxygen and cell energy with
  their change per tick, next to what the seeps add each tick;
- bottom left: a legend for the current field view, a colour bar labelled at
  concentrations for the single-field views or swatches for the composite;
- bottom right: a sparkline of each species' population and the total over
  the last 960 ticks (`StatsHistory`, one sample every 4 ticks).

Text is in pixels; overlay lines and points are in world space, so the HUD
places them just past the camera's near plane under the pixel they belong
on (`ScreenProjector`). Panels are one overlay line per pixel row.

The HUD only reads the simulation (`MicroEnvironment::stats()`), so it
cannot change a run: `mb_strata --headless` gives the same state hash with
and without `--no-hud`, and a test checks the hash tick by tick while the
HUD is built. Tab toggles it in the window; `--hud-scale N` sets the glyph
size to 8·N pixels (by default it follows the drawable height).

## Replays

`strata/src/replay.*` records a run as a `usd-replay/1` replay (usd-recorder's
`docs/REPLAY.md`) through usd-recorder's vendored, dependency-free C++ writer
(`strata/vendor/usd_recorder/`, v0.2.0): `mb_strata --record run.usda`.

- One sample every `--record-every` ticks (default 10) plus the first and the
  last; the time code is the tick and `tickRate` is `--tps`.
- One `Points` set per species. An entity's id is
  `(generation << 32) | slot`, so it is stable for the cell's life and never
  reused by another cell. Free swimmers sit at z 0.7, biofilm cells at z 0.35.
- One colour field: the `--view` heatmap block-averaged to `--record-field`
  columns (rows follow the aspect); a coarse cell that is mostly rock takes
  the grain colour. The same mostly-rock cells are the static `grains` cubes.
- Stats per sample: population per species, biofilm cells, births, deaths and
  the nutrient, waste and oxygen totals.
- Replay y is the chamber's row flipped (`height - y`), so the air interface
  (row 0) is at the far edge, as in the window; z is up.
- The state hash of each sample is `MicroEnvironment::stateHash()` at that
  tick, and the metadata names it.

The recorder takes the simulation `const` and is only called after a tick, so
it cannot change a run: `test_replay.cpp` checks the state hash tick by tick
with and without a recorder, and CI compares the final hash of a full headless
run with and without `--record`, then validates the replay with
`usd-recorder replay validate` and converts it to `.usdc` (uploaded as an
artifact). The field colouring (`field_view.*`) moved out of `scene.*` so the
recorder and its tests need no GL.

## Determinism

The same seed and tick count give a bit-identical state:

- every random number comes from one `strata::sim::Rng` (PCG32) seeded from
  the config; there is no `rand()`, no `std::random_device` and no standard
  distribution;
- the simulation never reads a clock. The window build measures real time
  only to decide how many fixed ticks to run per frame
  (`strata::sim::FixedStep`); a tick's result never depends on it;
- entities are visited in the registry's fixed slot order, cells in row
  order; births are applied after the pass;
- the simulation is compiled with `-ffp-contract=off` and uses only
  `+ - * /` and `sqrt` on floats.

`MicroEnvironment::stateHash()` fingerprints the tick, RNG state, all three
fields and every entity; the tests check it tick by tick.

## The opt-in build

`strata/` is a separate CMake project (CMake ≥ 3.24) next to the Makefile:

```sh
cmake -S strata -B build-strata -DCMAKE_BUILD_TYPE=Release
cmake --build build-strata -j4
ctest --test-dir build-strata --output-on-failure
```

It fetches Strata at the commit pinned in `strata/strata.ref` (bump it
deliberately) and builds:

- `mb_strata`: the window app, `--headless` frames/screenshots and `--bench`;
- `mb_strata_tests`: Catch2 tests for the simulation (no GPU);
- `mb_strata_render_tests`: a headless render smoke test (EGL + a GL 4.5
  driver; CI uses Mesa llvmpipe).

`make strata` wraps those commands. `make`/`make all`, the Dockerfile and the
existing CI jobs never touch `strata/`, so a machine without CMake 3.24 or
SDL3's build dependencies builds and tests everything else exactly as before.
`.github/workflows/strata.yml` builds and tests the Strata path on every pull
request.

## In a browser

Strata draws through WebGL 2 (its OpenGL ES 3.0 device) when built with
Emscripten, so the same `strata/` project builds the arcade page:

```sh
source /path/to/emsdk/emsdk_env.sh      # emsdk 6.0.11, as CI and Strata use
emcmake cmake -S strata -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j4
ctest --test-dir build-web --output-on-failure   # the Catch2 tests, under Node
mkdir -p site && cp build-web/mb_strata.html site/index.html && cp build-web/mb_strata.{js,wasm} site/
python3 -m http.server -d site 8000
```

- `strata/web/shell.html` is the page. The browser runs the frame loop: in
  `app/main.cpp` all window state lives in one `App`, and `main()` schedules
  `App::frame` with `emscripten_set_main_loop_arg` and returns. Natively the
  same `App::frame` runs in a `while` loop.
- Mouse and touch go to a layer over the canvas, which calls exported `mb_*`
  functions (pan, zoom, rotate, field, pause, speed, overlay, new chamber):
  one finger or the mouse drags, the wheel or a pinch zooms, a two-finger
  twist rotates. The keys are those of the window (SDL3 reads them on the
  canvas). Esc does nothing in a page.
- `?seed=N` picks the seed, `?cascades=N` and `?shadow=N` lower the sun's
  shadows (`--cascades`, `--shadow-map`) for weak GPUs, and `?bench=N` runs
  `--bench --ticks N` and prints the state hash on the page.
- Exceptions are wasm exceptions (`-fwasm-exceptions`), the mode Strata is
  built with; the render smoke test (EGL) is not built.
- Determinism holds in WebAssembly: 2,000 ticks of seed 1 reach the state in
  `strata/test/state-seed1-2000.txt` both natively (`strata.yml`) and in
  headless Chromium (`browser.yml`, `strata/web/smoke.cjs`), and CI fails if
  either differs.
- `strata/web/smoke.cjs` (Playwright 1.61.1) also checks that the chamber is
  drawn, the clock ticks, Pause stops it, and that a drag, the wheel, a key,
  a one-finger drag and a pinch change the picture, on a desktop and an
  iPhone 13 profile. Headless Chromium draws with SwiftShader on the CPU, so
  its frame rate there (a few frames per second) says nothing about a GPU.

## Known gaps

- No mutation or horizontal gene transfer yet (RESEARCH.md §5); species are
  fixed at birth.
- The server build (`mb_webapp`) does not show the Strata micro-environment.
- In a browser the drawable follows the canvas at the full device pixel
  ratio; on a weak phone GPU `?cascades=0` is the lever.
- The tuning is hand-picked so that all three species coexist on the default
  chamber; it is not calibrated to real organisms.
