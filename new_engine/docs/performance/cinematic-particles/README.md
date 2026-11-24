# Cinematic particles — implementation and acceptance

This pass adds the user's purchased particle art to the custom GL3.3 engine.
It is a reviewable visual upgrade, not a claim that the entire scene has reached
commercial AAA production quality.

## Implemented

- Purchased torch and campfire sheets:72 frames each, independently mipmapped
  in a144-layer 128×256 sRGB array. RGB is premultiplied in linear light before
  resampling. Black campfire padding adds no light; transparent white torch
  padding cannot leave pale fringes. Adjacent frames interpolate at 23.625fps.
  The grid/order and playback speed are an engine adaptation, not verified
  vendor particle-system settings.
- Four flame sheets and eight varied, velocity-aligned embers per source,
 32-source cap,384 quads/one instanced draw, unchanged 1 KiB emitter buffer.
  Camera-relative staging, per-source shared PBR flicker, wind, projected-size
  filtering and fades preserve stable motion and reflected fire.
- Seven explicit smoke sources: the Copper Lantern's chimney outlet, selected
  outdoor braziers, and three bandit fires. Six sparse world-anchored dust
  volumes occupy the tavern, street corridors, courtyard, market and camp.
  Smoke births are staggered into overlapping plumes; wind advection saturates
  within authored volumes. Fine particles fade smoothly with distance.
- Smoke/dust use a fixed 512-sprite budget,48-source capacity,24 KiB staging,
  depth-softened intersections, directional-shadow/ambient illumination,
  premultiplied sorted transparency and a distinct HDR destination. The
  original opaque depth is sampled and never attached to that destination.
- Rain varies in physical size, speed and shading. Nearby splashes have rings,
  crowns and droplets; extra crown geometry fades before the far range.
 4,096 rain + 768 splash instances and two draws remain fixed;38,400 vertices maximum.
- Moving weather and smoke fragments write temporal confidence 0. Untouched
  confidence survives exactly, preventing moving rain from entering foliage's
  temporal-history path. Smoke/dust resolve after contact/weather and before
  temporal resolve and bloom. Their cost is included in weather/whole-frame
  measurements.
- A bounded65² RG32F cache stops rain at conservative village roof/canopy caps
  and suppresses ground splashes underneath. Terrain/water support for fog
  remains separate. A cold/teleported cache waits for complete cover data.
  No roof callback occurs in drawing; at most 520 callbacks per update.
- All target allocation and first shader use are warmed before measured play.
  Stable-size walking performs no particle heap allocation, state readback or
  target reallocation. A deleted active program is detached during cleanup.

Additional GPU storage:25,166,016bytes fire mips +87,380bytes smoke mips
+8,323,200bytes smoke/dust HDR output at1360×765 +24,576bytes staging
+16,900bytes additional weather-cache channel: approximately 32.06 MiB.
The existing fire emitter buffer remains1KiB; this is not the total FX memory.
The retained startup village cover bake uses approximately 1 MiB CPU storage.

## Validation

Focused tests compile with `-Wall -Wextra -Werror`:

- `torch-regression-tests.log`: original torch material/light/emission,
  conservative bounds, deterministic motion, opaque depth, indirect-attachment
  preservation and exact 64-second/origin repetition.
- `purchased-fire-tests.log`: actual array art and GLSL, finite warm HDR,
  frame interpolation/motion, opaque blocking, water clipping and fixed caps.
- `purchased-smoke-tests.log`:283,812 authored-volume corner witnesses,
  deterministic sorted staging/rebasing, continuous distance transitions,
  finite input rejection, soft depth/opaque occlusion, untouched pixel
  preservation, temporal confidence 0, stable reuse and released GL owners.
- `weather-tests.log`: actual rain/splash GLSL, finite/shadow response,
  temporal metadata, cold/warm roof blocking, invalid-cover fallback,
  roof-independent fog, water support, fixed storage and cleanup.
- `new-assets/validate-vfx.log`: deterministic byte-exact cooker, linear-light
  premultiplication, orientation, invalid/trailing payload rejection.

These tiny GL fixtures use software GL for correctness only and make no FPS
claim. `native-optimized/` contains the sequential native Radeon acceptance with
no build, cook, tests or full hashing during timed renderer jobs. Full source
and runtime-asset fingerprints are taken only while the renderer is stopped.
Native timing includes all intervals after the declared 120-frame warmup.

Saved settings are Custom/manual 60 FPS, 1600×900 display,1360×765 render,
2048 shadow map, half-size reflections, contact shadows, fog and clouds on.
No quality reduction or automatic scale change is used to meet a timing gate.

## Purchased scenery pack scope

The new downloads were inspected without modifying the originals. Selected
fire/smoke art is imported into the running game. Buildings, props and nature
meshes remain inventoried rather than imported in this particle pass.
[Exact archive formats, model candidates and map compatibility](new-assets/ASSET_REVIEW.md)
are recorded separately. The BMP is a showcase image. The source set uses
older diffuse/specular maps, so it is not a direct metallic/roughness import.
No public redistribution license is asserted.

## Practical limits

Smoke is layered, lit, depth-softened sprite rendering. It does not implement
volumetric self-shadowing, particle collisions or smoke in planar water
reflections. Fire retains the existing reflection path. Roof cover is a
conservative architectural cap, not exact sloping roof collision or tree-leaf
coverage. Rain splashes do not affect the water simulation. These limits
remain visible work for a future cinematic-effects pass.

