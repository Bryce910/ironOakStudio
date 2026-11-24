# Rainforest terrain and weather showcase

This revision extends the existing native C/C++ OpenGL engine. Open [the showcase launcher](../../../../Launch_Valwood_Rainforest_Showcase.sh) to arrive on foot at **Old Tanthoon | Rainforest reflecting pool**. WASD walks, right-drag looks, Shift runs, Space jumps, H toggles tools and F toggles flight. **Light and atmosphere**, above Scenic trails, exposes hour, cloud, rain, wind and mist. Zero rain retains mist; zero rain and mist bypass the weather composition pass.

[Pool render](rainforest-pool-view.png) · [Entrance render](rainforest-view.png) · [Canopy render](rainforest-deep-view.png) · [Earlier entrance](before-rainforest.png) · [Authored layout](../../../projects/valwood_world/scenery/rainforest_layout.svg)

These are captured engine frames, with BMP-to-PNG format conversion only.

## What changed

Four newly downloaded **CC0 Poly Haven families** supply root clusters, a scanned stump, two shrub variants and low wood-sorrel. Five aliases use ten near/mid GLBs with original geometry/UVs, photographic PBR images and vegetation masks. Source URLs, downloads, hashes, adaptations and licensing are retained in [the asset documentation](../../RAINFOREST_SHOWCASE_ASSETS.md) and [provenance](../../../assets/polyhaven/rainforest_showcase/provenance.json). Powered by Poly Haven.

The rainforest pocket now contains **972 authored placements**, including **622 newly added root, stump, shrub and sorrel placements**, concentrated around the arrival and pool. The two earlier files retain their exact1,639 records. Combined scenery is2,611 records / 49 shared source groups, under the 4,096-record cap; largest alias batch remains552. The added package occupies37.51 MiB encoded and 54.00 MiB of unique RGBA8/mip image payload. Near/mid images and both shrub variants share cache entries. There are no runtime downloads.

An irregular shallow freshwater pool at **Y=134 m** sculpts only a32×44 m terrain support ellipse. Collision, rendered tiles, ecology exclusion and weather support share its height field. Water uses its own raised planar reflection, depth transmission, shoreline foam, filtered rain rings and wind waves. Raised-mirror visibility and below-water clipping cover both terrain and models.

Lighting adds a separate terrain-following, shadow-lit ground mist; world-space rain and surface splashes; shared wind deformation; and bounded wet-material responses. Rain reduces temporal history to limit trails. The weather pass uses eight half-resolution integration steps,4,096 rain /768 splash instance caps and a65² surface cache staged at at most520 requests per normal update. Initial support is warmed before the first visible frame. Its image payload is13.75 MiB at 1600×900. [Weather rendering and limitations](../../WEATHER_RENDERING.md) documents the implementation.

Nine compare-before-filter shadow taps with continuous B-spline weights replace square raw-depth-filtered leaf-shadow edges. The default sun map remains2048². Four procedural and six authored near groups use separate mid meshes for shadows while retaining detailed meshes in the color view. Their exact instance batches, source opacity images and world-space wind are preserved; shadow geometry is deliberately a lower-LOD approximation. No additional unique image payload is needed. New prop LODs crossfade over an 8 m overlap. Visible scenery uses a bounded front-to-back prefix with hidden shadow casters retained.

## Final native measurements

All tests ran sequentially on the **AMD Radeon RX 5300 at 1600×900**, using normal HUD and all default visual effects. SDL used its dummy audio driver for these rendering tests. The first120 frames of each profile are discarded. No cook, build or other test ran concurrently. FPS comes from measured frame times including presentation. GPU stage counters are delayed timestamp estimates; their means are not exact additive per-frame attribution.

| Test | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames over 22.222 ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [Return walk](path-final.json) | 7,560 | 56.91 | 45.73 | 21.066 | 24.575 | 23 | 15.678 |
| [Entrance sweep](rainforest-final.json) | 1,680 | 56.90 | 43.84 | 22.377 | 24.519 | 19 | 15.066 |
| [Canopy sweep](rainforest-deep-final.json) | 1,680 | 58.15 | 48.31 | 20.310 | 22.037 | 0 | 15.128 |
| [Pool sweep](rainforest-pool-final.json) | 1,680 | 55.29 | 45.47 | 21.310 | 23.782 | 5 | 16.633 |

Across **12,600 measured rainforest frames**, **99.627%** met the 45 FPS budget. There were **47 brief budget misses**. Averages span55.29–58.15 FPS; the entrance sweep's1% low is43.84 FPS and the worst measured rainforest frame is24.575 ms. This is **not a locked45 FPS floor**. The full walk's1% low is45.73 FPS, and the pool's is45.47 FPS.

