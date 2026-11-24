# Valwood world terrain results

Measured 23 October 2026 on the native AMD Radeon RX 5300 (3 GiB), Mesa 25.2.8, visible 1600×900 window and ordinary 60 Hz VSync. The owner's native C/C++ engine now has a separate full-map terrain project; the original Seldonne project is retained.

## What is built

- A manually traced geographic foundation based on the supplied map: separate landmasses, major rivers/estuaries, Ulasse and northwest lakes, Great Mashito ridges, Valwood, Old Tanthoon, Baldurne, the Wallows escarpment, Kerone and Crosswind coasts. The map image remains available in-game, with 15 landmark destinations.
- A **100.8 × 56.7088 km** coordinate space. Standard movement is **3.5 m/s**; the complete east–west width is an **eight-hour reference**. Native window-input verification measured 4.204 m over 1.200 s, against an expected 4.201 m. This is a scale calibration, not a measured eight-hour connected land route. The preview permits traversal at sea level; swimming, bridges, ferries and route design remain later work.
- First-person walking, right-drag look, normalized diagonal speed, slope checks independent of frame rate, jumping, preview flight and landmark travel. Double-precision global placement and camera-relative GPU coordinates preserve local geometry across the map.
- Region-aware ground materials, normal detail, near shadows, water, atmospheric distance, sky/clouds, HDR/FXAA presentation and bounded local grass/fir/fern/rock dressing. Northwest foundations follow their natural mountain elevations instead of excavating deep artificial bowls. This remains a terrain blockout with prototype visual quality.

## Native frame-rate checks

All runs use a fixed seed/lighting, one engine process and 120 excluded warm-up frames. The tour changes destination every 360 frames and sweeps the camera. The forest run continuously travels a local route through Valdoa with the panel visible; it does not use landmark loading holds. The mountain run uses the elevated inspection camera without panels.

| Measurement | All 15 landmarks | Local forest | Mountain flight |
| --- | ---: | ---: | ---: |
| Total frames | 5400 | 3600 | 1800 |
| Measured samples | 5280 | 3480 | 1680 |
| Average FPS (capped) | 59.95 | 59.95 | 59.95 |
| 1% low FPS | 59.38 | 59.74 | 59.76 |
| p95 frame (ms) | 16.705 | 16.701 | 16.701 |
| p99 frame (ms) | 16.742 | 16.715 | 16.715 |
| Worst frame (ms) | 17.369 | 16.962 | 16.855 |
| Within 45 FPS budget (%) | 100.000 | 100.000 | 100.000 |
| Over 22.222 ms | 0 | 0 | 0 |
| World GPU mean (ms) | 8.330 | 12.425 | 7.424 |

These measured workloads meet the requested 45–60 FPS range. They do not certify eight hours of play, all future content, higher resolutions or every possible camera view. The cap also prevents a claim about uncapped average FPS. Frame intervals are CPU timestamps sampled before swap, rather than physical scanout measurements. GPU values are asynchronous estimates covering this backend's world rendering **including shadows**. Other generic stage/static-asset fields are unused here; zero means uninstrumented. Terrain budgets are in the adjacent `.world.json` files.

The tour records **1,397 loading frames** across its long-distance jumps, with an observed longest hold of **111 frames / about 1.85 seconds**. During these holds the previous image remains visible and a loading panel is shown while detailed underfoot terrain prepares. Thus the tour's FPS is responsiveness through travel, including held views. The independent forest run has **zero loading frames**, establishing actual continuously rendered movement at its reported rate. Short input/atlas and lake captures are correctness checks, not extended performance evidence.

## Bounded terrain and foliage

The terrain streamer keeps 252 desired records and 198 target leaves within a fixed 256-record pool. A worker builds CPU meshes; the render thread uploads at most two normal-frame completions. Coarse parents remain until complete child groups can replace them. Near sampling is 4 m, with seven LOD levels, a 15 km atmospheric horizon, consistent shared-vertex heights/normals and closed skirts covering mixed-LOD interpolation gaps. Far skirts reach 96 m without extra geometry. The macro atlas occupies about 6.2 MiB.

