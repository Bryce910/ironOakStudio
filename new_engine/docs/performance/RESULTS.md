# Cinematic rendering and responsiveness results

## Latest: engine optimization and frame pacing

Measured on 23 October 2026, native AMD Radeon RX 5300 (3 GiB), Mesa 25.2.8, visible 1600×900 window and ordinary 60 Hz VSync. Before/after play use the same 3,600-frame travel route; before/after editor use 1,800 frames. All exclude 120 warm-up frames, use seed 4217 and frozen world lighting, and run one engine process at a time. The final sweep is a separate 3,600-frame travel/camera-orbit stress check with the normal gameplay HUD enabled. Play/editor comparisons suppress the gameplay HUD; editor authoring panels remain visible.

The final starting-area runs average 59.95 FPS. Matched play improves its slowest-1% average from 53.85 to 56.99 FPS, with two isolated ~30 ms frames remaining. The sweep has no measured frame below 45 FPS. This supports the requested range for the tested starting area at this resolution, but does not establish a strict minimum in every workload.

| Measurement | Before play | Final play | Before editor | Final editor | Final sweep + HUD |
| --- | ---: | ---: | ---: | ---: | ---: |
| Measured samples | 3480 | 3480 | 1680 | 1680 | 3480 |
| Mean FPS (display capped) | 59.95 | 59.95 | 59.95 | 59.95 | 59.95 |
| 1% low FPS | 53.85 | 56.99 | 56.97 | 56.60 | 57.06 |
| p95 frame (ms) | 16.703 | 16.704 | 16.711 | 16.707 | 16.867 |
| p99 frame (ms) | 16.770 | 16.735 | 16.741 | 16.743 | 17.255 |
| Worst frame (ms) | 34.068 | 30.157 | 30.207 | 30.358 | 19.340 |
| Frames over 22.222 ms | 4 | 2 | 1 | 1 | 0 |
| Within 45 FPS budget (%) | 99.885 | 99.943 | 99.940 | 99.940 | 100.000 |

Statistics above are independently recomputed from CSV using one nearest-rank convention for all runs. The 1% low is the reciprocal of the average duration of the slowest ceil(N/100) frames. These are consecutive CPU frame intervals sampled before swap, not physical display scanout measurements. The editor's one rare slow frame remains about 30 ms, with a slightly worse maximum/1% low than its baseline. Both builds already reach the display cap; this is not evidence of a large uncapped average-FPS gain. Stage GPU timings varied and do not show a uniform speed-up in the final capped run.

### Changes retained

- Matching embedded GLB images share full-resolution textures across model files. Keys verify the entire encoded content plus color space and effective sampler/mipmap policy. Per-material-field ownership handles duplicate mesh fields and independent renderer translation units. Static GPU accounting counts actual shared IDs once. The starting-area static estimate falls from 943.869 to 879.869 MiB: 64 MiB / 6.8%. This excludes terrain, characters, render targets and driver allocations; it is not total VRAM. The texture-sharing registry retains about 195 MiB of encoded source data in system RAM for collision-safe comparisons, with a 256 MiB cap and unmanaged fallback.
- Grass rejects blocked roots before computing blade curves and avoids repeated terrain/water queries. Deterministic rooted blades and distance-LOD prefixes remain stable. Grass-paint undo now also invalidates terrain coverage.
- Terrain shares blend/noise calculations and skips normal samples only after their final weight reaches zero. Opaque back-facing surfaces skip unnecessary shadow visibility taps while foliage transmission retains them. The SSAO blur uses nine bilinear taps instead of twenty-five point taps; disabled AO/bloom skip their passes and use neutral targets.
- The final default framebuffer no longer requests 4× MSAA for the presentation blit/UI. World geometry was already rendered into a single-sample HDR target and retains FXAA. Detailed GPU query work runs only while profiling or showing the debugger. Window titles update only when changed.
- NPC/enemy reloads release old owners and animation evaluation state. Added cleanup for terrain/water mesh buffers, vegetation resources, mounts and shadow maps. Wide-shadow refresh recognizes a restarted frame counter. This addresses the audited resources; it is not a claim that every legacy restart allocation has been eliminated.
- Profiling buffers CSV output, preallocates sample storage and records raw CPU spans, preceding-swap duration, budget misses, worst frames and 1% low FPS. GPU stages remain asynchronous smoothed estimates; CPU spans can include driver waits. Neither can simply be added to the other to derive frame time. The `render_total_gpu_ms` field covers world rendering and post-processing; it excludes the separate shadow pass.

