# Castle village model kit

This is an authored architectural kit for Valwood's custom OpenGL renderer.
The houses, rooms, roof structures, stalls, well, banner and cart were built
for this village. The keep incorporates cropped original Poly Haven fort
crenellations and its original arched gate surround. It is an initial art kit
for native review, not a claim of finished AAA scenery.

Powered by Poly Haven. Photographic stone, trim and plaster come from the
retained [modular_fort_01](https://polyhaven.com/a/modular_fort_01) source.
New material downloads are [roof_slates_02](https://polyhaven.com/a/roof_slates_02)
and [brown_planks_08](https://polyhaven.com/a/brown_planks_08), using the official
API and its expected size/MD5 records. These source assets use
[CC0](https://polyhaven.com/license); [API terms](https://github.com/Poly-Haven/Public-API)
and credit are retained in the new source provenance. The banner retains its
authored woven image. The three market-stall aliases now use purchased source
cloth and resident purchased wood PBR; their mixed licensing, source hashes
and preservation proof are documented in
[Market canopy fidelity](MARKET_CANOPY_FIDELITY.md).

No earlier source or cooked model was overwritten. New sources, prepared
images and their hashes live under `assets/polyhaven/castle_village/`.
`provenance.json` also records the hashes of the earlier gate/tower GLBs used
as read-only inputs. Its 26 retained source/prepared files total 33,647,590
bytes, excluding the provenance JSON itself.

## Models and geometry budgets

All runtime stems below have a `.glb` and `_mid.glb` file. The engine may use
only one LOD for small props. Dimensions are actual evaluated near-model
width × depth × height in meters, rounded for this table.

| Stem | Dimensions | Near / mid triangles | Draw materials |
| --- | --- | ---: | ---: |
| `village_house_a` | 9.894 × 11.970 × 12.007 | 2,802 / 1,174 | 8 |
| `village_house_b` | 11.892 × 13.970 × 14.000 | 2,802 / 1,174 | 8 |
| `village_inn` | 17.869 × 13.970 × 16.000 | 3,474 / 1,414 | 8 |
| `village_smithy` | 11.862 × 9.970 × 10.000 | 2,290 / 1,186 | 8 |
| `village_keep` | 36.000 × 28.000 × 35.000 | 26,503 / 13,020 | 7 |
| `village_stall_red/blue/gold` | 4.000 × 3.000 × 3.300 each | 864 / 384 each | 2 |
| `village_well` | 2.840 × 2.520 × 3.820 | 800 / 464 | 6 |
| `village_banner` | 1.600 × 0.258 × 4.800 | 48 / 48 | 4 |
| `village_cart` | 3.200 × 1.740 × 1.699 | 1,320 / 1,320 | 2 |

Roofs have real pitched geometry, overlapping course lips, fascia/ridge beams
and separate underside faces. Houses include foundation courses, exposed
timber posts/braces, recessed casements, shutters and hardware, chimneys,
dormers and ground-floor ceilings. Stalls have weathered source fabric,
soft sag, uneven hems and their original counters; goods are independent
supported instances. The cart has rims, spokes and cargo.
The main structural shell and its openings remain intact in the mid model;
small hardware, dormers and some roof courses are omitted. The keep's source
crenellations use bounded decimation in the mid model.

The initial 22 GLBs used **16 unique embedded
image payloads**, with a conservative RGBA8 plus full mip-chain estimate of
**82,138,432 bytes (78.33 MiB)**. After the source-cloth update, they total
**372,116,980 encoded bytes**, **28 unique embedded image payloads** and
**133,518,640 bytes (127.33 MiB)** standalone RGBA8 plus full mip storage.
The combined-scene increase is **21 MiB**: the new wood payloads already reside
on purchased architecture. Encoded disk size is not VRAM. Near and mid
variants use identical image sets; the existing texture cache can share their
images across buildings and repeated instances. Some stone payloads also
originate from the existing fort kit. No new runtime texture format or shader
feature is required.

## Placement and collision contract

Models are centered in XZ, base aligned to Y=0, with fronts facing local −Z.
Architecture ground floors end at Y=+0.02m and have no blocking entry plinth.
The retained terrain plane supplies ground-floor walking height. Upper floors
and tower tops are architectural detail; they are not implemented as new
player navigation surfaces by this asset kit.

Buildings have genuine front doorway voids: houses 1.8m × 2.6m, inn 2.4m ×
3.0m, smithy 3.0m × 3.2m and keep 4.6m × 6.0m. The manifest supplies exact
door plane/sill values and compound wall boxes, rather than an enclosing
building collider. House-family records have six wall boxes plus an interior
table; the keep has six shell pieces, four columns and four tower boxes.
Stalls use four posts and their counter, leaving canopy space open. Well/cart
have solid prop footprints and the banner has no collider.

`projects/valwood_world/models/village_asset_manifest.json` is the integration
source. Each `assets[]` entry carries its actual model hash, near/mid family,
measured bounds, `door_openings`, and `collision_boxes` in this order:

```text
[xmin, xmax, zmin, zmax, ymin, ymax]
```

The source-folder `collision_plan.json` is an earlier authoring plan. Runtime
integration must use the final evaluated manifest, including its centering
adjustments. Suggested color LOD splits are 90m for houses/inn/smithy, 180m for
the keep and 50m for stalls. The scenery module owns actual draw/LOD policy.

## Reproduction and validation

```sh
python3 tools/build_castle_village_assets.py --prepare
blender --background --threads 2 --python tools/build_castle_village_assets.py
python3 tools/build_castle_village_assets.py --validate
python3 tools/build_source_market_canopies.py --stage --publish --validate
```

The cooker runs on CPU with two Blender threads, packs images, merges material
batches, exports Y-up static GLBs and bakes UV0 transforms. It explicitly writes
standard glTF material color factors after export because Blender's MixRGB
multiply does not reliably export those constants. Colored cloth and shutters
therefore retain one shared image while keeping their intended runtime tint.

Validation reads the actual GLB vertex/index/material/image buffers. It checks
finite attributes, unit normals, valid nondegenerate triangles, bounded model
and material counts, opaque materials, the architecture's base/normal/ARM
channels, the intended color factors, and cross-LOD image sharing. All exterior
wall planes must have outward geometric and vertex normals, and large roof
faces must point upward. This is asserted on the exported mesh rather than
inferred from authoring code.

Fifteen body-height rays cross each real building entrance through the exported
triangles. An injected waist-high crossbar must make the same sweep fail. This
guards against visible-but-blocked doorways and a decorative beam crossing an
otherwise valid collision opening. The final manifest records normal-audit
counts and doorway/negative-control results for each model.

CPU cook/validation logs are `/tmp/valwood-village-assets-cook.log` and
`/tmp/valwood-village-assets-validation.log`. This asset validation does not
establish native frame rate or final visual quality; the integrated village
is reviewed and profiled separately in the engine.
