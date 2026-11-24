# Silverpine Waystation — native validation

The expanded starting area is implemented in the existing native Valwood engine. It adds **972 authored placements** to the retained 283, for **1,255 total**. These include a walk-through stone arch, ruin tower and connected wall fragments, five dressed tables, plank floors, supply stacks, firepits, stumps, rocks, framing trees and planted beds. The area is a walkable environment test slice; quests, NPC behavior and the planned persistent micro-AI simulation are not implemented by this pass.

[Layout and editing plan](../../STARTING_AREA_PLAN.md) · [Actual overhead placement plan](../../../projects/valwood_world/scenery/starting_area_layout.svg) · [Model sources and licenses](../../STARTING_AREA_ASSET_SOURCES.md)

The kit contains 18 GLBs from 12 new Poly Haven CC0 source families. Their original geometry, UVs, photographic albedos and normal/ARM materials are imported into the engine; four models have reduced middle-distance meshes. Shared images add approximately 140.0 MiB of decoded RGBA8 textures including mipmaps. This is an asset-budget estimate, not total GPU memory. Assets are local and require no runtime download.

Six feathered terrain foundations and a cached 513×513 mask support a **350.257 m return walk**. The same cached footprint data keeps grass out of floors, walls, solid props and paths while retaining controlled cover in forest clearings. Added scenery is grouped by model and LOD; steady-state updates and capsule queries allocate no memory. Gate collision uses two piers and a header, leaving its real 3.501 m opening clear.

All measurements below ran sequentially on the **AMD Radeon RX 5300 at 1600×900**, using the visible native driver, the normal HUD and 60 Hz presentation. Each run discards 120 warm-up frames. Software/offscreen GL checks are correctness tests and were excluded from FPS evidence. Renderer strings, captures, raw frame CSVs and world/resource reports accompany each profile.

| Native scene | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames >22.222ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [route](route-final.json) | 6,120 | 59.95 | 59.25 | 16.723 | 22.293 | 1 | 12.804 |
| [start](start-final.json) | 1,680 | 59.95 | 59.58 | 16.721 | 17.338 | 0 | 13.197 |
| [courtyard](courtyard-final.json) | 1,680 | 59.95 | 59.52 | 16.729 | 17.265 | 0 | 12.399 |
| [workshop](workshop-final.json) | 1,680 | 59.95 | 58.82 | 16.723 | 20.970 | 0 | 12.755 |
| [overlook](overlook-final.json) | 1,680 | 59.95 | 59.61 | 16.727 | 17.193 | 0 | 12.579 |
| [grove](grove-final.json) | 1,680 | 59.95 | 59.75 | 16.723 | 16.756 | 0 | 13.238 |
| [forest](regression/forest-final.json) | 1,680 | 59.95 | 59.77 | 16.721 | 16.747 | 0 | 13.312 |
| [lake](regression/lake-final.json) | 1,680 | 59.95 | 58.77 | 16.723 | 20.772 | 0 | 13.136 |

The six starting-area runs measured **14,520 frames**, with **1** frame above the 45 FPS budget. Including the two original-view regressions gives **17,880 measured frames**, **1** isolated miss and **99.99441%** within budget. Every run averaged 59.95 FPS. The single 22.293 ms route frame corresponds to 44.86 FPS; the measurements do not claim an absolute 45 FPS floor under every desktop workload.

The normal-speed route advanced **350.256672 m** over **6,005 walking frames / 100.083 simulated seconds**, reached all 18 waypoints, returned within 1 mm of its origin, and recorded **zero collision blocks, slope blocks or sliding retries**. Its 120 waiting frames are only the declared warm-up. The route uses the actual movement and collision helpers, rather than teleporting between viewpoints.

All eight profiles reported **zero terrain-loading holds, failed tiles, GL errors or remaining texture-cache owners**. GPU times are delayed query measurements including shadows and reflections; per-stage fields that the viewer does not instrument are not used as evidence. Cold startup/model loading occurs before these in-play measurements and ranged from 6.42 to 6.57 seconds in the six starting-area runs.

Visual captures at normal 1.75 m eye height:

- [Arrival arch](start-view.png)
- [Courtyard dressing](courtyard-view.png)
- [Working camp](workshop-view.png)
- [Overlook](overlook-view.png)
- [Grove](grove-view.png)

The first captures are retained in `iteration1/`. They exposed overly bare clearings and distant inspection cameras; the final revision restores controlled grass away from hard footprints, joins courtyard wall sections, adds overlook dressing and moves four views closer to their activity pockets. These captures show the current renderer, not concept art. AAA visual polish remains further work: the simple sky/cloud model, broad terrain surfaces, vegetation presentation and indirect/contact lighting are still visible limitations.

Verification evidence:

- [Full test suite](tests.log) and [final scene/camera/terrain checks](tests-final-scene.log): `make test` passed, including atlas geography/shorelines, terrain LOD seams, cover staging/determinism, texture ownership, material/shader regressions, real model imports and movement.
- [Actual-model Radeon scene check](scenery-native.log): 1,255 records, 26 aliases, 39 render groups, 280 collision shapes and 1,872 continuous half-metre sweeps across three player-capsule lanes; eight views draw real color and shadow shaders.
- Native [water](water-native.log), [foliage](foliage-native.log), [lighting](lighting-native.log), [temporal AA](temporal-native.log) and [model](models-native.log) fixtures pass on the Radeon. The final scene fixture also uses the revised cameras.
- [Camera regression source](../../../tests/world_camera_test.cpp): actual viewer sliding, slope composition, prop-top/ceiling contact, fixed-speed complete route and honest blocked-route reporting.
- [Scenery ASan/UBSan](scenery-asan.log): bounded parser/resource ownership, all new aliases, gate/rotated-corner sweeps, stacked props and allocation-free full-cap updates. LeakSanitizer was disabled for that CPU run; explicit cache/owner assertions cover GL teardown.
- [Asset validation](asset-validation.log), [layout validation](layout-validation.log), and [preservation audit](preservation.json) pass. All 35 original Seldonne authored files, the original 283 placement records and supplied map bytes remain unchanged.
- [Build fingerprints](build-manifest.json) match 206 executable/source/asset inputs before and after profiling. [Machine-readable summary](summary.json) retains aggregate counts and route statistics.

To test, run `../Launch_Valwood_Starting_Area.sh` from the engine root, or use `../Launch_Valwood_World.sh --scenic start --width 1600 --height 900`. Walk with WASD, hold right mouse to look, Shift to run and Space to jump. H toggles the HUD; its scenic buttons return to each authored pocket. F switches fly mode; Tab opens the world map. The tower is a closed exterior; lanterns and firepits are currently unlit visual props. Solid authored objects have collision; decorative foliage and the generated background forest remain nonblocking.

Reproduce from the engine root:

```sh
make -j4
make test
python3 tools/build_starting_area.py --validate-only
python3 tools/validate_starting_native.py
python3 tools/benchmark_starting_area.py --captures
python3 tools/benchmark_starting_area.py
python3 tools/check_starting_area_preservation.py
python3 tools/record_starting_area_build.py --verify
```

Native verification/profiling must run with access to the host Radeon; the sandbox otherwise selects llvmpipe. Close the live game first. Keep GPU/CPU workloads idle during profiles. The asset source and collision documents describe reproduction and supported authoring fields.
