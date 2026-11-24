# Source-scene dwarf placement

`projects/valwood_showcase/showcase.residents` places 21 idle dwarves on the purchased scene's actual floor geometry: six in the Temple, three in High Castle, four in Medium Castle, four in Lower Castle, and four at the Barbican. Broddi remains disabled. The king stands near the main Temple carpet center at source `(238.593349, 152.169998, 651.723299)`, with a verified overhead roof 47.39 m above the floor. No patrols are enabled.

`tools/place_nature_showcase_residents.py` reproduces the data and `resident-placement-audit.json`. Each selected floor comes from a renderer named floor, uses an upward source normal above 0.8, and has triangle area over 0.5 m². Nine support rays cover a 0.5 m radius and stay within 0.06 m of the selected floor. Independent upward rays check 80 m of source geometry; the minimum measured headroom across these 21 positions is 4.97 m. Overlapping body spheres conservatively test static surface clearance, with the narrowest positive margin about 0.09 m. Idle positions on the same level stay at least 3.2 m apart. Thin visual decals are excluded from obstacle geometry.

Mapping remains the source project's existing mapping: `worldX=strongholdX+sourceX−500`, `worldZ=strongholdZ−sourceZ+500`, `worldY=1130.8125+sourceY+0.015`. Resident data stores the resulting absolute world height so the old atlas floor cannot bury the characters.

`engine/world_stronghold.cpp` reads this optional project-root file before GPU pose loading. Missing file preserves the original 30 stations and atlas-height behavior. Existing malformed files reject the entire load without partially replacing the station vector. Input is bounded to 32 rows and 511 characters per line, validates finite coordinates/scale/phase/angles, family indices, boolean patrol values and nonzero patrol length, and skips valid Broddi-family rows. The renderer's prior fixed-capacity batches remain bounded. Future explicit patrol rows interpolate absolute Y, but no navigation route is implied by the stationary placement audit.

CPU verification compiled the real implementation through `tests/world_stronghold_residents_test.cpp` with warnings as errors and garbage collection of unused renderer sections. Seventeen checks passed, including the generated 21-row file, missing-file defaults, malformed-file atomic rejection, nonfinite/range/row-count limits, Broddi skipping, fixed height and interpolated patrol height. No GL context or GPU initialization was used.

```sh
g++ -std=c++17 -O1 -Wall -Wextra -Werror -Wno-subobject-linkage -ffunction-sections -fdata-sections -Inew_engine new_engine/tests/world_stronghold_residents_test.cpp -Wl,--gc-sections -o /tmp/valwood-stronghold-residents-test
/tmp/valwood-stronghold-residents-test /home/computer/Desktop/final_engine/new_engine/projects/valwood_showcase/showcase.residents
```

Visual framing, source-scene walking collision and animation appearance still require root's native rehearsal. This audit establishes static floor support and clearance, not a claim that every selected position has been viewed in the game.
