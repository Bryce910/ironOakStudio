# Authored landscape and vegetation: native results

Generated 2026-10-24T05:30:58.804737+00:00. This is a further art and rendering pass in the owner's native C/C++ SDL/OpenGL engine. It improves composition and vegetation variety; it does not establish finished AAA fidelity.

## What changed

- Three bounded, locally shaped landscape districts: Ulasse's asymmetric crown/headwall with gullies and buttresses; the Valwood clearing and trail valley; and four Crosswind headland spurs with incised coves. The reference-map water and fifteen settlement foundations remain intact.
- A real 2,282 m, 3.2–3.7 m wide forest trail, shared by terrain, material and plant-exclusion logic. Its maximum measured one-meter grade is 0.152. The first 350 m has deliberate framing and understory compositions.
- 283 editable placements: 26 trees, 74 rocks, 168 ferns, 12 logs and three coastal outcrops. Rigid props use sampled actual footprints for grounding. Independent dense sampling verifies all 89 rigid props, including a negative control for the previous floating-cliff placement.
- Four tree families: mature pine and fir, rounded broadleaf and young pine. Eight additional source-derived GLBs bring the cinematic set to 21. Mature crowns were recooked into finer, individually oriented photographic shoots; new near/mid/far species variants use actual height for projected-size fading. These are runtime adaptations of CC0 source assets, with practical geometry/texture budgets.
- Rock, meadow, moss and forest litter follow authored masks. Foreground grass now combines photographic clumps with shorter curved fill; rock faces reject unsuitable grass/trees. Macro-scale rock material blending reduces the previously visible distant quilting.
- Depth-reprojected temporal AA uses eight-sample jitter, neighborhood clamping, disocclusion/camera-cut rejection and reactive history for moving vegetation/water. Held views keep matching resolved color/depth. Main views animate wind, water and clouds during performance tests.
- Conservative local/horizon visibility caching avoids repeated selection work within the existing culling margins. Streaming still pumps and publishes every frame; draws, wind and temporal sampling continue. New fields, cumulative camera motion, aspect changes and cuts invalidate selections. Rotation thresholds shrink for wider aspect ratios.

See [pass plan](../../AUTHORED_LANDSCAPE_PLAN.md), [terrain implementation](../../CINEMATIC_TERRAIN_SHAPING.md), [editable scenery and TAA limits](../../AUTHORED_SCENERY.md), and [asset sources/reproduction](../../CINEMATIC_ASSET_SOURCES.md).

## Final controlled measurements

Actual renderer: **AMD Radeon RX 5300 (radeonsi, navi14, LLVM 20.1.2, DRM 3.64, 7.0.0-38-generic)**, OpenGL 4.6/Mesa 25.2.8. Visible 1600×900 windows with the world tools panel, VSync at 60 Hz, normal on-foot eye height. There was one engine process and no concurrent cook/test workload. Each run excludes 120 warm-up intervals. The forest/lake run for 3,600 frames each; ridge/coast for 1,800; the 15-landmark tour for 5,400. Camera motion follows the existing accelerated out-and-back sweep (about 170 m per 12 s), rather than ordinary 3.5 m/s walking. Time-dependent shaders advance at frame/60 for reproducibility.

**15,600 measured intervals; weighted mean 59.95 FPS; 99.949% within the 45 FPS budget.** 8 intervals exceeded 22.222 ms; worst 23.508 ms. No dropped profiler samples. These are short, VSync-limited regression checks rather than an uncapped headroom or universal 45–60 FPS guarantee.

| Route | Measured intervals | Mean FPS | 1% low FPS | p99 ms | Worst ms | >22.222 ms |
|---|---:|---:|---:|---:|---:|---:|
| Forest | 3,480 | 59.95 | 57.78 | 16.751 | 23.508 | 2 |
| Lake | 3,480 | 59.95 | 59.09 | 16.722 | 23.117 | 1 |
| Ridge | 1,680 | 59.95 | 57.25 | 16.725 | 23.035 | 2 |
| Coast | 1,680 | 59.95 | 57.91 | 16.730 | 22.985 | 1 |
| Tour | 5,280 | 59.95 | 58.40 | 16.752 | 23.128 | 2 |

Each route's JSON, complete interval CSV, world counters, native log and capture remain beside this report, for example [forest JSON](forest-final.json), [forest CSV](forest-final.json.csv), [forest world counters](forest-final.json.world.json) and [forest log](forest-final.log). The same names apply to `lake`, `ridge`, `coast` and `tour`.

### Repeated pass and limitations of the comparison

The first complete animated pass is preserved under [before-selection-cache](before-selection-cache/NOTE.txt); all five means were 59.95 FPS, with 10 of 15,600 intervals over 22.222 ms and a 24.540 ms worst interval. The table compares that pass with the final cache build. Both complete sets are retained, including their slow frames. This is one sequential pair; scheduling noise and VSync/driver waits prevent attributing every tail change to the cache.

| Route | Earlier budget misses | Final budget misses | Earlier raw update ms | Final raw update ms |
|---|---:|---:|---:|---:|
| Forest | 2 | 2 | 3.878 | 3.167 |
| Lake | 3 | 1 | 3.619 | 2.938 |
| Ridge | 1 | 2 | 1.804 | 1.398 |
| Coast | 0 | 1 | 1.178 | 1.048 |
| Tour | 4 | 2 | 2.446 | 1.852 |