### Changes rejected after native testing

Adaptive VSync and skipping empty ImGui backend submissions were tested. Removing empty submissions shifted driver waiting into swap and worsened play p99 to 19.697 ms despite the same 59.95 FPS average. Restoring unconditional backend submission recovered p99 to 16.735 ms. Ordinary VSync is retained. Diagnostic JSON/CSVs with `diagnostic-` prefixes are preserved and are not the final result.

### Verification and evidence

The final `make -j4` and repeated `make test` pass. Tests cover launch/import/save/editor workflows, terrain-buffer lifetime, paint invalidation, async thumbnail cancellation, repeated NPC reload reference counts, exact-content collisions, sampler/color-space separation, cache fallback and cross-translation-unit GL texture ownership/deletion. Standalone texture-cache, grass and profiler tests also pass AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer outside the traced sandbox. These sanitizer checks do not instrument the whole engine.

The 72 terrain/static material image comparisons and 168 SSAO blur image cases pass on llvmpipe and the native Radeon. On Radeon, material outputs matched exactly for the exercised cases; blur float differences were at most 2.39e-7, with at most one R8 output code difference. Tests retain tighter floating-output tolerances and allow one quantized code for either source format because float roundoff can cross an R8 rounding boundary. Full source texture dimensions, decoded pixels, color formats, sampler state and final deletion are checked. No texture downsizing was used for this pass.

Profiles: [before play](frame-pacing/before-play.json), [final play](frame-pacing/after-play.json), [before editor](frame-pacing/before-editor.json), [final editor](frame-pacing/after-editor.json), [sweep with HUD](frame-pacing/after-sweep-hud.json). Per-frame CSVs accompany every profile; [independent comparison](frame-pacing/comparison.json) and [build manifest](frame-pacing/build-manifest.json) identify this pass. Captures: [before arrival](frame-pacing/before-arrival.png), [final arrival](frame-pacing/after-arrival.png), [normal HUD](frame-pacing/after-sweep-hud.png), [editor](frame-pacing/after-editor.png), [close meadow](frame-pacing/after-meadow.png). Renderer captures were inspected for missing materials and visual regressions.

Reproduce from the engine root with `./tools/benchmark_valwood.sh play`, `./tools/benchmark_valwood.sh editor --frames 1800`, or `./tools/benchmark_valwood.sh play --view sweep`. The script uses a visible window, 120 warm-up frames and 3,600 frames by default; its gameplay HUD is suppressed. For the HUD stress run invoke `./rpg --project projects/seldonne --play --benchmark --view sweep --warmup 120 --frames 3600 --width 1600 --height 900 --profile docs/performance/local-sweep-hud.json` after closing the running game. Launcher locks prevent accidental duplicate sessions.

These checks do not cover sustained combat, all scenes, larger resolutions, arbitrary imported assets or long editor soak sessions. Rare stalls remain in matched play/editor, and the scene still falls below the intended AAA visual target. The next rendering work remains temporal antialiasing, stronger foliage LODs and more consistent scenery assets.

## Earlier grass revision

Replaced the tall uniform crop with shorter curved, tapered ribbons in irregular fan-shaped clumps. Blade normals follow the curve and transverse fold, then bend toward the canopy for foliage fill. Thin-leaf transmission, root occlusion, olive/straw variation, coherent wind and player/trail bending remain part of the material. Terrain cover uses the same world-space ecology and density/style/tint authoring masks, with soil-darkened reflectance, while roads, water, camp clearings, snow and rocks retain their own weights. Grass now writes depth, uses analytic edge/subpixel coverage, and no longer applies a second taper in the fragment shader.

