# Walking-demo finishing pass

## Scope delivered

- An additive 582-placement finishing layer: 382 castle-district details and
  200 rainforest plants, stones and ground pockets. The world now reads eight
  authored scenery sources with 7,242 placements. Original scenery/model assets
  and saved graphics preferences are preserved.
- A connected Bakers arcade and district-wide additions of 28 houses, eight
  canopies, measured facade attachments, counter goods, deliveries, resting
  places and planted edges. The full 108.206 m arcade branch has a three-lane
  dry, collision-clear proof; acceptance walks the entire branch out and back.
- Local scene-reflection cubemaps and diffuse SH lighting for six castle and two
  rainforest domains, captured only while loading. These are ordinary mipmapped
  reflections and SH diffuse lighting; they are not a full global-illumination
  or roughness-prefiltered reflection solution.
- More irregular freshwater banks and sheltered water treatment, layered
  rainforest planting, bounded weather/particle effects and lower-wall material
  weathering. The creek remains outside the outer town wall.
- Eight procedural ambience layers, twelve distance-driven footstep variants,
  shelter/region transitions and focus muting. These are synthesized ambience,
  not recorded dialogue or a professional location sound library.
- An eight-stop F9 tour, F10 graphics controls and a lazily loaded Tab atlas that
  also works with the tools hidden.

## Performance and material work

The native GPU is an AMD Radeon RX 5300 with approximately 3 GiB VRAM. The saved
profile remains Custom, 85% internal render scale at 1600 × 900 output, manual
60 FPS target, 2K shadows, half-resolution reflections, contact shadows,
volumetric fog and clouds. No automatic quality reduction was enabled.

The renderer keeps its validated exact-sized instance upload policy, caches
uniform lookups without hot allocations, binds complete shared fallback
textures and prepares actual main-MRT material variants during loading.

Offline BC7 blocks use a pinned MIT-licensed bc7enc16 encoder for eligible opaque
base/emission maps only, at their original dimensions and complete mip chains.
Masked foliage, normal/ARM maps and facade data maps retain their original
uploads. Original source files remain authoritative and unchanged. Runtime
checks reject invalid caches and fall back to original textures without stale
GL errors. There is no gameplay-time encoder.

The native scene loads 62 validated block textures in the castle, saving
473,233,712 bytes versus their RGBA reference chains. A post-frame GL inventory
measured approximately 2,025.8 MiB of named resource storage with the atlas closed,
versus 2,455.7 MiB in the earlier uncompressed inventory. Named-object estimates
exclude driver-private storage, allocation padding and the desktop. Opening the
full-resolution atlas adds approximately 71.8 MiB. The fixed-time street render
compared with original-texture rendering measures 55.72 dB whole-frame RGB PSNR;
independent source/mip colour and exact-alpha gates also pass.

## Verification

- Final build and CPU tests: uniform ownership/allocation behavior, procedural
  audio cadence, probe math, project validation and 545 cache-parser checks pass.
- Software GL regressions: actual material MRT/depth parity, model/cache policy,
  resident and pet pose banks, torches and instance-stream geometry/ownership
  pass. Fixtures retain strict cache reuse, exact data-map and reference checks.
- Native texture regression: 71 real mip chains, original-source identity,
  dimensions, malformed/missing caches and a deliberately failed GL upload
  pass; the latter cleanly falls back to the original texture.
- Native tour: all eight destinations and return pass, both probe banks retained,
  48 probe face readbacks only during loading, zero walking bakes, zero GL errors,
  zero failed terrain tiles and complete resource cleanup.
- Map integration: real Tab events open, close and reopen the same retained
  image exactly once while tools remain hidden. Actual PulseAudio startup and
  240 audio updates pass. Auditory quality still benefits from user listening.
- Final full eleven-case frame-time acceptance: **PASS** at the saved settings.
  All cases pass correctness, average >=45 FPS and slowest-1% >=45 FPS gates.
  Every post-warmup CSV interval is retained; saved preferences and frozen
  runtime inputs remain unchanged. See `native-cooked-final/results.json`.

| Case | Average FPS | Slowest 1% FPS | Worst frame ms | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| Full 585 m castle walk | 59.95 | 57.12 | 25.347 | 3 |
| Full 216 m arcade return walk | 59.95 | 57.35 | 22.270 | 1 |
| Arcade sweep | 59.95 | 59.32 | 17.670 | 0 |
| West street | 59.95 | 59.18 | 18.886 | 0 |
| East street | 59.95 | 59.63 | 16.965 | 0 |
| Market | 59.95 | 59.48 | 17.813 | 0 |
| Tavern | 59.95 | 59.07 | 19.121 | 0 |
| Outer creek | 59.95 | 59.33 | 17.538 | 0 |
| Castle garden | 59.95 | 59.28 | 18.264 | 0 |
| Rainforest pool | 57.02 | 49.51 | 21.193 | 0 |
| Rainforest understory | 58.33 | 47.31 | 29.528 | 1 |

There are 29,160 measured frames and five isolated frames over the 45 FPS budget.
This passes the average/slowest-1% acceptance gate; it does **not** establish
an absolute 45 FPS minimum for every individual frame. The previous multi-hundred
millisecond pauses did not recur in this full candidate suite. Historical failed
measurements remain in their original evidence directories.

A separate five-view repeat with a newly isolated driver shader cache also
**passes** correctness, FPS, frozen-input and preference gates. It preserves the
user's existing driver cache and settings. The first launch recorded 97 shader
cache misses. All 8,400 post-warmup frames stay within 22.222 ms; see
`native-cooked-cold-repeat/results.json` and `driver-cache-variant.json`.

| Fresh-cache repeat | Average FPS | Slowest 1% FPS | Worst frame ms |
| --- | ---: | ---: | ---: |
| Market | 59.95 | 58.37 | 18.279 |
| Arcade sweep | 59.95 | 59.78 | 16.764 |
| Tavern | 59.95 | 58.99 | 18.103 |
| Rainforest pool | 57.05 | 49.51 | 20.427 |
| Rainforest understory | 58.30 | 47.71 | 21.449 |

Across both suites, all 16 cases pass and 37,560 measured frames include every
hitch. The full suite's read-only process sampling measured 805.36 MiB peak RSS
and zero process swap. Runtime/settings fingerprints match between suites.
The acceptance helper itself recomputes timings from complete CSVs; the report
and its claims also received an independent read-only review.

## Handoff

Use `Launch_Valwood_Demo.sh` in the parent workspace. The interactive launch uses
the same executable and saved profile, with normal time, real sound and no QA
flags. Controls and the eight tour stops are in `docs/DEMO_WALKTHROUGH.md`.
Final fixed-time images are in `cooked-visuals/`; map/actual-device integration
is in `cooked-smoke-map-fixed/`, and the return tour is in `tour-return/`.

This is a custom-engine visual-demo iteration. The screenshots do not establish
commercial AAA visual parity; foliage density/variety, broader terrain art,
character presentation, sky detail and a production sound pass remain areas for
further art direction.
