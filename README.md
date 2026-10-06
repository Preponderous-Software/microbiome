# microbiome

[![Play in your browser](https://img.shields.io/badge/Play-in%20your%20browser-2ea44f)](https://danielstephenson.dev/play/microbiome)

This application allows the user to witness the activity of a virtual microbial community.

## Play in your browser
The simulation also runs entirely in your browser (compiled to WebAssembly with Emscripten), no server needed:

- https://microbiome.play.danielstephenson.dev
- or find it among the other games at https://danielstephenson.dev/play

This is in addition to the existing server-backed live viewer at https://microbiome.preponderous.org, which stays as it is.

To build it yourself, activate an [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (CI uses 6.0.10) and run `make web` (or `web/build.sh`). It writes `web/build/index.html`, `index.js` and `index.wasm`; serve that directory with any static file server, e.g. `python3 -m http.server -d web/build 8000`, and open http://localhost:8000. The entry point is `src/browser.cpp` (the page is `web/shell.html`): it runs the same `Microbiome` the console app and web server run, one tick every 200 ms, starting a new generation after an extinction just like `mb_webapp`. It reports no usage, writes no log files, and the native targets never compile it. `.github/workflows/browser.yml` builds it on every pull request and push.

## Example Console Output
```
==============================
    .        o     o  o       
          .        +     .    
 +  o           +  +        . 
       o        +  +  o  o    
    +           o  .  +  o  . 
    o  !  +  !  o             
 +  +        +              o 
          +     +  .  +  +    
 o  +        .  !     !       
       !  o  .     .        o 
==============================

Name: Simulation 1
Size: 10x10
Microorganisms: 45
Dead Microorganisms: 55
Total Energy: 1858

Ticks elapsed: 16 of 120
```

## Testing
The project now uses **Catch2** as the primary testing framework, providing better test organization and reporting.

### How to run the tests in the dev container
1. Install Docker
2. Clone the repository
3. Open the project in VSCode.
4. Install the Remote-Containers extension.
5. Click on the green button in the bottom left corner of the window.
6. Select "Reopen in Container".
7. Open a terminal in VSCode.
8. Run the `./run_tests.sh` script or build the tests with `make catch2_tests` and run them with `./mb_tests`.

### Available Test Commands
- `make catch2_tests` - Build the Catch2 framework tests as `./mb_tests` (recommended)
- `make tests` - Build the legacy assert-based tests as `./tests` (for backward compatibility)
- `make test` - Alias for `catch2_tests`
- `./run_tests.sh` - Run both Catch2 and legacy tests
- `make` - Builds both test binaries alongside the applications, so a test suite that stops compiling fails the build rather than going unnoticed

CI (`.github/workflows/docker-build.yml`) builds the production image and then executes both
suites inside it, so a failing assertion fails the build too, not only a failing compile.

Every build target lists the sources and headers it is built from, so changing any of them
rebuilds the affected binaries. `make tests` in particular used to consider a `tests` binary
up to date whenever it was newer than `src/tests.cpp` alone, and could re-run a stale build
after a change elsewhere.

The two suites overlap, with one gap: the web server's HTTP tests (`GET /api/state`, the
index page's footer link and share tags, and `GET /og.png`) currently live only in the legacy suite, because
`make catch2_tests` deliberately links no Ulfius dependency. Run `./run_tests.sh` to cover both.

### Catch2 Features
- Better test reporting and failure diagnostics
- Test filtering by tags: `./mb_tests [microorganism]`
- Verbose output: `./mb_tests --success`
- List all tests: `./mb_tests --list-tests`

## Running the application
### How to run the application with Docker-Compose
1. Install Docker and Docker-Compose
2. Clone the repository
3. Open a terminal in the root directory of the repository
4. Run the following command: `docker-compose up --build --remove-orphans`

### How to run the application in the dev container
1. Install Docker
2. Clone the repository
3. Open the project in VSCode.
4. Install the Remote-Containers extension.
5. Click on the green button in the bottom left corner of the window.
6. Select "Reopen in Container".
7. Open a terminal in VSCode.
8. Run the `./cr.sh` script or build the project with `make` and run it with `./mb_app`.

### How to view the simulation live in a browser
The `webapp` target builds a small [Ulfius](https://github.com/babelouest/ulfius)-based web server (`mb_webapp`) that runs the simulation continuously and serves a live view of it.

With Docker Compose:
1. Run `docker-compose up --build microbiome-webapp`
2. Open http://localhost:8080 in a browser

Building locally (in addition to `make` and `g++`, this needs `libulfius-dev` and `pkg-config`, e.g. `apt-get install pkg-config libulfius-dev` on Debian/Ubuntu):
1. Run `make webapp`
2. Run `./mb_webapp` (set `MICROBIOME_WEB_PORT` to use a port other than the default 8080)
3. Open http://localhost:8080 in a browser

The page polls `GET /api/state` a few times a second for the current grid, microorganisms, and biomatter as JSON, and once the population goes extinct the server starts a new generation automatically.

The page carries a description, a canonical link and Open Graph / Twitter card tags for https://microbiome.preponderous.org. Its link-preview image (`og:image`) is `assets/og.png`, a 1200×630 card served at `GET /og.png`. The PNG is compiled into the server as `src/header/ogImage.h`, so the binary needs no files beside it. After redrawing the PNG, run `make og-image-header` (needs `xxd`) and commit both. The legacy suite fails if they differ.

## Notable Classes

### Microbiome
The Microbiome class represents a virtual microbial community. It is an extension of the Environment class provided by env-lib-cpp. Within the microbiome, there are a number of microbes that are able to interact with each other and the environment. The Microbiome class is responsible for managing the microbes and the environment they exist in.

### Microorganism
The Microorganism class represents a single microbe. It is an extension of the Entity class provided by env-lib-cpp. The Microorganism class is responsible for managing the microbe's energy and metabolism.

### Biomatter
The Biomatter class represents decomposing biomass left behind when a microorganism dies. It is an extension of the Entity class provided by env-lib-cpp. Living microorganisms bias their movement toward it (chemotaxis) and can forage it for energy, modeling nutrient recycling instead of dead microorganisms simply vanishing from the energy budget.

### WebServer
The WebServer class (built on [Ulfius](https://github.com/babelouest/ulfius), see [#19](https://github.com/Preponderous-Software/microbiome/issues/19)) runs a Microbiome simulation continuously in the background and exposes its state over HTTP, so it can be viewed live at http://localhost:8080 instead of only in the console.

## Simulated Mechanics

See [RESEARCH.md](RESEARCH.md) for the microbiology background behind these mechanics and what's still missing.

- **Metabolism** - every microorganism loses energy each tick and dies once its energy reaches zero.
- **Decomposition** - a dead microorganism's biomass becomes Biomatter in the environment rather than an inert corpse.
- **Chemotaxis** - microorganisms bias their movement toward neighboring locations that have Biomatter, rather than moving with pure uniform randomness.
- **Foraging** - a microorganism sharing a location with Biomatter can consume it for energy, depleting it over time.
- **Reproduction** - a microorganism that accumulates enough energy divides via binary fission into two daughter cells, each inheriting half its remaining energy (minus the energetic cost of dividing) and its metabolic rate.

## Usage reporting
The console app (`mb_app`) reports to [trace](https://trace.danielstephenson.dev) by default: one `startup` event per launch, carrying the program name (`microbiome`), its version from `version.txt`, and a random installation ID. Nothing about you, your machine or the simulation is sent (the trace server sees the IP address of the request, as every web server does). The live-view web server (`mb_webapp`) and the test suites report nothing.

The first run prints one line saying so on stderr and writes a small settings file, `usage-reporting.conf`, to `$XDG_CONFIG_HOME/microbiome/` (by default `~/.config/microbiome/`; `~/Library/Application Support/microbiome/` on macOS, `%APPDATA%\microbiome\` on Windows). To turn reporting off:

- set `enabled=false` in that file, or
- set `TRACE_USAGE_REPORTING=off` or `DO_NOT_TRACK=1` in the environment (this turns it off for every trace-reporting program, and nothing is printed or written).

The installation ID is a random UUID, made the first time reporting is on and kept in `trace-install-id` in `$XDG_DATA_HOME/microbiome/` (by default `~/.local/share/microbiome/`; `~/Library/Application Support/microbiome/` on macOS, `%APPDATA%\microbiome\` on Windows). It is not derived from anything about you or your machine; it only lets trace count installations rather than launches. Delete the file to reset it. Setting `TRACE_INSTALL_ID` sends that value instead, and the file is left alone. Every opt-out above also stops the ID: with reporting off, the file is never created, read or sent.

The event is sent in the background by the vendored [trace-client-cpp](https://github.com/Stephenson-Software/trace-client-cpp) header (`src/header/trace_client.hpp`) through the system `curl`; if curl is missing or the machine is offline, nothing is sent and the simulation is unaffected. `MICROBIOME_USAGE_REPORTING_ENDPOINT` points it at another server, e.g. a local one while testing. Details: https://github.com/Stephenson-Software/trace#usage-reporting

## 📄 License

This project is licensed under the **Preponderous Non-Commercial License (Preponderous-NC)**.  
It is free to use, modify, and self-host for **non-commercial** purposes, but **commercial use requires a separate license**.

> **Disclaimer:** *Preponderous Software is not a legal entity.*  
> All rights to works published under this license are reserved by the copyright holder, **Daniel McCoy Stephenson**.

Full license text:  
[https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md](https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md)