Launch `/home/computer/Desktop/final_engine/Launch_Valwood_Particles.sh`.
WASD walks, right mouse looks, H restores the HUD and F10 opens inspection stops.

## Performance review and terrain optimization

The first complete batch is retained in `native-final/`, including its failure:
forest 56.73 FPS average / 44.75 FPS 1% low. It is not the final acceptance.
The village route, hero, tavern, market and campfire passed in that batch.

The terrain renderer previously used only a coarse rear-facing rejection.
`world_terrain_visibility.h` now rejects a tile only if its complete generated
AABB, including the full skirt and a 4 m rounding/jitter guard, lies outside an
actual clip half-space. Main and reflected cameras use their own matrices;
shadow submissions, LODs, streaming, geometry and quality settings are retained.
An independent homogeneous-corner oracle proved 108,200 complete rejections
across 150,000 randomized cameras/boxes. No per-frame allocation is introduced.

The native fixed-time 1600×900 forest comparison changed only one channel of
one pixel by one 8-bit level across 1.44 million pixels. See
`terrain-pixel-comparison.json` and `terrain-culling-capture/`. The first
optimized forest profile passed 57.32 FPS average / 45.00 FPS 1% low in
`terrain-culling-native/`; the complete same-binary repeat is recorded in
`native-optimized/`, including all outliers.

The roof bake now starts a separate weather-cover field at 0 rather than
inheriting the sky-access field's 24 m reference floor. Only actual roof/canopy
footprints raise the cover field, so the open 22.9 m creek is not blocked by an
imaginary 24 m plane. Fog retains its original terrain/water channel.

## Terrain-culling batch (superseded)

All6 correctness checks passed, including GL/resource ownership, saved quality,
full-scene inventory and route completion. The strict45FPS timing gate did
**not** pass every scene: the hero street had intermittent frame-time spikes.
These samples remain in the result; no slow-frame removal or quality override
was used. The table reports the complete batch before the authored-depth correction.

| Case | Average FPS |1%low FPS | Worst frame ms | Frames over22.222ms |
|---|---:|---:|---:|---:|
| route | 59.89 | 46.75 | 54.818 | 22 |
| hero | 59.74 | 33.23 | 42.521 | 14 |
| tavern | 59.95 | 59.27 | 17.329 | 0 |
| market | 59.95 | 54.97 | 22.256 | 1 |
| campfire | 59.95 | 59.69 | 16.843 | 0 |
| rainforest | 57.27 | 45.23 | 22.914 | 3 |

Peak aggregate device VRAM: 2,984,001,536 bytes, including other apps.

The585.072m route reached all29 waypoints, with zero blocked/sliding/slope
steps or return error. All520 model files and30 scenery files match the
previous lighting acceptance byte hashes (`preserved-assets.json`).

`hero-repeat/` is a separate diagnostic rerun of the same executable, not a
replacement for the failed hero row. Remaining frame-pacing spikes require
further profiling; this pass does not claim a universal45FPS floor or finished
AAA production quality.

## Authored depth ordering: final street correction

The first street repeat (`hero-repeat/`) confirmed intermittent hitches in the
previous order. The village now renders its existing authored scenery once
before terrain/procedural vegetation, using the same materials, geometry,
shadow path and MRT outputs. Opaque walls can reject hidden foliage before its
PBR fragment work. Real doors/windows remain mesh openings; forest/camp order
is retained. The existing interior early-draw counters and exclusions remain
valid, and the complete authored pass is included in GPU/whole-frame timing.

`depth-order-native/`: street59.95FPS average,59.12FPS1%low,19.155ms worst,
zero frames over the45FPS budget in the first1800-frame check.
`depth-order-captures/` and `depth-order-pixel-comparison.json`: the complete
1600×900 hero, tavern interior and exterior images match the previous order
**byte for byte**, with zero changed pixels in all3views.

`native-depth-order/` is the final complete same-binary repeat after this fix.
Earlier failed datasets are retained and are not the final renderer's gate.

## Current final acceptance

**PASS**: `native-depth-order/results.json`, same executable/source as the
byte-exact village visual checks. All6 correctness and45FPS-average/1%low
gates passed at the unchanged saved settings. The test does not promise a
universal per-frame floor or whole-world coverage.

| Case | Average FPS |1%low FPS | Worst ms | Frames over22.222ms |
|---|---:|---:|---:|---:|
| route | 59.95 | 57.75 | 22.686 | 3 |
| hero | 59.95 | 55.71 | 25.864 | 2 |
| tavern | 59.95 | 59.56 | 17.576 | 0 |
| market | 59.95 | 59.73 | 16.857 | 0 |
| campfire | 59.95 | 59.71 | 16.895 | 0 |
| rainforest | 57.83 | 46.90 | 21.643 | 0 |

Across 18,600 measured frames, 5 isolated intervals exceeded22.222ms; worst25.864ms. No interval exceeded33.333ms.
The full585.072m walk reached all29 waypoints without blocking, sliding,
slope rejection or return error. `final-verification.json` confirms unchanged
520 model files,30 scenery files, all7 original protected inputs, saved
preferences, executable identity and matching source/visual fingerprints.

Final walkthrough screenshots are in `depth-order-captures/`. The purchased
campfire close-up is also visible in `preview/campfire-bandit-fire.png`; its
non-village rendering path is retained by the authored-depth optimization.

The engine is relaunched using `Launch_Valwood_Particles.sh` after all timed
profiling/verification is complete. No other native test renderer remains.
