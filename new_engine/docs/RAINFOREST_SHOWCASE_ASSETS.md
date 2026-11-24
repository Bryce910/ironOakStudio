# Rainforest showcase asset kit

The rainforest uses four newly downloaded Poly Haven source families, adapted
into five fixed runtime aliases. Powered by Poly Haven.

| Runtime alias | Original source | Near / mid triangles | Placement |
| --- | --- | ---: | --- |
| `showcase_roots` | [root_cluster_01](https://polyhaven.com/a/root_cluster_01) | 10,000 / 2,399 | Rigid terrain support; box collision |
| `showcase_stump` | [tree_stump_02](https://polyhaven.com/a/tree_stump_02) | 9,000 / 1,999 | Rigid terrain support; box collision |
| `showcase_shrub` | [shrub_02](https://polyhaven.com/a/shrub_02), object a | 7,590 / 1,799 | Root grounded, alpha masked |
| `showcase_shrub_b` | Same source, object c | 9,234 / 2,200 | Root grounded, alpha masked |
| `showcase_sorrel` | [shrub_sorrel_01](https://polyhaven.com/a/shrub_sorrel_01), four original plants | 2,260 / 648 | Low groundcover clump, alpha masked |

These are original downloaded model faces and photographic PBR images. The
sorrel clump rearranges four of its source plants; it is low wood-sorrel rather
than a replacement reed or a newly invented mesh. Node transforms and UV0
texture transforms are baked, horizontal origins centered, and the base of
each mesh normalized to Y=0. Vegetation retains photographic opacity masks.
There are no blended materials, animations, external runtime texture paths,
or additional texture reads introduced by this kit.

All four asset families use [CC0](https://polyhaven.com/license). The preserved
API metadata is reference material; its license is not represented as an asset
license. The download follows the [public API terms](https://github.com/Poly-Haven/Public-API),
uses an identifying User-Agent, and retains the API credit above.

`assets/polyhaven/rainforest_showcase/provenance.json` records source URLs,
byte lengths, SHA-256 values, reference/derived status, and license for each
source and prepared image. Source files remain separate from cooked GLBs.
The source kit plus prepared images occupies 26,753,824 bytes at validation.

## Bounded runtime package

The ten GLBs occupy **39,330,664 bytes**. Their twelve distinct embedded image
payloads have a conservative RGBA8 plus complete mip-chain estimate of
**56,623,088 bytes (54.00 MiB)**. Encoded GLB bytes are disk storage, not VRAM.
Each near/mid pair shares exactly the same encoded images, and both shrub
variants share their images. The engine's existing texture cache can therefore
intern them rather than allocating a new image for each GLB.

`projects/valwood_world/models/showcase_manifest.json` records each GLB hash,
triangle count, source, adaptations, and measured Y-up bounds. The companion
`showcase_manifest.summary.json` records aggregate storage and texture bounds.
Maximum image size is 2048; woody materials retain more image detail than
small groundcover. CPU Blender cooking is limited to two threads.

Showcase near/mid split distances are 65m for roots, 55m for stumps, 50m for
shrubs, and 24m for sorrel. A 4m overlap either side uses complementary ordered
coverage in color and shadow passes. The shared `uPropLodRange` uniform resets
after each scenery draw. Existing model families retain their previous LOD
selection. Opaque roots/stumps and the two taller photographic shrubs cast sun
shadows. Shrub shadows use the same alpha masks, wind and complementary LOD
coverage as their color pass. Near trees and both taller shrubs use separate
instance VBOs on their existing mid meshes for the sun-shadow pass; full-detail
color geometry and all caster placements remain unchanged. Texture images are
shared. `VALWOOD_DISABLE_SHADOW_LOD=1` is read once at scene load and selects
original shadow meshes while retaining the same loaded/uploaded resources for
a controlled comparison. Sorrel, legacy ferns and other low plant props
retain the existing policy that omits them from the sun-shadow pass.

## Reproduction and validation

From the engine directory:

```sh
python3 tools/build_rainforest_showcase_assets.py --download --prepare
blender --background --threads 2 --python tools/build_rainforest_showcase_assets.py
python3 tools/build_rainforest_showcase_assets.py --validate
python3 tools/build_rainforest_area.py --validate-only
```

The asset validator reads actual GLB buffers and verifies finite positions,
unit normals, valid indices, nondegenerate triangles, UV0, bounded triangle and
image dimensions, preserved opaque/transparent mask ranges, source/GLB hashes,
and cross-LOD image identity. Source download checks use the API's expected
size and MD5 in addition to recorded SHA-256 provenance.

The final authored source contains 972 rainforest placements, including 622
showcase placements: 36 roots, 24 stumps, 144 shrub-a, 116 shrub-c, and 302
sorrel. It adds to the exact preserved 1,639 earlier placements, yielding 2,611
records and a largest combined alias batch of 552. The runtime is bounded at
4,096 total authored records. The most recent arrival refinement adds 222
close records with 953,144 summed near triangles before culling; all 622 new
showcase instances together sum to 3,422,624 near triangles, distributed along
the 216.316m route and pool banks. These totals are geometry bounds, not a
per-frame submission or FPS claim.

The generator checks real model lower-body triangles for tree clearance,
full prop footprints, actual terrain heights and local-water samples. Trees
retain the pool bank reserve; low bank plants/rigid props may occupy its dry
feather only with conservative full-footprint ellipse clearance and dry
samples. The original six-point path remains clear, with 0.161m minimum spare
over its 3.2m strip, and both arrival and pool camera footprints are protected.
`--force` is required to regenerate editable outputs. Deterministic replay,
actual edited-file rejection at the pool, and the preserved earlier source
hashes are checked separately from renderer and performance measurements.

Correctness logs for this revision are under `/tmp/valwood-showcase-*`:
asset validation; author generate/validate/proofs; scenery CPU and ASan/UBSan
(with `ASAN_OPTIONS=detect_leaks=0`); and the raised-pool-frustum negative
control. The latter changes only a temporary copy of scenery selection and
must fail its pool-only visibility assertion. Actual-model GL and final
native performance are integrated and measured separately by the parent run.
