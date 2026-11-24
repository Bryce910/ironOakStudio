# Lighting, shorelines and vegetation: native results

Recorded 2026-10-24T14:37:51.529480+00:00. This pass continues in the owner's native C/C++ SDL/OpenGL engine. Lighting coherence and vegetation coverage improve; the shoreline changes are local and visually subtle. This is not finished AAA fidelity.

## Implemented

- Shared diffuse sky/ground lighting, direct sunlight and visible sky radiance across terrain, vegetation, sky and water. Four fixed terrain-height probes estimate horizon sky access, with a tangent-plane correction so an ordinary hillside is not darkened twice. Broad moving cloud shadows use the same world-anchored field across surfaces. Cloud edges and horizon haze were refined after the first native review.
- Two cached fine shoreline fields around the Ulasse lake and Crosswind review areas. Uneven banks, gravel shelves and runnels meet small coves/spits. Effective signed shore may change by at most 8 m within these fields; the broader map, settlement foundations and reference world dimensions remain intact. Dense checks measured offsets from −7.815 to +7.611 m and 89 local sign changes, all inside the permitted reference band. Both original 33,153-point coarse classification goldens remain unchanged, and all 33,089 sampled core points retain their signs.
- Neutral gravel and a softly varied damp reach follow actual terrain and a coast-only material gate. Low inland ground stays dry. The two paired 513×513 shoreline caches add about 4.016 MiB over the preceding atlas, bringing CPU atlas storage to about 18.50 MiB. The earlier height-only shoreline draft measured roughly 79–83 ns in a CPU sampling workload; final paired-field microtiming was not isolated. Final scene frame times below cover the integrated build.
- A separate opaque HDR/depth snapshot gives shallow water bottom transmission, guarded refraction and a broken contact wash band. All four texels in the bilinear color footprint must be submerged, preventing foreground banks leaking into refraction. Reflections have a bounded three-tap filter; nearby bright sun glints respect the existing shadow map. The snapshot costs 16.48 MiB at 1600×900 and is allocated lazily.
- Tree LOD choices use each instance root, with all mask/size/distance coverage combined inside one LOD interval. This fixes premature disappearance during overlapping fades. Cards and meshes share tint, haze, broad lighting and cloud attenuation; physical canopy heights replace transparent-canvas heights. Alpha edges and the normal-detail cutoff transition more smoothly. Instance caps, geometry assets and draw distances are unchanged.
- A fifth **Scenic trails** viewpoint, **Ulasse | At the waterline** (`--scenic shore`), starts on dry ground at the normal 1.75 m eye height. The original lake comparison entrance remains exactly 4.054718 m above sea level.

[Plan](../../LIGHTING_SHORELINE_PLAN.md), [lighting contract](../../WORLD_LIGHTING.md), [water details and limits](../../WORLD_WATER.md), [terrain contract](../../CINEMATIC_TERRAIN_SHAPING.md), [foliage validation](FOLIAGE.md).

## Radeon frame times

Visible 1600×900, 60 Hz VSync, world tools shown, animated foliage/water/clouds. Actual renderer: **AMD Radeon RX 5300 (radeonsi, navi14, LLVM 20.1.2, DRM 3.64, 7.0.0-38-generic)**, OpenGL 4.6 / Mesa 25.2.8. One engine process; no concurrent build, cook or test workload. Each route excludes 120 warm-up intervals. Forest/lake run for 3,600 frames each, ridge/coast/shore for 1,800, and the 15-landmark tour for 5,400.

**17,280 measured intervals, 59.95 FPS weighted mean, 99.988% within the 45 FPS frame budget.** There were 2 intervals over 22.222 ms; the worst was 25.326 ms. No profiler samples were dropped.

| Route | Intervals | Mean FPS | 1% low FPS | p99 ms | Worst ms | >22.222 ms |
|---|---:|---:|---:|---:|---:|---:|
| Forest | 3,480 | 59.95 | 59.39 | 16.740 | 17.430 | 0 |
| Lake | 3,480 | 59.95 | 58.09 | 16.738 | 25.326 | 2 |
| Ridge | 1,680 | 59.95 | 59.55 | 16.723 | 17.558 | 0 |
| Coast | 1,680 | 59.95 | 59.59 | 16.721 | 16.966 | 0 |
| Shore | 1,680 | 59.95 | 59.77 | 16.719 | 16.809 | 0 |
| Tour | 5,280 | 59.95 | 59.35 | 16.737 | 17.866 | 0 |

The accelerated camera follows an approximately 170 m out-and-back sweep per 12 seconds and turns through a full orbit. It is a stress route, not ordinary 3.5 m/s walking. Shader time advances at frame/60 for reproducibility. These short VSync-limited checks do not guarantee a minimum frame rate at every position, weather setting, resolution or gameplay load.

The five comparable routes contain 15,600 measured intervals and 2 budget misses; the preceding authored build had 8 misses across those same route durations. The extra close-shore route adds 1,680 intervals and has no earlier matching profile. The complete prior data is retained under [before](before/RESULTS.md), and all current JSON/CSV/logs remain beside this report. No slow frames or unsuccessful visual iterations were hidden.

| Comparable route | Earlier misses | Current misses | Earlier render GPU ms | Current render GPU ms |
|---|---:|---:|---:|---:|
| Forest | 2 | 0 | 12.091 | 12.413 |
| Lake | 1 | 2 | 12.609 | 12.996 |
| Ridge | 2 | 0 | 12.254 | 12.615 |
| Coast | 1 | 0 | 11.966 | 12.402 |
| Tour | 2 | 0 | 8.121 | 8.563 |

