# Purchased environmental assets — implementation and acceptance

The user's four local archives now supply actual runtime meshes in the custom
OpenGL engine. This is a curated, placed selection of 18 families, rather than
an import of every mesh in the archives. Originals are preserved separately.

## Implemented scenery

- Nature: oak, beech, two shrubs, two grass clumps, fern, reed, clover and palm.
- Furnishings: barrel, chest, bench, bucket, intact/broken fences, roofed well
  and stone storehouse.
- 36 embedded GLBs, with a near/mid pair for every selected variation.
- 801 authored additions: 638 grass/clover/fern/reed, 103 shrubs/palms,
  36 trees and 24 furnishings. The original 5,859 placements remain byte-identical;
  the seventh scenery source brings the rendered total to 6,660.
- Compositions include castle gardens, tavern/residents' greenspace, outer
  creek banks, six tree groves and rainforest understory pockets. Streets,
  actor/pet routes, crop plots and the drawbridge remain open.

## Asset preparation

`tools/import_bep_nature.py` and `tools/import_bep_props.py` safely select source
members, preserve their original bytes, inspect meshes with script execution
disabled, and export isolated LODs. Source FBXs bundle overlapping LOD objects;
only the selected mesh is included in each runtime file.

Diffuse/normal/AO maps are explicitly remapped from obsolete source paths.
Legacy Spec maps are preserved as source data; no unsupported automatic
Spec-to-Roughness conversion is used. Roughness follows source material values,
metallic remains zero. Vegetation uses masked coverage and double-sided leaves.
UV0 and metre scale are baked for the native loader. Source Blend reference
inspection confirms the tree/shrub/palm atlas families.

Thirty-nine image payloads share bytes across near/mid/shadow assets. Estimated
RGBA8 texture storage including mips is 48 MiB for Nature and 30 MiB for props.
Geometry totals across the 18 distinct near assets are 51,501 triangles; the
18 mids total 19,469 triangles. These are per-model inventory totals, not the
entire scene's submitted triangles.

The user identifies these as purchased assets. The ZIPs contain no separate
license document; no public redistribution permission is asserted.

## Runtime and stability

New aliases use bounded instanced groups in `world_scenery.cpp`. Existing
three-LOD trees retain their original behavior. New two-mesh trees use the
stable complementary prop LOD interval; their shader LOD is -1, preventing the
legacy 265 m billboard handoff from fading out the new mid mesh. All new
families fade before the end of their draw range. Fade uniforms reset after
every filtered/full submission, preserving other renderer producers.

New plants retain rooted wind, stable alpha sampling, normal maps, existing
foliage lighting, and terrain/reflection visibility. Tree collision uses trunks;
rigid furnishings use actual rotated footprints. The original render-order,
particle, temporal, water-preparation and terrain-culling optimizations remain.

## Validation

- Importers verify finite positions/UVs, valid indices, nondegenerate triangles,
  normalized normals, embedded shared textures, LOD budgets and bounds.
- The placement generator checks the union of both LOD bounds, dry 5x5 footprint
  samples, terrain relief, old rigid structures, public routes, farm plots and
  tree separation. Independent text round-trip validation passes.
- `scenery-tests.log` contains production CPU/GL validation: seven sources,
  6,660 actual records, 2,342 castle-route capsule sweeps and 1,305 rainforest
  sweeps. New trees submit their mid mesh at 300 m, use shader LOD -1, and reset
  optional fade uniforms. Cache owners return to zero.
- `static-check.log` verifies all 551 protected pre-existing assets/preferences,
  actual new inventory, model imports and authored placement round trip.
- `project-tests.log` verifies the new inspection names are accepted by launch
  parsing. `build.log` records the successful native build.

Native visual and performance results are recorded separately in evidence
subdirectories. Failed/preliminary captures are retained and are not FPS proof.
The acceptance helper preserves saved Custom85% manual60FPS settings, full
population, 2K shadows, reflections, contact shadows, volumetric fog/clouds,
weather and particles. Every measured interval after 120 warmup frames is retained.
No cook, build or fingerprint scan runs during the timed renderer.

## Walk it

Run `/home/computer/Desktop/final_engine/Launch_Valwood_Environment.sh`.
It starts inside the southwest castle-town garden with the new furnishings.
WASD moves, right mouse looks, H toggles the HUD and F10 cycles inspection
locations. New scenic IDs are `castle-purchased-gardens`,
`castle-creek-garden`, and `rainforest-purchased-understory`.

## Additional performance work

The initial `native-final` batch passed rendering/collision correctness but
missed the 45FPS 1% low gate in the entrance and rainforest views. These failed
measurements remain in place. `native-forest-depth` is an intermediate draw-order
experiment, also retained with its missed gates.

Authored-first submission now also applies inside the rainforest. It lets the
same rocks, trunks and plants reject hidden terrain/procedural foliage. A
fixed-time undergrowth comparison changed only 36 of 1,440,000 pixels (9 pixels
by more than 1 LSB, maximum 5 LSB); the geometry, materials and draw count remain
unchanged. `forest-order-preview` contains the inspected captures and
`forest-depth-image-comparison.json` records the exact comparison.

The completed scene now calls asynchronous `glFlush()` before UI GL-state
handling. This submits pending GPU commands earlier without a GPU-completion
wait. Empty UI rendering is retained. `early_gpu_submission` reports one
submission per frame and explicitly records no completion wait.

On this build, `native-early-submit` passes all three targeted cases:
entrance 59.95 FPS / 56.18 FPS 1% low, undergrowth 58.46 / 45.30 and pool 57.54 / 45.84.
A few isolated intervals exceed 22.222 ms; this is not a universal 45 FPS floor.
The complete route and three remaining regression cases are recorded separately
under `native-regression`. Their source freezes must match before combining
results; no failed measurements are omitted from their own datasets.

## Final measured result

`final-acceptance.json` combines seven distinct cases from `native-early-submit`
and `native-regression`, with identical complete runtime fingerprints and saved
preferences. There are 21,120 rendered / 20,280 measured frames. All seven
rendering, resource ownership, asset inventory and movement checks pass.

| Case | Average FPS | 1% low FPS | 45FPS average/1% low gate |
|---|---:|---:|---|
| Castle entrance | 59.95 | 56.18 | Pass |
| Rainforest undergrowth | 58.46 | 45.30 | Pass |
| Rainforest pool | 57.54 | 45.84 | Pass |
| Complete castle walk | 59.85 | 41.73 | **Miss** |
| Market | 59.95 | 52.85 | Pass |
| New garden | 59.95 | 59.76 | Pass |
| Outer creek | 59.95 | 59.13 | Pass |

The strict seven-case performance gate is **not met**. The complete walk has
45 of 10,200 measured intervals above 22.222 ms, including 8 above 33.333 ms;
the maximum is 56.288 ms. It finishes all 29 route legs, with no collision,
slope or sliding blocks and zero return error. Average performance is strong,
but the transient route hitches remain unresolved. No slow interval was
removed or hidden, and graphics quality was not lowered to meet the gate.

Peak aggregate observed VRAM is 2,984,296,448 bytes, including other desktop
applications. Every native case retains all effects, 140 residents, masked
materials, weather and particles. There are 353 shared texture entries and zero
unmanaged duplicate uploads/cache leaks/GL errors. `final-verification.json`
checks the measured executable/runtime, all original 520 model files and 30
scenery files, and unchanged graphics preferences. `final-preview` uses the
same final runtime. Its three inspection cameras have independent source-OBB
capsule clearance checks in `camera-clearance.json`.
