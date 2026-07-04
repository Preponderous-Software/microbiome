#include "header/webServer.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <jansson.h>
extern "C" {
#include <ulfius.h>
}

namespace {
    const char* INDEX_HTML = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Microbiome</title>
<style>
  :root {
    color-scheme: dark;
    --bg: #0b1210;
    --panel: #111a17;
    --text: #d8e6df;
    --muted: #7fa08f;
    --grid: #1c2b26;
    --alive: #4fd67a;
    --subsisting: #4fa9d6;
    --dying: #e05252;
    --biomatter: #b58a4a;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1rem;
    padding: 1.5rem 1rem 3rem;
  }
  h1 { margin: 0; font-size: 1.25rem; font-weight: 600; }
  .subtitle { margin: 0; color: var(--muted); font-size: 0.85rem; }
  .stats {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem 1.5rem;
    background: var(--panel);
    border-radius: 0.75rem;
    padding: 0.75rem 1.25rem;
    font-size: 0.85rem;
  }
  .stats span strong { color: var(--text); }
  .stats span { color: var(--muted); }
  canvas {
    background: var(--grid);
    border-radius: 0.75rem;
    max-width: 100%;
    height: auto;
  }
  .legend {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
    font-size: 0.8rem;
    color: var(--muted);
  }
  .legend span { display: inline-flex; align-items: center; gap: 0.4rem; }
  .dot { width: 0.6rem; height: 0.6rem; border-radius: 50%; display: inline-block; }
</style>
</head>
<body>
  <h1>Microbiome</h1>
  <p class="subtitle">a virtual microbial community, viewed live</p>
  <div class="stats" id="stats"></div>
  <canvas id="grid" width="600" height="600"></canvas>
  <div class="legend">
    <span><i class="dot" style="background:var(--alive)"></i>alive</span>
    <span><i class="dot" style="background:var(--subsisting)"></i>foraging</span>
    <span><i class="dot" style="background:var(--dying)"></i>low energy</span>
    <span><i class="dot" style="background:var(--biomatter)"></i>biomatter</span>
  </div>

<script>
  const canvas = document.getElementById("grid");
  const ctx = canvas.getContext("2d");
  const statsEl = document.getElementById("stats");
  const style = getComputedStyle(document.documentElement);

  function color(name) {
    return style.getPropertyValue(name).trim();
  }

  function render(state) {
    const cell = canvas.width / state.gridSize;
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    for (const b of state.biomatter) {
      ctx.fillStyle = color("--biomatter");
      const size = cell * 0.35;
      ctx.fillRect(b.x * cell + (cell - size) / 2, b.y * cell + (cell - size) / 2, size, size);
    }

    for (const m of state.microorganisms) {
      let fill = color("--alive");
      if (m.energy < 10) {
        fill = color("--dying");
      } else if (m.timesEaten > 0) {
        fill = color("--subsisting");
      }
      ctx.fillStyle = fill;
      ctx.beginPath();
      ctx.arc(m.x * cell + cell / 2, m.y * cell + cell / 2, cell * 0.3, 0, Math.PI * 2);
      ctx.fill();
    }

    statsEl.innerHTML = `
      <span>generation <strong>${state.generation}</strong></span>
      <span>tick <strong>${state.tick}</strong></span>
      <span>alive <strong>${state.aliveCount}</strong></span>
      <span>total deaths <strong>${state.deadCount}</strong></span>
      <span>total energy <strong>${state.totalEnergy}</strong></span>
    `;
  }

  async function poll() {
    try {
      const response = await fetch("/api/state");
      const state = await response.json();
      render(state);
    } catch (err) {
      // server may be between ticks or restarting a generation; try again shortly
    } finally {
      setTimeout(poll, 300);
    }
  }

  poll();
</script>
</body>
</html>
)HTML";

    int handleIndexRequest(const struct _u_request*, struct _u_response* response, void* userData) {
        static_cast<WebServer*>(userData)->writeIndexResponse(response);
        return U_CALLBACK_CONTINUE;
    }

    int handleStateRequest(const struct _u_request*, struct _u_response* response, void* userData) {
        static_cast<WebServer*>(userData)->writeStateResponse(response);
        return U_CALLBACK_CONTINUE;
    }
}