GPU timestamps are delayed smoothed estimates, not exact attribution for a particular slow interval. Total render GPU time includes shadows and reflections; reflection time must not be added again. CPU submission/editor sections may include driver waits. Neither a longer UI interval nor a lower capped mean establishes the underlying cause. Unused legacy stage/static-asset fields remain zero and are not complete resource measurements. Differences between these sessions should not be treated as a controlled attribution to one isolated feature.

| Route | Opaque copies | Snapshot peak MiB | Held-view frames | Longest hold | World startup s |
|---|---:|---:|---:|---:|---:|
| Forest | 0 | 0.000 | 0 | 0 | 5.366 |
| Lake | 3600 | 16.479 | 0 | 0 | 5.285 |
| Ridge | 0 | 0.000 | 0 | 0 | 5.304 |
| Coast | 1800 | 16.479 | 0 | 0 | 5.331 |
| Shore | 1800 | 16.479 | 0 | 0 | 5.309 |
| Tour | 712 | 16.479 | 1911 | 265 | 5.258 |

The four original local routes and close-shore run have zero held views. Landmark jumps hold the prior image until terrain is ready: 1,911 total held frames, longest 265 (approximately 4.42 seconds at 60 Hz). World counters, including opaque copies and held frames, include warm-up. Both tour FPS and its GPU average include held views, so they do not measure active rendering alone. World startup is preparation outside the frame interval window; the profiler's separate startup field is not end-to-end launch latency.

## Correctness and resource checks

Release build, strict changed-module compilation, the full suite and the subsequent targeted launcher regression pass. Six final native fixtures cover actual water/foliage/lighting shaders, authored scenery grounding and cleanup, temporal history, and 28 real model fixtures. The new preset initially failed the launcher's separate allowed-name list; the validator/help and launcher test were fixed, and the shore capture was repeated successfully.

Atlas/actual 32/64-cell LOD/cover sanitizer checks pass with AddressSanitizer and UndefinedBehaviorSanitizer; leak detection is disabled in this environment. Negative controls fail for removed shoreline offsets, horizon/tangent correction, world cloud anchoring, inland material gates, sky horizon fade, the refraction footprint guard and water glint shadows. These inspect real outputs, rather than assuming that shader compilation proves visual correctness. [Validation](validation.txt) and [preservation](preservation.json) record scope and evidence.

All six profiles have zero failed terrain tiles, GL errors and remaining shared texture entries/references at teardown. Observed grass/photographic-grass/horizon counts remain within the existing 46,000 / 10,500 / 100,000-root and 28,000-card bounds. `authored_largest_batch` remains a final-update snapshot, not a run peak. Terrain GPU storage remains about 50.947 MiB; maximum observed CPU tile memory is 40.839 MiB. These exclude atlas, scene assets and framebuffers. TAA still uses about 32.96 MiB; the optional new water pair adds 16.48 MiB. Component estimates are not a full VRAM residency audit.

The original map image and all 35 authored Seldonne files remain unchanged. The world is still 100.8×56.7088 km, with a 3.5 m/s walking speed and eight-hour reference-width calibration. A continuous eight-hour navigable land route or performance soak has not been validated. [Source/result fingerprints](build-manifest.json) identify this build.

## Final native captures and visual limits

All five stills use the normal on-foot eye height with tools hidden. The moving benchmark's end positions and tools are retained separately as `*-final.png`.

| Forest | Lake |
|---|---|
| [![Forest](forest-view.png)](forest-view.png) | [![Lake](lake-view.png)](lake-view.png) |
| Ridge | Coast |
| [![Ridge](ridge-view.png)](ridge-view.png) | [![Coast](coast-view.png)](coast-view.png) |

[![Close shore](shore-view.png)](shore-view.png)

The ridge and nearby rocks show clearer light/shade separation. Sky refinement hides the worst flattened horizon ribbons, but clouds still read as a soft procedural layer. Small coves and wet gravel are present; the close shoreline remains a broad bare mudflat with a sharp contact, and the original lake view changes subtly. Further composition and softer shoreline detail are needed. Distant trees still read as sparse cutout symbols in some views. Mathematical LOD coverage checks improve known fade defects; static images do not prove perfect transitions in motion.

Lighting is a hemisphere/horizon approximation, with no traced GI or reflection probes. Cards remain single-view prelit billboard images, and TAA has no exact object motion vectors. The water snapshot contains terrain/sky, with rigid props and plants drawn later, so submerged props do not contribute bottom color. Waves have no geometry or fluid simulation. The planned micro-model living-world simulation remains a separate milestone.

The first native-review captures are preserved under `before-native-review`; they exposed the straight edge and stretched cloud layer and prompted the final refinements. They are diagnostics, not alternate final performance evidence.

## Try and reproduce

```sh
./Launch_Valwood_World.sh --scenic shore --width 1600 --height 900
```

Run that from the parent of `new_engine`. Use **WASD**, hold **right mouse** to look, and **H** to toggle tools. **Scenic trails** switches the five views; **Hour/Cloud** adjust lighting; **Tab** opens the reference map.

After closing the live engine, from `new_engine`:

```sh
python3 tools/benchmark_cinematic_scenery.py --expect-authored --expect-lighting --output /tmp/valwood-lighting-repeat
```
