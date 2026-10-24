Microbiome (Strata micro-environment)
=====================================

A Winogradsky-like chamber of soil grains, nutrient seeps and oxygen from
above, where aerobes, fermenters and cross-feeders compete, simulated and
drawn with the Strata engine. The simulation is deterministic: the same seed
gives the same chamber on every machine.

Run
---

Linux:    ./mb_strata   (needs OpenGL 4.5, which any current Mesa or vendor
          driver has, and glibc 2.34 or newer: Ubuntu 22.04, Debian 12,
          Fedora 35 and later)

  mb_strata --seed 7 --size 192            another chamber, larger
  mb_strata --view oxygen                  colour the field by oxygen
  mb_strata --headless --ticks 600 --screenshot out.png
                                           one picture, no window
  mb_strata --bench --ticks 2000           ticks per second, and the state hash
  mb_strata --record run.usda              record a usd-replay/1 replay

Tab toggles the statistics overlay, R starts a new chamber, H shows help
and Esc quits. mb_strata reports no usage and writes no log files.

Only a Linux build is made: the Strata build of Microbiome is built and
tested on Linux only.

More: https://github.com/Preponderous-Software/microbiome (docs/STRATA.md)

Microbiome is under the Preponderous Non-Commercial License (LICENSE.txt).
It is built on the Strata engine; THIRD-PARTY.md lists what that links in,
and their licences.
