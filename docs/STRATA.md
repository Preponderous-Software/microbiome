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
| Console app `mb_app`, live viewer `mb_webapp` (microbiome.preponderous.org), browser/arcade build (`src/browser.cpp`), `env-lib-cpp`, both test suites, Docker image | **Unchanged.** They keep the existing `Microbiome`/`Environment` simulation and the Makefile. |

The browser build stays on the current path: Strata needs OpenGL 4.5 core and
SDL3/EGL, which the Emscripten/WebGL build does not provide, so
`microbiome.play.danielstephenson.dev` keeps running `src/browser.cpp`.

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

## Known gaps

- No mutation or horizontal gene transfer yet (RESEARCH.md §5); species are
  fixed at birth.
- The browser and server builds do not show the Strata micro-environment.
- No text overlay in the window; statistics go to stdout.
- The tuning is hand-picked so that all three species coexist on the default
  chamber; it is not calibrated to real organisms.