Raw update cost is the CPU section containing streaming/visibility work. The editor/render submission sections can include driver blocking; a longer editor interval does not by itself establish a UI algorithm bottleneck. GPU timestamps are delayed smoothed estimates, not exact attribution for an individual slow interval. Reflection GPU time is included in total render GPU time. Legacy unused stage/static-asset fields in these profiles are zero and do not represent absent work or complete VRAM use.

| Route | Mean total render GPU ms | Reflection GPU ms | World startup s | Held-view frames | Longest hold frames |
|---|---:|---:|---:|---:|---:|
| Forest | 12.091 | 0.000 | 5.325 | 0 | 0 |
| Lake | 12.609 | 1.118 | 5.163 | 0 | 0 |
| Ridge | 12.254 | 0.000 | 5.191 | 0 | 0 |
| Coast | 11.966 | 1.131 | 5.218 | 0 | 0 |
| Tour | 8.121 | 0.311 | 5.233 | 1911 | 265 |

The four local runs have zero held-view frames. The tour jumps between distant landmarks and intentionally retains the last valid image while terrain prepares: 1,911 held frames, longest 265 (about 4.42 s at 60 Hz). These counters include warm-up. Tour FPS includes these held views and therefore does not mean every interval presents a fresh scene. The tour GPU average also includes held views and is not an active-rendering cost alone. World startup measures preparation outside the main interval window; the profiler's separate startup field is not end-to-end launch latency.

## Resource bounds and correctness

The world remains 100.8×56.7088 km; at 3.5 m/s the reference width corresponds to eight hours. This calibrates distance/speed and does not validate a continuous eight-hour traversable land route or an eight-hour performance soak.

- Terrain retains 252 tile records, 64 cells per tile and 2 m nearest spacing. Terrain GPU estimate is 50.947 MiB; maximum observed CPU tile memory is 40.839 MiB. These exclude atlas, materials, scene assets and framebuffer allocations.
- Local residual/material fields add 6.02 MiB to the CPU atlas (about 14.48 MiB total atlas storage), plus about 3.01 MiB for the GPU authored-mask array. CPU field generation is staged. Height sampling and streaming tests cover actual mixed-LOD skirts and negative coordinates.
- TAA's two RGBA16F/R32F history sets cost about 32.96 MiB at 1600×900. New species/deadwood unique images are estimated at 157.3 MiB including RGBA8 mipmaps under the current uncompressed cook; shared near/mid payloads are reused. These figures are component estimates, not a full GPU-residency measurement.
- Limits remain 46,000 short-grass roots, 10,500 photographic grass instances, 100,000 cached horizon roots, 8 horizon buckets of 3,500 instances and 768 authored records. All final observed counters stay within these limits. Tree counts are submitted instances before shader alpha/projected-size rejection. `authored_largest_batch` is the last update's snapshot, not a run peak.
- All five runs report zero failed terrain tiles, GL errors and remaining texture-cache entries/references at teardown. Release build, strict changed-module compilation, full `make test`, actual-asset native fixtures and the documented sanitizer/negative-control checks passed. [Validation transcript](validation.txt) records their scopes. [Preservation](preservation.json) compares the original map and Seldonne baseline; [build manifest](build-manifest.json) fingerprints this result.

## Native visual review

These four on-foot stills show the final authored/rendering features with tools hidden. They precede only the CPU visibility-cache change, which does not change asset/shader content. The final moving-camera benchmark captures are retained separately under `*-final.png`; they show the last sweep position and tools panel.

| Forest trail | Ulasse lake |
|---|---|
| [![Forest trail](forest-view.png)](forest-view.png) | [![Ulasse lake](lake-view.png)](lake-view.png) |
| Mashito ridge | Crosswind headland |
| [![Mashito ridge](ridge-view.png)](ridge-view.png) | [![Crosswind headland](coast-view.png)](coast-view.png) |

The forest has stronger framing, a readable trail and denser layered ground cover; rock materials are less visibly tiled at distance, and coastal props now meet terrain. Remaining production gaps are visible: straight trail sections with overly even edges, repeated grass/rock arrangements, isolated distant tree cards, smooth slopes needing talus/erosion detail, uniform lake banks, sparse coastal composition, simple water/sky and approximate indirect light. TAA has no object motion vectors, so moving-leaf history is approximate and needs broader motion review. Native stills and synthetic AA tests do not certify freedom from ghosting in all gameplay.

`forest-initial.*` is an early diagnostic with TAA unavailable due to missing project shader links; it is not final evidence. `forest-taa.*` and `ridge-macro.*` are intermediate diagnostics. The complete preceding scenery baseline remains under `before/`; earlier frozen-time benchmarks are historical comparisons with different animation conditions.

## Try it

From the project directory above `new_engine`:

```sh
./Launch_Valwood_World.sh --scenic forest --width 1600 --height 900
```

Use **WASD** to move, hold **right mouse** to look and **H** to toggle tools. The **Scenic trails** controls switch forest/lake/ridge/coast; **Tab** opens the world map. Editable placements load from `projects/valwood_world/scenery/authored_instances.txt` on the next launch. This file workflow does not yet supply a scenery painting UI, prop collision, complete gameplay areas or the planned micro-model world simulation.

Reproduce the controlled test after closing the live window:

```sh
python3 tools/benchmark_cinematic_scenery.py --expect-authored --output /tmp/valwood-authored-repeat
```
