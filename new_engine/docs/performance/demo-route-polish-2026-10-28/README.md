# Jungle and castle–tavern demo polish — 28 October 2026

Implemented and tested in the existing custom engine, using owned assets. This pass is ready for a walking-demo review; it does not establish finished AAA fidelity or a locked every-frame 45 FPS floor.

## Changes

- Eight compact, smooth terrain banks fitted around 1,416 existing asset footprints. The three-lane walking route, pool support and campsite core remain unchanged.
- 200 additional foliage, root/stone anchors and street-detail placements; 7,758 authored instances overall. No new model aliases or source images. Existing eight scenery instance files preserved.
- Startup-only canopy shelter field from 129 actual crowns: about 1 MiB of data in the existing texture array, no extra sampler or shadow pass. Dark, cloudy jungle atmosphere, fog and rain retained.
- Seven actual tavern windows contribute a bounded diffuse sky return alongside the existing warm practical lighting.
- Camp craftsman gait follows measured walking stride. Bartender turns ease toward the serving counter and back. Existing residents, six shield guards, four campers and two walking goats retained.

## Native acceptance

RX 5300 (3 GB), 1600×900 output, fixed 1360×765 scene, manual 60 FPS target. 2K shadows, half-resolution reflections, contact shadows, soft shadows, volumetric clouds/fog and rain retained. Firefox remained open/minimized. No builds/cooks/hash scans during timed rendering.

| Test | Average FPS | Slowest 1% FPS | Worst frame ms | Frames below 45 |
|---|---:|---:|---:|---:|
| final-jungle / route | 55.89 | 45.76 | 23.405 | 6 |
| final-jungle-repeat / route | 55.89 | 45.66 | 25.831 | 8 |
| final-camp / clearing | 59.67 | 54.26 | 18.781 | 0 |
| final-camp / approach-return | 58.51 | 48.47 | 21.210 | 0 |
| final-castle / castle-west-street | 59.95 | 54.27 | 20.020 | 0 |
| final-castle / castle-finished-arcade | 59.92 | 53.49 | 19.840 | 0 |
| final-castle / castle-tavern-interior | 59.95 | 52.54 | 22.376 | 1 |

All intervals after exactly 120 warmup frames are retained and independently recomputed from complete CSVs. Average and slowest-1% targets passed; rare individual frames still exceed 22.22 ms. Do not interpret the percentile gate as an every-frame floor.

## Verification

CPU terrain, canopy, route clearance, original population motion golden checks, continuous NPC transitions and actual pose rendering passed. Native AMD shader tests passed, including full terrain and foliage fragments in both scalar and gather variants. Canonical source/geometry validation passed without relaxing placement or motion thresholds. The final nine-stop integration crossed both lighting-cache regions and returned, with resource cleanup, camp/wildlife safety and GL checks passing. See adjacent test logs, native results JSON/CSVs and visual-review-v2 captures.

## Demo controls

WASD to move; right mouse to look. F9 advances the nine showcase stops through village and jungle; F10 opens graphics settings. The interactive demo starts at the jungle understory. The previous exported jungle movie predates this polish pass.
