# Whole-scene temporal stability — 28 October 2026

Corrected a color-sampling mismatch that could expose the anti-aliasing projection jitter across buildings, terrain, trees and characters. The issue predates the dragon integration and reproduces with dragons disabled.

## Cause and correction

The temporal resolve samples current HDR color at `UV + projection_jitter` to reconstruct a stable output grid. Its synthetic fixtures supplied linear-filtered color, but the actual contact-lighting, weather and particle passes produce nearest-filtered textures. The resolve inherited that nearest filtering. Tiny projection offsets therefore survived in RGB even though canonical depth was reconstructed at the corrected sample location. Low-history-confidence characters expose more of this error.

`engine/world_temporal.cpp` now owns two GL3.3 sampler objects: linear color on units 0/2 and point depth on units 1/3. They are created during loading, reused across frames/resizes and deleted at teardown. The resolve never changes borrowed input texture parameters. It unbinds its four sampler bindings before returning, including failure paths, so subsequent PBR, shadow, fog and postprocessing passes retain their existing filtering. There are no new full-screen passes, manual four-tap reconstructions, per-frame allocations or state readbacks.

A separate local issue was corrected in `engine/world_dragon_breath.cpp`: surviving breath fragments now write reactive confidence zero, matching the existing particle contract. Previously, some blended alpha values landed in the range reserved for static foliage coverage, which could cause plume/background trailing. The fire/smoke RGB blends and caller GL state remain unchanged. Inactive breath performs no GL work and was not the global shaking cause.

## Native visual comparison

The preserved old executable and final executable rendered 32 consecutive 1600 × 900 display frames per view. Both use the saved fixed 70% internal scale, identical settings, a stationary third-person observation camera, frozen wind/cloud/resident time, active eight-phase Halton jitter and disabled dragons. Background metrics use the top 500 display rows to exclude the foreground avatar's animation. These readback sequences are visual evidence, **not FPS measurements**. Every raw sequence frame is preserved losslessly as PNG; original final BMP captures and renderer metadata are also retained.

| Scene | Background luma variance before | After | Reduction |
| --- | ---: | ---: | ---: |
| Castle market | 6.8935 | 3.0353 | 55.97% |
| Wildflower grove | 40.0234 | 19.8898 | 50.30% |
| Frostshade clearing | 23.4767 | 11.7654 | 49.88% |

Values are mean temporal variance of display luma in 0–255 units. This is a reduction in rendering instability, not a claim of zero residual aliasing. Thin distant foliage still has subpixel coverage changes; ordinary play also retains wind, animated characters and changing flames. See [all metrics](comparison.json), `before/` and `after/`.

## Verification

- The new sampler regression fails against the old implementation: a nearest-filtered input differs from the same linear-filtered scene by 0.016601562 RGBA on its first tested phase.
- With the fix, five 3D/source classes over 16 frames and all eight jitter phases produce identical RGBA and canonical depth for linear/nearest input textures and hostile prebound sampler objects. Input filter parameters are unchanged and sampler bindings clear on success and invalid-matrix failure.
- Existing temporal tests pass for oblique depth planes, rigid wall/sky edges, moving actors, unsupported foliage rejection, cuts, disocclusion, resize, held output and teardown. CPU reprojection/jitter math tests pass.
- Software and native Radeon temporal fixtures pass. Native purchased-art breath tests pass weak plumes and smoke-only fringes over opaque, rigid-tagged and foliage-tagged backgrounds, with untouched depth/indirect data, restored caller state and zero GL errors. An old-alpha negative control fails the new reactive-confidence assertion.
- The first native breath-fixture attempt exposed a harness problem: it unconditionally forced software/offscreen rendering on the native host. The fixture now honors the same explicit native opt-in as other tests; its subsequent native run passes. The failed diagnostic log is retained separately and was not a game/runtime crash.
- The full engine builds. Existing unrelated legacy/editor warnings remain in `build.log`; the modified production modules introduce no build warnings.

## Native performance after correction

Radeon RX 5300; 1600 × 900 display, 1120 × 630 internal resolution, fixed quality, 2048 shadows, contact lighting, reflections, clouds and fog enabled. Normal animation/wind time runs during profiles; audio uses the dummy driver. Every interval after the initial 120-frame warmup is retained, and there are no capture readbacks, concurrent builds or tests during timing.

| Test | Retained frames | Average FPS | 1% low FPS | Frames below 45 FPS |
| --- | ---: | ---: | ---: | ---: |
| Market, rotating view: initial run | 2,280 | 58.63 | 18.30 | 45 |
| Same rotating market: repeat | 2,280 | 59.95 | 47.04 | 0 |
| Dragon clearing, dragons/normal animation active | 2,280 | 59.92 | 45.71 | 3 |
| Complete approximately 350 m jungle return walk | 7,560 | 59.26 | 45.70 | 12 |

The initial market run contained a short 10-frame burst at frames 313–323, reaching 172.5 ms, affecting update, rendering and even passive editor CPU time. The same view in later orbits and the complete repeated run did not reproduce that burst. A weather-cache boundary overlaps it, but repeats without the severe stall; current evidence cannot distinguish first-use or external scheduling causes. Both runs are retained. Do not interpret the repeat as proof that every occasional stall is eliminated.

The 1% low is the reciprocal of the mean duration of the slowest 1% of retained intervals. The repeat market, dragon and jungle tests pass the 45 FPS statistical target; the initial market does not. Individual below-45 frames remain in the dragon and jungle tests. All four functional cases pass with zero GL errors/remaining texture references, fixed graphics scale, no dragon/VFX failures, and a completed jungle route without collision/slope obstruction. Frozen runtime input metadata and saved graphics preferences are unchanged across each profile group; see `native/freeze-after.json` and `market-repeat/freeze-after.json`.

The updated interactive game is reopened at the castle market with normal audio and ordinary animation. H opens tools/destinations, V changes camera, and RMB looks around.
