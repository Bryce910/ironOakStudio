# Cinematic scenery results

Measured 23 October 2026 in the owner's native C/C++ SDL/OpenGL engine on the **AMD Radeon RX 5300, 3 GiB**, Mesa 25.2.8, visible **1600×900**, 60 Hz VSync. This is a substantial scenery upgrade and a measured inspection build; it does not establish finished AAA production quality.

## What changed

- **13 new sourced runtime models:** near/mid/far pine and fir, three photographed 3D grass tufts, three mossy rocks and one coastal cliff. Near trees retain source woody geometry with photographed needle cards placed over source canopy positions. Far trees are inexpensive camera-facing photographs. Embedded PBR maps, shared textures and capped instances keep them practical on this GPU. [Sources, CC0 licensing, exact budgets and adaptation limits](../../CINEMATIC_ASSET_SOURCES.md).
- Denser deterministic forests and distant canopy continuity; detailed nearby tree LODs blend into simplified meshes and silhouettes. Photographed grass covers the foreground, with curved procedural blades extending the distance. Smooth patch variation replaces the old hard grass islands.
- Ulasse headwalls, uneven alpine peaks, ridges, saddles, Valwood foothills and Crosswind coastal relief. Existing map geography, settlement foundations, **100.8 × 56.7088 km** coordinate space and ordinary **3.5 m/s** walking remain. Eight hours is the calibrated full east–west width, rather than a tested eight-hour connected walking route.
- Near terrain spacing improves from 4 m to **2 m**, with 64 cells per tile and seven LOD levels. Triplanar rock, normal/roughness/AO maps, slope-aware snow/soil/grass, macro terrain sun shadow and restrained ground specular add surface definition.
- Golden-hour light, sharper nearby shadows, foliage transmission, altitude-aware atmospheric depth, layered analytic clouds, HDR/filmic tonemapping, lightweight contact AO and edge antialiasing. Water now reflects the actual terrain and inexpensive tree proxies in a half-resolution HDR planar reflection.
- Four scenic inspection destinations: **Ulasse shore, Valwood forest, Mashito ridge and Crosswind coast**. Travel holds the previous image while the destination prepares; local movement continues to render during normal streaming.

## Native moving-camera checks

The five final runs are sequential, with one engine process and no concurrent asset cooks/tests. Each excludes 120 warm-up frames, uses the default fixed seed and 17.05-hour / 0.28-cloud lighting, and leaves the inspection panel visible. The benchmark moves approximately 170 m out and back over a 12-second cycle while sweeping the camera. This deliberately accelerated camera route stresses streaming; it is not a measurement of ordinary walking speed. The tour visits all 15 map destinations, jumping every 360 frames.

| Measurement | Forest | Lake | Ridge | Coast | All-map tour |
| --- | ---: | ---: | ---: | ---: | ---: |
| Total frames | 3600 | 3600 | 1800 | 1800 | 5400 |
| Measured samples | 3480 | 3480 | 1680 | 1680 | 5280 |
| Mean FPS, capped | 59.95 | 59.95 | 59.95 | 59.95 | 59.95 |
| 1% low FPS | 59.38 | 59.74 | 58.87 | 59.23 | 59.56 |
| p95 frame, ms | 16.703 | 16.701 | 16.703 | 16.701 | 16.706 |
| p99 frame, ms | 16.730 | 16.722 | 16.733 | 16.728 | 16.734 |
| Worst frame, ms | 19.086 | 16.798 | 20.709 | 18.643 | 17.021 |
| Within 45 FPS budget, % | 100.000 | 100.000 | 100.000 | 100.000 | 100.000 |
| Frames over 22.222 ms | 0 | 0 | 0 | 0 | 0 |
| Total world GPU mean, ms | 12.038 | 12.121 | 11.641 | 11.718 | 6.529 |
| Reflection GPU mean, ms | 0.000 | 0.944 | 0.000 | 1.041 | 0.210 |
| Held loading frames, total run | 0 | 0 | 0 | 0 | 2690 |

No measured interval in these workloads exceeded the 22.222 ms budget for 45 FPS; each run averaged 59.95 FPS under the 60 Hz cap. The 60 Hz cap prevents a claim about uncapped throughput. These are CPU frame-interval timestamps, not physical scanout measurements. Total GPU timings are asynchronous estimates including shadows, reflection and postprocessing. Generic per-stage/static-asset profile fields are unused in this backend; zero there means uninstrumented. Raw per-frame CSV and JSON accompany each profile.

The tour includes **2,690 held loading frames**, with a longest hold of **301 frames / approximately 5.02 seconds**. Its frame rate therefore describes responsiveness through destination loading, including held views. The tour GPU mean also includes those held views and does not represent active-render cost alone. The forest, lake, ridge, coast runs have **zero held loading frames** and supply the continuously rendered movement evidence.

A prior same-density pass is retained in `before-profiler-fix/`. Its forest run recorded three intervals over the 45 FPS budget, with a 55.49 FPS 1% low. Two misses coincided with CSV flush boundaries: the former `setvbuf(NULL, ..., 65536)` call actually used a 4 KiB libc buffer on this machine. The profiler now owns an explicit 64 KiB backing buffer, closed before reset; a regression test verifies buffering and complete shutdown/restart flushes, and the former implementation fails that test. This correspondence suggests profiler I/O contributed to the earlier misses without proving their complete cause. The table above repeats all five routes after this correction and the ridge/coast composition fixes; the rendering/material settings are otherwise unchanged.

