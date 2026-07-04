# Research: Making the Microbiome Simulation More Realistic

This document surveys real microbiology and microbial-ecology concepts that are
missing from the current simulation and proposes concrete mechanics for
closing the gap. It is meant as a reference for future feature work (including
[#13](https://github.com/Preponderous-Software/microbiome/issues/13) and
[#14](https://github.com/Preponderous-Software/microbiome/issues/14)), not a
committed roadmap — each proposal below should still go through its own
design/PR.

## 1. What the simulation currently models

Reading through `src/`, the present model is intentionally minimal:

- A single, undifferentiated `Microorganism` type (`src/header/microorganism.h`)
  with three traits: `energy`, `metabolicRate`, and derived `isDead()`.
- Every organism starts with a random energy value and a random fixed
  metabolic rate; there is no genotype, species, or trait variation beyond
  those two numbers.
- Each tick, every living organism takes one random walk step on the grid and
  loses `metabolicRate` energy (`Microbiome::initiateMicroorganismMovement`,
  `src/microbiome.cpp:29`). There is no directional bias — movement is
  uniform random regardless of what's nearby.
- Death is just `energy <= 0`. Dead organisms remain on the grid as inert
  entities.
- The only interaction between organisms is opportunistic necrophagy: a
  living organism sharing a grid cell with a dead one consumes it for
  `metabolicRate * 2` energy (`Microbiome::initiateConsumptionOfDeadMicroorganisms`).
  There is no other predation, cooperation, or signaling.
- There is no reproduction, no mutation, no nutrient field independent of the
  organisms themselves, and no environmental variables (temperature, pH,
  oxygen, etc.).

This is a reasonable "hello world" for an agent-based model, but it doesn't
yet resemble a microbial community — it's closer to a population of
identical particles with a decay timer. The sections below map real biology
onto mechanics that would move it closer to an actual microbiome.

## 2. Population dynamics

### 2.1 Reproduction ([#13](https://github.com/Preponderous-Software/microbiome/issues/13))
Real bacteria reproduce by binary fission: a cell that has accumulated enough
biomass splits into two, each inheriting (approximately) half the parent's
resources. This is the single biggest missing mechanic — without it, the
population is monotonically decaying and the simulation is really just
modeling die-off, not a living community.

Suggested mechanic: when `energy` crosses a `reproductionThreshold`, split
the organism into two, each getting `energy / 2` (minus a fission cost to
account for the real energetic cost of replicating DNA/membrane), placed in
adjacent free cells if available. This turns the system from pure decay into
actual population dynamics (growth, carrying capacity, boom/bust cycles),
and is a prerequisite for almost everything else in this document — mutation,
selection, and evolution all require organisms that reproduce.

### 2.2 Death → biomatter → nutrient cycling ([#14](https://github.com/Preponderous-Software/microbiome/issues/14))
Currently a dead organism is just an inert corpse worth a fixed energy bonus
to whichever neighbor happens to eat it, and organisms that are never
scavenged simply vanish from the energy budget (`getTotalEnergy` correctly
notes energy can go negative and clamps it, which is itself a symptom of
energy leaving the system with nowhere to go). Real decomposition recycles
nutrients: dead biomass is broken down by decomposers/extracellular enzymes
and its constituent nutrients (carbon, nitrogen, phosphorus) re-enter the
shared pool for other organisms to take up. Modeling death as "spawn a
biomatter/detritus resource in the environment" (per #14) rather than "spawn
a corpse with a fixed bonus" keeps the system's total energy conserved and
opens the door to a real nutrient cycle instead of a one-off scavenging
bonus.

### 2.3 Carrying capacity and logistic growth
With reproduction added, population growth needs a limiting factor or it
diverges. In real microbial cultures this comes from finite substrate
(Monod, 1949: growth rate is a saturating function of limiting-nutrient
concentration, μ = μmax·S/(Ks+S)) and finite space. Concretely: bound
reproduction on local nutrient availability and grid occupancy, so that
population growth naturally follows a logistic curve (fast growth while
nutrients are abundant, plateauing as they're depleted) rather than
unbounded exponential growth or a hard organism-count cap.

### 2.4 Multi-species dynamics
A "microbiome" is by definition a multi-species community — human/soil/gut
microbiomes host hundreds to thousands of taxa. The current `Microorganism`
class has no species concept at all. Introducing 2+ species with different
trait profiles (metabolic rate, motility, diet) enables classic ecological
dynamics: competitive exclusion when two species share a niche too closely,
coexistence when niches differentiate, and predator/prey-style
Lotka-Volterra oscillations if one species preys on another (see §4).

## 3. Movement and sensing

### 3.1 Chemotaxis
Real bacteria (e.g., *E. coli*) don't move randomly — they bias a
run-and-tumble random walk up nutrient gradients and away from repellents,
via receptor proteins and the Che signaling pathway. The simulation's
`moveEntityToRandomAdjacentLocation` call is uniform random regardless of
what's around the organism. A cheap first step toward realism: bias movement
probability toward adjacent cells with more available nutrient/biomatter
(§2.2) or more nearby dead organisms, and away from cells that are
overcrowded. This alone would produce visibly more organic-looking clustering
around food sources instead of the current uniform scatter.

### 3.2 Motility variation
Not all microbes swim — many are non-motile and rely on diffusion/passive
transport or biofilm growth instead. Giving some species zero or reduced
movement probability (rather than assuming every organism relocates every
tick) is both more realistic and a cheap way to differentiate species
behaviorally without new mechanics.

## 4. Interactions between organisms

### 4.1 Predation beyond necrophagy
The only current inter-organism interaction is eating something already
dead. Real microbial predation exists too: predatory bacteria like
*Bdellovibrio* invade and consume other living bacteria, protists graze on
bacterial populations, and bacteriophages (viruses) infect and lyse
bacterial cells, sometimes carrying genes between hosts (transduction, see
§5.2). Any of these would introduce a genuine predator/prey loop instead of
pure scavenging, and are the natural extension once multi-species dynamics
(§2.4) exist.

### 4.2 Cross-feeding / syntrophy
In real communities, one species' metabolic waste is frequently another's
substrate — classic syntrophy, seen for example in anaerobic digestion
consortia or gut cross-feeding between primary fermenters and
butyrate-producers. If §2.2's nutrient/biomatter pool is typed (e.g., raw
biomatter vs. metabolic byproducts) rather than a single fungible energy
number, different species could be specialized consumers of different pool
types, producing emergent food webs instead of one-shot scavenging.

### 4.3 Quorum sensing and biofilms
Many bacteria coordinate behavior based on local population density via
diffusible signaling molecules (autoinducers) — a mechanism called quorum
sensing, used to trigger collective behaviors like biofilm formation,
bioluminescence, or virulence-factor production once a local population
crosses a density threshold. A simple analog: once local density in a
neighborhood exceeds a threshold, switch organisms into a "biofilm" state
with reduced motility, reduced individual metabolic rate (shared/matrix
protection), and increased local survival — a meaningfully different
emergent structure from the current uniform random scatter.

## 5. Genetics and evolution

### 5.1 Mutation and trait inheritance
Once reproduction (§2.1) exists, giving offspring small random mutations to
inherited traits (metabolic rate, motility, nutrient preference) turns the
simulation into an actual evolutionary process: traits that improve survival
and reproduction in the current environment will become more common over
generations, for free, via selection — no explicit fitness function needed
beyond "survive and reproduce more."

### 5.2 Horizontal gene transfer
Bacteria don't only inherit traits vertically from a parent cell — they also
swap genetic material directly via conjugation (direct cell-to-cell
transfer, often plasmid-borne), transformation (uptake of free DNA from the
environment, e.g., from lysed cells), and transduction (phage-mediated).
This is the real-world mechanism behind rapid spread of traits like
antibiotic resistance across a population in far fewer generations than
mutation alone would allow. A simplified version — e.g., a small per-tick
probability that two co-located organisms exchange a trait value — would let
traits spread independently of the reproduction tree, which vertical-only
inheritance can't capture.

### 5.3 Dormancy and stress response
Under starvation or other stress, many bacteria don't just die — they enter
reversible dormant states (e.g., sporulation in *Bacillus*/*Clostridium*, or
the "viable but non-culturable" state) with near-zero metabolism, and can
revive when conditions improve. A `dormant` state that pauses metabolism
below some energy/nutrient threshold (instead of marching straight to death)
would make population crashes recoverable rather than terminal, which is
closer to how real cultures behave under fluctuating nutrient availability.

## 6. Environmental heterogeneity

The environment today is spatially uniform — every grid cell is
interchangeable except for what's currently sitting on it. Real microbial
habitats have structure: oxygen gradients (creating aerobic surface layers
and anaerobic pockets, relevant to obligate aerobes vs. anaerobes vs.
facultative organisms), pH and temperature gradients, and moisture
gradients, all of which affect which organisms can survive where. Even a
coarse per-cell scalar field or two (e.g., oxygen level, nutrient level)
that diffuses over time and that organism survival/metabolism depends on
would let spatial structure emerge (distinct zones favoring different
species) instead of the current spatially-homogeneous grid.

## 7. Suggested near-term ordering

Not a commitment, but a sensible dependency order given the above — several
later items are much more interesting once the earlier ones exist:

1. **Reproduction** (#13) — nothing else about population/evolutionary
   dynamics is meaningful without it.
2. **Biomatter on death** (#14) — closes the energy budget and creates a real
   nutrient pool to build chemotaxis and cross-feeding on top of.
3. **Chemotaxis toward nutrient/biomatter** — cheap, high visual/behavioral
   payoff once there's something in the environment worth moving toward.
4. **Species differentiation** (trait profiles, then divergent metabolism) —
   turns "a microorganism" into "a microbiome."
5. **Mutation on reproduction** — evolution falls out of 1 and 4 almost for
   free.
6. Everything else in this document (quorum sensing, HGT, dormancy,
   environmental gradients, predation) builds on top of 1–5.

## 8. Selected references

- Monod, J. (1949). *The Growth of Bacterial Cultures.* Annual Review of
  Microbiology. (Source of the Monod equation referenced in §2.3.)
- Lotka, A. J. (1925) / Volterra, V. (1926). Predator-prey population
  dynamics, referenced in §2.4/§4.1.
- Miller, M. B., & Bassler, B. L. (2001). *Quorum Sensing in Bacteria.*
  Annual Review of Microbiology. (§4.3)
- Thomas, C. M., & Nielsen, K. M. (2005). *Mechanisms of, and Barriers to,
  Horizontal Gene Transfer between Bacteria.* Nature Reviews Microbiology.
  (§5.2)
- Lennon, J. T., & Jones, S. E. (2011). *Microbial seed banks: the ecological
  and evolutionary implications of dormancy.* Nature Reviews Microbiology.
  (§5.3)