The physical return route covered **432.632946 m**, at 3.5 m/s nominal benchmark walking speed, in 7,417 walking frames /123.616667 simulated seconds. It reached all ten subsequent waypoints, returned within1mm, and recorded **zero collision blocks, slope blocks, sliding retries or terrain-loading holds**. It uses the ordinary movement/collision helpers. It is an out-and-back walk, not an 8-hour whole-world traversal test.

| Test | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames over 22.222 ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [Original starting area](regression/start-final.json) | 1,680 | 57.61 | 46.71 | 20.558 | 23.694 | 1 | 16.125 |
| [Original lake](regression/lake-final.json) | 1,680 | 59.95 | 58.38 | 16.720 | 22.616 | 1 | 13.524 |

The original start and lake retain their expected scenery, report rainforest weight zero, and each had one isolated budget miss in 1,680 measured frames. They are included rather than implying an absolute performance guarantee outside the rainforest.

## Same-build comparisons

| Test | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames over 22.222 ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [Pool sweep](rainforest-pool-final.json) | 1,680 | 55.29 | 45.47 | 21.310 | 23.782 | 5 | 16.633 |
| [Full-detail shadow meshes](shadow-full-final.json) | 1,680 | 52.59 | 43.96 | 22.444 | 24.193 | 24 | 17.737 |
| [Weather effects disabled](weather-off-final.json) | 1,680 | 57.39 | 48.37 | 20.349 | 21.606 | 0 | 15.917 |

Using dedicated shadow meshes reduces the pool's mean shadow stage from **3.380 ms to 2.256 ms**, with detailed color meshes unchanged. The weather pass itself averages **0.432 ms** there. The effects-disabled comparison also disables added wind/wetness and rain ripples; it retains the authored biome, pool and local humid lighting. It is not a comparison against the old scene.

Higher-resolution shadow-map diagnostics are retained alongside these profiles. The continuous filter gives the important visible improvement;2048 was retained for this machine's frame budget. Source/shader/asset/binary inputs were frozen for the final run: [all 247 fingerprints match](fingerprints-final.log). [Summary JSON](summary.json) contains the measured counts and route.

## Verification

- [Complete test suite](tests.log) passed. All **ten native rendering fixtures** passed: [native overview](native-correctness.log), with individual logs in[native-correctness](native-correctness/). These include actual-model drawing, source-image/cache ownership, water depth controls, color/shadow wind agreement, raised-water clipping, complementary prop LOD coverage, shadow-proxy primitive counts and image sharing, temporal AA, sky, contact lighting and the actual weather module.
- The focused pool regression checks72 radial shore sections, continuity, dry banks and route separation. **10,232 surrounding half-meter heights remain bit-identical** to pre-pool terrain. The existing rainforest test still preserves8,385 global terrain samples,8,254 excluded climate samples and all 15 settlement centers against earlier fingerprints.
- Authoring validates real geometry, dry bank footprints, both camera starts, full walking-strip clearance, deterministic replay and edited-file rejection at the pool. [Authoring proof](author-proofs.log), [asset validation](assets-validation.log), [source preservation](protected-1639-records.json), and[the broader preservation audit](preservation.json) are retained. Original35 Seldonne authored files and the supplied map remain unchanged.
- [Scenery CPU/ASan ownership tests](shadow-proxy-asan.log) pass, including missing-proxy cleanup and failed-second-upload rollback. Source image/cache owners and GL errors are zero at the end of every measured run. LeakSanitizer was disabled under this sandbox; explicit owner assertions cover the tested GL/cache teardown.
- The first weather native fixture tripped its process-wide allocation counter. Its corrected check tracks the module's calling render thread after driver warmup while camera anchors and wind change; the module owns no workers. [The initial failure](native-correctness/weather-native-initial.log) is retained. The passing result establishes zero steady calling-thread C++ allocations after warmup, not an assertion that driver/compiler workers never allocate.

## Production-quality limits

This is a stronger playable rainforest showcase, **not finished AAA art across the 100.8×56.7 km world**. Detailed authoring covers a210×235 m pocket inside the much larger biome. Repeated tree silhouettes, sparse groundcover between beds, low terrain microrelief, stippled leaf edges, distant pine/fir cards and approximate screen-space/volumetric lighting remain visible limits. Rain has fixed particle budgets, coarse terrain support away from the pool and no general overhead shelter simulation; water remains a shaded surface rather than a fluid simulation. Further art direction, vegetation species, terrain detail and broader world authoring are still needed.

Reproduce after closing the live engine, using host Radeon access and idle build/cook jobs:

```sh
make -j4
make test
python3 tools/validate_starting_native.py --output docs/performance/showcase-recheck
python3 tools/benchmark_rainforest.py --captures --output docs/performance/showcase-recheck
python3 tools/benchmark_rainforest.py --output docs/performance/showcase-recheck
```