A deterministic scatter module retains the same rooted blades as density changes with distance. The 180k-blade cap samples the whole circle at high densities, replacing the old north-to-south cutoff. Rebuilds retain the visible field, stage twelve rows per frame, reject empty distant cells before world queries and orphan uploads. The small shared paint texture reuses storage.

Native Radeon RX 5300 / Mesa 25.2.8, 1600×900, 60 Hz/VSync. Old/new play follow the same 720-frame out-and-back route with 60 warm-up frames. The new editor uses the first 360 frames, also excluding 60 warm-up frames, so its column is an independent editor check.

| Measurement | Previous grass / play | New grass / play | New grass / editor |
| --- | ---: | ---: | ---: |
| Frames / measured samples | 720 / 660 | 720 / 660 | 360 / 300 |
| Startup (s) | 8.96 | 8.61 | 8.67 |
| Mean FPS (capped) | 59.95 | 59.87 | 59.84 |
| Median frame (ms) | 16.680 | 16.680 | 16.679 |
| p95 frame (ms) | 16.705 | 16.694 | 16.703 |
| p99 frame (ms) | 16.760 | 16.709 | 16.783 |
| Worst frame (ms) | 30.610 | 39.441 | 35.339 |
| Grass GPU (ms) | 1.498 | 0.986 | 0.890 |
| Terrain GPU (ms) | 3.972 | 4.151 | 4.173 |
| Update CPU (ms) | 0.255 | 0.857 | 0.930 |

Grass GPU time falls about 34% in the final matched play run. The denser near-field scatter uses more update CPU time than the previous field, and terrain gains a coverage/color blend. Both play versions reach the display cap; these data do not establish a higher uncapped FPS. The final runs still have isolated 35–39 ms spikes, including a worse maximum than the previous field. Further streaming/presentation work is required before claiming hitch-free operation. Static-resource estimates remain 20 unique assets / 165 live instances / approximately 944 MiB, excluding other allocations.

An older Valwood process was still running a superseded binary during the initial checks. With both copies competing for the GPU and memory, frame pacing degraded severely. Closed the stale play window, then repeated each measurement with one engine process. The launchers now hold an OS-released instance lock and focus the existing window on another launch; the benchmark script rejects a run while a launcher-managed session is open. The `*-contended.json` files and the initial `before-play.json` are diagnostic records, not valid isolated performance comparisons.

Final data: [previous field](grass-pass/before-matched.json), [new play](grass-pass/after-play.json), [new editor](grass-pass/after-editor.json), with per-frame CSVs alongside. Renderer captures: [previous arrival](grass-pass/before-arrival.png), [new arrival](grass-pass/after-arrival.png), [player-height meadow](grass-pass/after-meadow.png), [editor](grass-pass/after-editor.png). All new captures were inspected at native resolution.

`make -j4` and `make test` pass, including actual PBR/terrain/grass shader linkage. Added checks for deterministic world roots, retained LOD prefixes, masks, negative coordinates, finite inputs and the full-field budget. The GL integration test checks grass-paint texture invalidation and upload. Shell syntax and held-lock behavior were verified for both launch and benchmark scripts. Existing optional animation fallback messages and unrelated legacy compiler warnings remain.

This remains a visual prototype. Fine foliage can shimmer without temporal AA; LOD density changes and approximate foliage occlusion still need further work. The surrounding tree silhouettes, mixed asset styles, hillside forms and indirect lighting also remain below the AAA target.

## Earlier cinematic pass


Tested 23 October 2026 on the native AMD Radeon RX 5300 (3 GiB), Mesa 25.2.8, at 1600×900. Each matching run starts a fresh process, follows the same deterministic moving-camera route for 360 frames, and excludes 60 warm-up frames. VSync is enabled at 60 Hz. Source/OS/shader caches can affect startup. Baseline contains the original sparse scene; the cinematic build also renders substantially denser vegetation, additional dressing and full static-object PBR. The comparison therefore includes visual-quality work, not only optimization.