Terrain GPU buffers remain approximately **13.664 MiB** in all measured runs. The tour's peak internally owned terrain CPU mesh storage is about **13.23 MiB**, with no failed jobs. These estimates exclude material textures, models, framebuffers, UI and driver memory; they are not total VRAM. CPU mesh storage also excludes transferred meshes after ownership passes to the renderer.

Grass uses up to 22,000 short nine-leaf tufts, with most roots inside 25 m and a sparse distant guard. Trees are limited to 80 rendered instances, with small fern/rock fields. Placement is deterministic and staged in batches of at most 4,096 candidates. Assets share the existing full-resolution texture cache; color/shadow uniform locations are cached separately. Stationary unchanged model selections avoid instance uploads. All native profile exits recorded zero GL errors and zero remaining shared texture owners.

## Verification

- Final build and full `make test` pass, including all existing Seldonne import/save/editor/shader checks.
- Atlas tests verify finite heights, coordinate signs, reference scale, water/land agreement, 15 dry landmarks, continuity and northwest foundation slope regressions.
- Streaming tests verify bounded occupancy, balanced adjacent LODs, negative keys, generation cancellation, reset, transferred mesh lifetime, winding, normals, skirts and teardown. Walking tests invoke the actual controller helper at 45/60/mixed frame rates, including diagonals, threshold slopes, downhill, short rises and invalid input.
- An exhaustive atlas-to-mesh check verifies all 26,364 possible LOD4/5 and 8,164 possible LOD5/6 border vertices. Maximum gaps are 10.43 m and 39.40 m, covered by the actual 24 m and 96 m skirts.
- Cover tests verify deterministic roots, independent anchors, bounded pumping, cap enforcement, water exclusion and retained arrays during cancellation. Model tests load seven actual assets, render color/shadow pixels, reject bad instances, validate two-program uniform caching and release all textures. These also pass on the native Radeon context.
- Targeted AddressSanitizer/UndefinedBehaviorSanitizer checks pass for atlas, tiles, walking and cover. Leak checks are included for the first three; cover's retained test invocation disables leak detection. The complete engine is not sanitizer-instrumented.
- The original map copy matches its source hash. Seldonne's authored/configuration root files match the before-work hashes, and its native arrival regression capture renders normally at approximately 60 FPS. See `preservation.json` for exact file accounting.

## Inspect the result

Run `./Launch_Valwood_World.sh` from the enclosing `final_engine` folder. The launcher reuses the existing engine lock; close the old project's session before switching projects. In this world, WASD walks, right-drag looks, Shift sprints, F toggles inspection flight and Q/E change flight height. Tab opens the source atlas with clickable gold destinations. The Landmark travel panel also exposes all 15 stops; keys 1–9 select the first nine. Escape closes the atlas/releases mouse.

[Generated geography overview](atlas-overview.png), [atlas controls](atlas-controls.png), [local forest](forest-walk.png), [mountain terrain](mountains-flight.png), [Ulasse lake](ulasse-lake.png), [original Seldonne regression](seldonne-regression.png). These are numerical terrain visualization or native renderer captures, not generated concept art. JSON and per-frame CSV records are retained alongside each run. The `diagnostic-` and `before-final-visuals-` files are historical development checks, not this final result.

Reproduce with `./tools/benchmark_valwood_world.sh --landmark valdoa`, or add `--tour --frames 5400` for the map tour. For flight add `--editor --landmark brandalhall --frames 1800 --no-hud`. The script opens a visible window and refuses to contend with a launcher-managed game session.

## Remaining world work

Settlements are geographic anchors only. The detailed Seldonne scene remains a separate project; its buildings, quests, characters and authoring workflow are not yet merged into this large world. Local sculpt/paint tools, persistent placed-content streaming, travel crossings, swimming gameplay and living-world AI are later milestones. The first-person atlas mode is an inspection runtime rather than a complete RPG expansion.

Coasts are a rough hand trace and some water systems use a shared sea level. Mountain shapes are broad procedural foundations rather than authored erosion/hero terrain. Vegetation still needs richer intermediate LODs, stronger canopy coverage and temporal antialiasing. Water reflections/shoreline treatment, terrain texture repetition and distant atmosphere remain below the intended AAA target. Lighting validation here uses the fixed daytime setting; this report does not certify every weather/night condition.
