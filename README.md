# microbiome
This application allows the user to witness the activity of a virtual microbial community.

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
- `make catch2_tests` - Build and use Catch2 framework tests (recommended)
- `make tests` - Build legacy assert-based tests (for backward compatibility)
- `make test` - Alias for `catch2_tests`
- `./run_tests.sh` - Run both Catch2 and legacy tests

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

Building locally (in addition to `make g++`, this needs `libulfius-dev` and `pkg-config`, e.g. `apt-get install pkg-config libulfius-dev` on Debian/Ubuntu):
1. Run `make webapp`
2. Run `./mb_webapp` (set `MICROBIOME_WEB_PORT` to use a port other than the default 8080)
3. Open http://localhost:8080 in a browser

The page polls `GET /api/state` a few times a second for the current grid, microorganisms, and biomatter as JSON, and once the population goes extinct the server starts a new generation automatically.

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

## 📄 License

This project is licensed under the **Preponderous Non-Commercial License (Preponderous-NC)**.  
It is free to use, modify, and self-host for **non-commercial** purposes, but **commercial use requires a separate license**.

> **Disclaimer:** *Preponderous Software is not a legal entity.*  
> All rights to works published under this license are reserved by the copyright holder, **Daniel McCoy Stephenson**.

Full license text:  
[https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md](https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md)