These short runs do not certify every camera position, night/weather configuration, future RPG content, higher resolutions or an eight-hour play session. Stable scenic stills are visual checks, not additional long performance evidence.

## Bounded rendering and streaming

The fixed terrain pool has 256 records, 252 desired records and 198 target leaves; the worker stages meshes and the render thread normally uploads at most two completions per frame. Coarse ancestors remain until their replacements are ready. Exhaustive checks cover both 32- and 64-cell mixed-LOD borders; the actual 24 m and 96 m skirts cover the measured interpolation gaps.

Terrain buffers use **50.947 MiB**; the maximum internally owned CPU terrain meshes in these runs is **41.445 MiB**. The macro atlas uses approximately **8.46 MiB**. These are component budgets, not total engine VRAM/RAM; textures, models, framebuffers, driver memory and transferred CPU meshes are outside those terrain counters.

Local placement stages at most 4,096 candidates per pump, with bounded fields of 46,000 grass roots and 7,000 tree roots. Stable global ranking preserves the protected nearby neighborhood when capped fields overlap. Nearby photographed grass is capped at **10,500 submitted tufts** across three 3,500-instance batches. Near/mid trees have separate small caps; local and horizon silhouettes use batches below the model loader's 4,096-instance limit. Horizon selection stages 2,048 candidates per pump, retains at most 100,000 roots and submits at most 28,000 cards in eight buckets. Both main and reflected view visibility participate in selection. Counts in `.world.json` describe submitted instances before projected-size/alpha rejection, rather than all-visible trees.

One shared texture cache owns repeated embedded images. Instance buffers upload only when selections change. Every final native run exits with **zero GL errors, failed tiles or remaining shared texture owners**. This verifies the recorded teardown paths; it is not a full allocation/leak proof.

## Verification and preservation

- Release build and full `make test` passed, including existing project import/save/editor, audio, shader and runtime checks.
- Model tests load **20 real GLBs** (seven existing plus 13 new), validate bounds and invalid instance rejection, render color/shadow pixels including the camera-facing far-card path, switch shader programs, and release shared texture owners. They also pass on the actual Radeon context.
- Atlas tests verify the map scale, 15 dry landmarks, foundation behavior, mountain headwalls, finite heights, coordinate round trips and continuity. Streaming/controller tests cover bounded pools, cancellation, lifetime, seam normals, skirts, slope handling and ordinary walking at 45/60/mixed frame rates.
- Exhaustive 64-cell border checks visit 52,636 LOD4/5 and 16,272 LOD5/6 vertices, with maximum gaps 17.897 m and 19.571 m. Existing 32-cell compatibility remains checked too.
- Current atlas, tile, controller, actual 32/64-cell LOD meshes and cover tests pass AddressSanitizer/UndefinedBehaviorSanitizer. **Leak detection was disabled** in these targeted invocations; the full renderer is not sanitizer-instrumented.
- The changed viewer passes strict `-Wall -Wextra -Werror` syntax compilation. The three asset/benchmark Python tools compile. Baked assets pass metadata/image/triangle/finite-data validation.
- The profiler buffer lifecycle regression passes normally and under ASan/UBSan (leak detection disabled); it checks buffered rows, restart/disable flushes and a 10,000-sample complete CSV.
- The source map and retained copy have matching SHA256. Seldonne's before-work authored root files remain identical; `preservation.json` accounts separately for runtime logs/layout files. The original detailed Seldonne scene remains its own project.

See `validation.txt`, `build-manifest.json`, the per-run logs/CSV/JSON and the source manifest for exact evidence. Historical captures/profiles under `before-dense-grass/`, `before-profiler-fix/` and diagnostic files are development comparisons, not the final measurements above.

## Inspect and reproduce

From the enclosing `final_engine` directory:

```sh
./Launch_Valwood_World.sh --scenic forest --width 1600 --height 900
```

Use **WASD** to walk, **right-drag** to look, **H** to hide/show the inspection panel, **F** for inspection flight and **Q/E** for flight height. The panel's **Scenic trails** buttons select lake/forest/ridge/coast; the forest destination's name does not imply an authored trail mesh. **Tab** opens the map and its landmark destinations. The launcher focuses an existing session instead of starting a second GPU workload.

Final stable native captures: [forest](forest-view.png), [Ulasse lake](lake-view.png), [Mashito ridge](ridge-view.png), [Crosswind coast](coast-view.png). These images come from the running engine, not generated concept art.

Close the interactive engine before reproducing the sequential performance suite:

```sh
python3 tools/benchmark_cinematic_scenery.py
```

## Remaining visual and world work

This pass supplies better models, material response, depth and density; finished AAA scenery still needs authored hero terrain/erosion, more canopy species and understory, less repetitive placement, polished paths/shore transitions and stronger temporal antialiasing. Far trees are single-view photographic cards with approximate baked light, clouds are analytic rather than volumetric, contact AO is lightweight, and planar reflections use cheap proxies. Lighting validation here covers the fixed golden-hour setting rather than the complete day/night/weather range.

Settlements remain geographic anchors in this large-world inspection project. Buildings, quests, characters, swimming/crossings, persistent placed-content editing and the planned offline living-world AI are separate unfinished milestones. They are not implied by the visual upgrade or these frame-rate results.
