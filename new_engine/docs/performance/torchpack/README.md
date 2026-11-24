# Purchased medieval torch integration

The user-supplied `/home/computer/Downloads/medieval-torch-pack.zip` is integrated
into the custom Valwood OpenGL engine. Five genuine source variants have near
and mid GLBs. The village uses 22 assemblies: six poles, fourteen wall torches,
and two braziers. All original 2,437 castle placements and the other three
scenery sources are preserved; the combined scene now has 5,070 placements.

`Launch_Valwood_Torch_Demo.sh` opens the detailed market street at dusk. The
ordinary castle launcher retains afternoon lighting. Both use normal walking,
NPCs, animals, scenery and the same editable H lighting controls. An alternate
start is `Launch_Valwood_Castle_Demo.sh --lighting dusk --stop training`.

## Appearance and cost

- Actual source UVs, base color, OpenGL normal, packed AO/roughness/metallic and
  glowing-fuel emission. Four shared 1K runtime images add approximately
  21.33 MiB with uncompressed RGBA8 mipmaps; source maps remain untouched.
- Emissive materials use sRGB decoding, the glTF factor and optional emissive
  strength. Generic material state resets prevent emission leaking into grass
  or unrelated meshes.
- Each source fuel socket drives three filtered, curling flame layers and
  three rising sparks. A smooth seeded 64-second signal also flickers its PBR
  light, independently of camera position. Flame shape variation is restrained.
- At most eight nearby lights contribute. Selection fades continuously at the
  eighth/ninth rank exchange and between 60–80m; disabled sources do not consume
  ranks. Each light has an 8m physical influence radius. Fixed arrays and cached
  uniform locations avoid per-frame heap work for the new torch subsystem.
- Visible flame layers use one instanced draw per active main/reflection pass,
  one 1KiB buffer, zero flame textures and zero owned render targets. Holder LODs
  remain available beyond the full 230–300m flame fade.
- Model, terrain, actor and water PBR paths receive the same local lights.
  Water also receives mirrored flame imagery. Fuel and flame emission remain
  outside indirect-only ambient occlusion. Animated flame pixels use reactive
  temporal metadata, depth-test against opaque geometry and preserve depth.
- Real supporting-wall planes and a receiving-hall volume clip local spill.
  These lights do **not** render point-light cubemap shadows; small props do not
  cast new torch shadows. Existing directional and contact shading remain on.

## Verification

Purchased source attribution, immutable source hashes and conversion steps are
in `assets/purchased/medieval_torches/PROVENANCE.json`, its `CREDITS.md`, and
`projects/valwood_world/models/purchased_torches_manifest.json`. No new license
or creator identity is inferred from the archive.

Placement evidence is in `../village-torches/`: seven source tests and 35 castle
acceptance tests pass, including detached brackets/flames, altered seeds,
clipping, stale model metadata and missing/stale project shader links. Measured
proofs include 168 real backplate samples, 54,062 source-volume tests, 1,144 route
checks and 40 actual terrain support rays. The original scene prefix is exact;
the ground-history PNG remains byte-identical.

Focused CPU tests exercise 600 simulated seconds without allocations, source
rebasing, malformed inputs, flicker bounds, disabled-source selection and 4,001
rank-crossing samples. Software and actual Radeon GL tests exercise the real
flame shaders, all ten source GLBs and exact production light functions from
all three surface shaders. They verify HDR emission, sRGB/linear handling,
finite BRDF/range behavior, independent seeds/time, opaque depth, water clipping,
indirect-buffer preservation and zero GL/resource errors. Existing model,
pose-blend, terrain/foliage lighting and water regressions also pass.

`captures/dusk/` contains untouched 1600×900 native in-game captures.
`captures/night-study/` is an earlier dark-night visual study; it is not the
final launch preset. CPU previews in the purchased-source folder are not game
captures. Native timing and whole-system Radeon memory records are under
`native/`. Timing runs use the final executable/assets, full surface lighting,
NPCs, pets, weather, shadows and temporal AA, with a 60FPS cap and 120-frame warmup.

A first afternoon hall sweep measured 58.00 FPS mean and 44.73 FPS at the 1% low.
The renderer was submitting the exterior moat and its mirror through the
opaque keep shell. A guarded interior-camera test now omits this hidden water
work. Source near/mid meshes pass 19,500 occlusion rays, boundary cases and
removed-shutter positive controls (`hall-water-proof.json`). Exterior cameras,
the entrance margin and views above the roof keep their existing water path.
The failed timing run is retained in `native/pre-hall-cull/`.

## Final native timing

RX5300, 1600×900, full scene, 60FPS cap. All four dusk cases meet the mean/1% low/p99 timing criterion. The complete castle route traverses 848.94m with no blocked movement, loading pauses, missing resources or GL errors.

| Dusk case | Mean FPS | 1% low FPS | p99 ms | Maximum ms |
|---|---:|---:|---:|---:|
| castle-route | 59.88 | 50.84 | 16.757 | 127.808 |
| hall | 59.95 | 59.62 | 16.726 | 17.101 |
| hero-route | 59.95 | 58.35 | 16.757 | 19.165 |
| market | 59.95 | 58.51 | 16.728 | 19.935 |

The full castle route has 7 of 14,734 measured frames over 22.22ms, including a 127.808ms stall. Its 88m market-street companion has no measured frames over the 45FPS budget. Some afternoon attempts also have short stalls: market 1% low 34.67FPS, hero-mid 41.12FPS; a market repeat reaches 55.13FPS. The runs do not establish a single cause for these intermittent stalls. They are retained, and `all_final_timing_runs_meet_target` is explicitly false. The frame-rate floor is not locked.

The hall optimization changes the final display image by exactly zero pixels at the same deterministic sweep endpoint, camera and lighting (`hall-image-comparison.json`). Its afternoon 1% low improves from 44.73 to 58.46FPS; its dusk 1% low is 59.62FPS.

Final whole-system Radeon samples peak at 2866.54MiB used of 3056MiB, with 189.46MiB minimum sampled headroom. This includes other applications and is not exclusive engine memory.

`verification.json` includes every final attempt. Recheck functional/native evidence with `python3 docs/performance/torchpack/verify.py`; the verifier preserves timing failures rather than filtering them. Source/asset freezes match across all final runs. The actual final demo-start screenshot is `captures/final/hero-entry-view.png`.