WebServer::WebServer(AppConfig* appConfig, int listenPort) {
    config = appConfig;
    port = listenPort;
    instance = new _u_instance();

    std::lock_guard<std::mutex> lock(stateMutex);
    microbiome = new Microbiome(generation++, "Web Simulation", config->getEnvironmentSize(), config->getEntityFactor());
}

WebServer::~WebServer() {
    stop();
    if (simulationThread.joinable()) {
        simulationThread.join();
    }
    delete instance;
    delete microbiome;
}

void WebServer::run() {
    if (ulfius_init_instance(instance, port, NULL, NULL) != U_OK) {
        throw std::runtime_error("Failed to initialize web server instance on port " + std::to_string(port));
    }

    ulfius_add_endpoint_by_val(instance, "GET", "/", NULL, 0, &handleIndexRequest, this);
    ulfius_add_endpoint_by_val(instance, "GET", "/api/state", NULL, 0, &handleStateRequest, this);

    keepRunning = true;
    simulationThread = std::thread(&WebServer::runSimulationLoop, this);

    if (ulfius_start_framework(instance) != U_OK) {
        keepRunning = false;
        simulationThread.join();
        throw std::runtime_error("Failed to start web server on port " + std::to_string(port));
    }

    std::cout << "Serving microbiome simulation on http://localhost:" << port << std::endl;

    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    ulfius_stop_framework(instance);
    ulfius_clean_instance(instance);
}

void WebServer::stop() {
    keepRunning = false;
}

void WebServer::runSimulationLoop() {
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(tickIntervalMs));

        std::lock_guard<std::mutex> lock(stateMutex);
        if (microbiome->getNumAliveMicroorganisms() == 0) {
            delete microbiome;
            tickCount = 0;
            microbiome = new Microbiome(generation++, "Web Simulation", config->getEnvironmentSize(), config->getEntityFactor());
            continue;
        }

        microbiome->initiateMicroorganismMovement();
        tickCount++;
    }
}

void WebServer::writeStateResponse(_u_response* response) {
    std::lock_guard<std::mutex> lock(stateMutex);

    json_t* root = json_object();
    json_object_set_new(root, "generation", json_integer(generation));
    json_object_set_new(root, "tick", json_integer(tickCount));
    json_object_set_new(root, "gridSize", json_integer(microbiome->getGrid()->getSize()));
    json_object_set_new(root, "aliveCount", json_integer(microbiome->getNumAliveMicroorganisms()));
    json_object_set_new(root, "deadCount", json_integer(microbiome->getNumDeadMicroorganisms()));
    json_object_set_new(root, "totalEnergy", json_integer(microbiome->getTotalEnergy()));

    json_t* organisms = json_array();
    for (Microorganism* organism : microbiome->getMicroorganisms()) {
        Location& location = microbiome->getGrid()->getLocation(organism->getLocationId());
        json_t* entry = json_object();
        json_object_set_new(entry, "id", json_integer(organism->getId()));
        json_object_set_new(entry, "x", json_integer(location.getX()));
        json_object_set_new(entry, "y", json_integer(location.getY()));
        json_object_set_new(entry, "energy", json_integer(organism->getEnergy()));
        json_object_set_new(entry, "timesEaten", json_integer(organism->getTimesEaten()));
        json_array_append_new(organisms, entry);
    }
    json_object_set_new(root, "microorganisms", organisms);

    json_t* biomatterEntries = json_array();
    for (Biomatter* b : microbiome->getBiomatter()) {
        Location& location = microbiome->getGrid()->getLocation(b->getLocationId());
        json_t* entry = json_object();
        json_object_set_new(entry, "id", json_integer(b->getId()));
        json_object_set_new(entry, "x", json_integer(location.getX()));
        json_object_set_new(entry, "y", json_integer(location.getY()));
        json_object_set_new(entry, "energy", json_integer(b->getEnergy()));
        json_array_append_new(biomatterEntries, entry);
    }
    json_object_set_new(root, "biomatter", biomatterEntries);

    ulfius_set_json_body_response(response, 200, root);
    json_decref(root);
}

void WebServer::writeIndexResponse(_u_response* response) {
    u_map_put(response->map_header, "Content-Type", "text/html; charset=utf-8");
    ulfius_set_string_body_response(response, 200, INDEX_HTML);
}
