# Starting-area asset sources

Powered by Poly Haven.

This forest waystation kit contains **18 static GLBs from 12 new source
families**. These are selected original models and bounded mesh reductions,
with their authored UVs and PBR maps, rather than regenerated placeholder
architecture. All model/texture sources are [Poly Haven CC0
assets](https://polyhaven.com/license), suitable for modified commercial
redistribution. Source previews and API metadata are reference material,
separate from the runtime models. The public API's requested credit appears
above; [API terms](https://github.com/Poly-Haven/Public-API).

The files are in `projects/valwood_world/models/`. The `starting_` prefix is
included in every runtime stem and placement alias.

| Stem after `starting_` | Official source; author | Near / mid triangles | Near physical size, X × Y × Z in metres |
| --- | --- | ---: | --- |
| `gate` | [Modular Fort 01](https://polyhaven.com/a/modular_fort_01); Rico Cilliers | 1,368 | 7.410 × 8.616 × 2.672 |
| `wall` | Same source, solid straight wall module | 406 | 7.410 × 7.794 × 2.550 |
| `tower` | Same source, closed exterior round tower | 4,772 | 15.838 × 13.498 × 15.838 |
| `platform` | [Modular Wooden Pier](https://polyhaven.com/a/modular_wooden_pier); Rico Cilliers | 5,512 | 2.524 × 0.080 × 2.203 |
| `table` | [Wooden Picnic Table](https://polyhaven.com/a/wooden_picnic_table); Ulan Cabanilla | 9,800 / 2,200 | 2.241 × 0.746 × 3.022 |
| `crate` | [Wooden Crate 01](https://polyhaven.com/a/wooden_crate_01); James Ray Cock | 6,576 | 0.825 × 0.350 × 0.409 |
| `barrel` | [Wooden Barrels 01](https://polyhaven.com/a/wooden_barrels_01), barrel01; James Ray Cock | 10,000 / 2,400 | 0.743 × 0.917 × 0.743 |
| `bucket` | [Wooden Bucket 01](https://polyhaven.com/a/wooden_bucket_01); James Ray Cock | 5,116 | 0.367 × 0.550 × 0.342 |
| `lantern` | [Wooden Lantern 01](https://polyhaven.com/a/wooden_lantern_01); James Ray Cock | 8,273 | 0.221 × 0.530 × 0.235 |
| `axe` | [Wooden Axe 02](https://polyhaven.com/a/wooden_axe_02); Ulan Cabanilla | 2,218 | 0.051 × 0.704 × 0.226 |
| `stump` | [Tree Stump 01](https://polyhaven.com/a/tree_stump_01); Rob Tuytel | 9,000 / 1,800 | 1.425 × 0.574 × 1.590 |
| `firepit` | [Stone Fire Pit](https://polyhaven.com/a/stone_fire_pit); Sebastian Platen | 3,887 | 1.448 × 0.388 × 1.433 |
| `celandine` | [Celandine 01](https://polyhaven.com/a/celandine_01), individual a; Rob Tuytel/Rico Cilliers | 622 | 0.276 × 0.186 × 0.228 |
| `wildflower` | [Flower Heliophila](https://polyhaven.com/a/flower_heliophila), small variant; Jenelle van Heerden | 6,000 / 1,492 | 0.343 × 0.269 × 0.273 |

Primary files have no LOD suffix, e.g. `starting_gate.glb` and
`starting_table.glb`. The four reduced files use `_mid.glb`. Exact evaluated
vertex bounds, source object names, SHA-256, byte sizes and vegetation flags
are in `starting_manifest.json`. All geometry is Y-up, in metres, with its
minimum Y at zero and X/Z bounds horizontally centered. Gate/wall length is
along local X. The axe blade is broad in local YZ, thin along X.

## Walkability and adaptations

The gate has an actual arch opening, not a textured solid wall. Its clear
straight-jamb width is **3.501m**, local X ±1.7507; the arch curves above
approximately 4.2m. Post collision belongs in two boxes, X
[-3.705, -1.7507] and [1.7507, 3.705], Z ±1.336. A solid whole-gate AABB
would incorrectly block the passage. The source stone sill is 0.269m above
the normalized root; bury the foundation approximately **0.30m** for a
terrain-height walking path. The tower is a closed exterior prop, with no
ground doorway. The wall is a solid module; neither is a complete building
interior. Architecture uses foundation grounding, with loose props placed
on supported ground. The plank-only platform is a low 8cm threshold.

The lantern retains the original timber/iron frame, door, handle and candle.
Only transparent glass panes were removed because this renderer has no
sorted glass pass. The lantern and firepit do not themselves add a light,
flame, interaction or sound. Plants retain original geometry and combine
official diffuse/alpha maps into MASK materials; they use vegetation flag3.
The Heliophila flowers are small white blooms, a decorative fantasy choice,
not a claim of local botanical accuracy. All other models use flag0.

## Budgets, checks and reproduction

The complete kit totals **81,442 triangles across all LOD files**. There are
42 unique embedded encoded image payloads: approximately **140.0 MiB** of
RGBA8 pixels including full mip chains, assuming tested cache sharing.
This excludes geometry, driver overhead and scene targets. Fort modules
share all six image payloads exactly; every near/mid pair also shares exact
payloads. Most albedos are 1K, normals 512px and packed ARM maps 256px.
Architecture uses 2K/1K/512px. No source is fetched at runtime.

From the engine root, with Blender 4.0 and Python with Pillow/NumPy:

```sh
asset_python=/home/computer/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3
$asset_python tools/build_starting_area_assets.py --download --prepare
blender --factory-startup --background --threads 2 --python tools/build_starting_area_assets.py
$asset_python tools/build_starting_area_assets.py --validate
```

The CPU cook performs no rendering. Downloaded mesh/texture bytes are checked
against official API sizes and MD5s. Sources, previews and tiered images stay
under `assets/polyhaven/starting_area/`; `provenance.json` records SHA-256 and
source URLs, and `validation.json` records the final texture budget/sharing.
The standalone validator checks static GLBs, index bounds, finite geometry
and UV0, unit normals, nondegenerate triangles, supported opaque/MASK
materials, baked UV transforms, embedded image limits, exact LOD sharing and
the 160MiB ceiling. Native visual inspection, gameplay collision and frame
timings belong to the integrated scene checks; source budgets alone do not
establish those outcomes.
