#!/usr/bin/env bash
# Builds the browser version into web/build/ (index.html, index.js, index.wasm).
# Needs an activated Emscripten SDK (em++ on PATH). Serve web/build/ with any
# static file server, e.g. `python3 -m http.server -d web/build 8000`.
set -euo pipefail
cd "$(dirname "$0")/.."

PROJECT_FILES="src/microorganism.cpp src/microbiome.cpp src/appConfig.cpp src/logger.cpp src/microorganismFactory.cpp src/biomatter.cpp"
ENVLIBCPP_FILES="env-lib-cpp/src/entity.cpp env-lib-cpp/src/environment.cpp env-lib-cpp/src/grid.cpp env-lib-cpp/src/location.cpp"

mkdir -p web/build
# -fexceptions: env-lib-cpp signals "no location at these coordinates" by
# throwing, and Microbiome catches it at the grid edge, so exception catching
# must stay on (Emscripten disables it by default).
em++ -O2 -std=c++17 -fexceptions \
    src/browser.cpp $PROJECT_FILES $ENVLIBCPP_FILES \
    -sALLOW_MEMORY_GROWTH=1 \
    -sEXPORTED_FUNCTIONS=_main,_mb_restart \
    -sEXPORTED_RUNTIME_METHODS=ccall \
    --shell-file web/shell.html \
    -o web/build/index.html
echo "Built web/build/index.html"