| Measurement | Baseline play | Cinematic play | Baseline editor | Cinematic editor |
| --- | ---: | ---: | ---: | ---: |
| Startup (s) | 19.26 | 8.68 | 18.92 | 8.58 |
| Mean FPS (capped) | 59.95 | 59.75 | 59.95 | 59.95 |
| Median frame (ms) | 16.68 | 16.68 | 16.68 | 16.68 |
| p95 frame (ms) | 16.70 | 16.69 | 16.75 | 16.73 |
| p99 frame (ms) | 17.67 | 16.72 | 16.79 | 16.78 |
| Worst frame (ms) | 30.27 | 32.17 | 29.16 | 33.45 |
| Terrain GPU (ms) | 8.13 | 4.10 | 8.31 | 4.07 |
| Shadow GPU (ms) | 1.09 | 3.29 | 1.07 | 3.32 |
| Prop GPU (ms) | 0.73 | 1.37 | 0.66 | 1.31 |
| Grass GPU (ms) | 0.07 | 1.55 | 0.08 | 1.54 |

Both versions largely hit the display cap. These results establish reduced startup and terrain GPU cost while adding detail; they do not establish a higher uncapped average frame rate. Rare 32–34 ms frames persist, including worse maximum frame times than the baseline in these final short runs. Occasional first-load/streaming and presentation stalls remain. Do not describe this build as hitch-free or commercially certified.

Static-resource telemetry in the final runs records 20 unique cached assets serving 165 live instances, 147 cache hits and about 944 MiB of estimated unique static GPU storage. That estimate excludes terrain, animated characters, render targets and driver allocations; it is not total VRAM consumption. Baseline cache fields are zero because that system did not exist, not because it used no memory. The warm-cache eviction target cannot evict assets that still have live instances.

GPU stage values average smoothed asynchronous timestamp measurements. CPU sections include driver blocking, so the large `editor_ms` value must not be interpreted as pure UI computation. JSON and per-frame CSV data are retained beside this report. The earlier 720-frame out-and-back run is retained as `cinematic-play-long.json`; it predates the final terrain culling/material polish and is a separate extended check.

## Validation

`make -j4` and `make test` pass. Tests cover launch validation, import copies and duplicate protection, malformed input, shared audio shutdown/reinitialization under AddressSanitizer, actual GLB placement/transform/collision, Save World/readable/terrain/light round trips, PBR/terrain/grass shader linkage, shared-resource lifetime across release/reload, terrain vertex/index buffer destruction and async thumbnail generation cancellation. Existing unrelated compiler warnings remain in legacy engine code; the newly added cache/profiling/thumbnail modules introduce no compiler warnings in the final build.

Native arrival, river and editor captures were reviewed. These are renderer outputs; no generated concept image replaces gameplay evidence. Original project files before the scene-dressing pass are retained under `projects/seldonne/backups/cinematic-pass`.

## Inspect and reproduce

- [Original arrival](captures/baseline-arrival.png)
- [Final arrival](captures/cinematic-arrival.png)
- [Final river](captures/cinematic-river.png)
- [Final editor](captures/cinematic-editor.png)

From the engine root: `./tools/benchmark_valwood.sh play` or `./tools/benchmark_valwood.sh editor`. Local runs write `local-play.json` / `local-editor.json`, preserving these reference measurements. The historical runs below used shorter defaults; the current benchmark defaults to 3,600 frames. Run on the native desktop GPU; offscreen llvmpipe results are not comparable.

## Remaining visual and engineering work

The scene still has mixed stylized and realistic assets, thin/aliased tree silhouettes, simple grass geometry, abrupt mesh/impostor transitions and limited indirect lighting/reflection quality. Temporal AA, higher-fidelity intermediate foliage LODs, consistent hero assets, reflection probes and broader streaming/editor soak tests are the next milestone. See [the production roadmap](../PRODUCTION_ROADMAP.md). This pass is a foundation for the AAA target; it does not meet that target yet.
