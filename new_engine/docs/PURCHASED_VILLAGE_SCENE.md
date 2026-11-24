# Purchased village assets

Valdoa uses all six purchased house families throughout its 51 residential
parcels and all four shop families across nine market booths. The castle court,
outer walls and two closed side portcullis gates use detailed source masonry.
The layout contains 1,305 village placements and 3,916 combined placements,
below the 4,096 runtime cap. All 2,611 pre-village records remain byte-identical.
Forty large barrels, 91 crates and 234 source food/tableware objects
supply the market and shopfront deliveries.

Merchants Lane is a deliberately composed east-side trading street at local
`x=61.75`, from `z=-27` to `49`. Eleven staggered frontages frame 5.8 metres
of paving with a matching grass mask. Rooflines range from 9.27 to 16.89 metres;
the minimum opposing eave gap is 6.787 metres. Its southern approach now has
distinct cooper deliveries, clothier warehousing, an inn tasting table and
bakery/brewer supplies. Stacked crates, bread and cheese, smaller casks,
produce sacks, firewood and weathered timber lamps create different silhouettes
along both edges, with most new street cargo in the first 25 metres of the view.
The market also receives two separate provisioning pockets. A final close-market
pass adds 103 objects within 35 metres of its scenic camera: irregular delivery
stacks and mixed food, a cart, an inn tasting/seating group with three timber
stools, smaller counter supplies and 44 mixed ferns/flowers in broken verge
islands. Earlier sack rows were staggered into loose working clusters.

Three red-and-gold cloth rows cross the lane. Their rope endpoints are solved
against actual façade triangles, rather than guessed from bounding boxes.
Eighteen source cloth pieces hang from 78 short modular rope segments; the
lowest cloth remains 4.833 metres above the walking surface. The flags are
static, two-sided source meshes with alpha coverage; dynamic cloth simulation
and illuminated lamp emitters are not claimed. The ten timber street lamps
retain their source metal and wood PBR materials.

The six named dressing pockets contain 183 grounded, planted or support-linked
objects. Seven isolated purchased prop families add 14 near/mid GLBs, each with
one material primitive. They share 21 encoded PBR images across LODs and use
26.688 MiB conservative RGBA8 texture storage with mipmaps. The source FBXs
remain untouched; the rigid modular rope is rotated in a generated bake copy
so native yaw can span the street. Four-side neutral previews are retained in
`assets/purchased/village_props/cooking/previews/material-fidelity-before/`.
This is a native authored arrangement inspired by the vendor image, not a claim
to reproduce the Unreal demo map.

The lane's 86 metre walking route connects the two public cross streets.
Real solid footprints leave a minimum 1.168 metre margin beyond the player
capsule on the centerline, with 177 flat ground checks. The two NPC return
strips remain clear. Four existing travelers circulate through the lane and
eight others browse between the market columns on separate 1.8 metre return
strips, while the total population remains 64. The original 806 metre main route retains
its 5.530 metre minimum margin. A scenic view is recorded at `(61.75,47)`,
looking toward `(61.4,-18)`.

The builder checks 68,364 height-aware cargo-volume comparisons, 28,078
purchased building footprint comparisons, 1,215 real-mesh support rays and
1,185 same-surface product pairs. Named support contact is permitted only after
five actual surface intersections; unrelated intersecting cargo is rejected.
Negative fixtures reject overlapping cargo, low overhead cloth, an obstructed
walking line and unsupported tabletop goods.

The original keep remains at `(0, -145)` in village coordinates, with its
coherent complete silhouette, existing open doorway collision and original
placement. The vendor describes the purchased `SM_Castle.FBX` as a background
asset, with no interiors. Its disconnected elevated turret and wall masses are
part of that source composition, not a conversion defect. The cooker retained
all 6,657 source render triangles and applied one global affine transform.
Its converted GLBs and originals remain available for a suitable distant
backdrop placement; the live walkable court retains the complete original keep.

The closed west/east portcullis towers replace two old side towers; adjacent
masonry segments meet their measured gate sides. The southern player entrance
and central castle court entrance retain the existing open gates. No
terrain/world scale changed, and the existing 806m route remains clear.

The six generated coloured stall GLBs retain their bins, tables and colliders
with the earlier cone produce removed. They now use purchased cloth and wood
PBR, smooth restrained sag and uneven hems, at two primitives each. Exact
countertop/support geometry and collision boxes are preserved. See
[Market canopy fidelity](MARKET_CANOPY_FIDELITY.md) for memory, hashes and limits.
The earlier cone-bearing files remain in
`assets/purchased/market_goods/before-produce-replacement/`; the immediate
pre-cloth-update files are in `assets/purchased/market_canopies/before/`.

The editable source is
[`purchased_village_details.json`](../projects/valwood_world/scenery/purchased_village_details.json),
referenced by `castle_village_layout.json`. It records named house replacements,
new shop/table positions, masonry and cargo replacements, and goods attached to named counters or tables. The
builder casts five rays against each real parent mesh to verify product support,
checks actual model footprints and roads, then emits the ordinary instance file,
shared terrain/ecology include, collider inventory, manifest and SVG layout.

```sh
python3 tools/build_castle_village.py --force --require-models
make -j4
```

The isolated street prop manifest is
[`purchased_props_manifest.json`](../projects/valwood_world/models/purchased_props_manifest.json).
It records original source hashes separately from generated bake-input hashes,
source material/tiling mappings, actual triangles and bounds, and shared image
identities. Regenerate only these props with:

```sh
python3 tools/build_village_props.py --prepare
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
  --python-exit-code 1 --python tools/build_village_props.py -- --cook
python3 tools/build_village_props.py --validate
```

The full layout and its route/support proofs are recorded in the generated scene manifest. Goods have no blocking collider. Purchased buildings, castle, closed side gates, walls and cargo use their actual
loaded model boxes; they do not inherit the prototype houses' doorway openings.
The imported houses have closed, non-walkable interiors. Canopy collision is
currently conservative over its full footprint. The blue canopies use 15cm
burial because the source pole bottoms differ by approximately 13cm; their
separate tables and products retain the measured tabletop heights.

Every detailed wall is assigned to one of 12 measured straight runs. Panels
meet end to end rather than inheriting the old prototype blocks' overlapping
spacing. The builder checks all 151 adjacent joins and 1,828 collinear panel
pairs using actual GLB bounds, plus 20 tower/open-gate connections. Four side
portcullis joins use actual low-height mesh sections; the source parapets
project beyond their lower stone piers and cannot define the wall endpoints.
The maximum panel gap is 0.000000343m, with no positive interior overlap beyond
floating-point roundoff. Each straight run retains a uniform wall-top height.

House D's source contained six near-coincident outward plaster triangles,
separated by only 4.77 micrometres but using different atlas UVs. Both LODs now
retain the outermost source face and remove the redundant six triangles. Its
retained UVs, normals, tangents, positions, image bytes and measured bounds are
unchanged. Before GLBs and proof remain in
`assets/purchased/medieval_village/geometry-audit/duplicate-plaster-repair.json`.
The architecture cooker applies the same deterministic cleanup on regeneration.

The source archives were supplied by the user as purchased content. Originals
remain untouched in Downloads, and selected source files/provenance remain in
`assets/purchased/`. No free or Creative Commons license is asserted for these
imports. The original purchased source files and all other prototype model bytes
are preserved. The six generated coloured stalls received the separate
cone-removal and purchased-source cloth updates documented above.

## Model budgets

Architecture details, material conversion, measured bounds and source mapping
are in
[`purchased_village_manifest.json`](../projects/valwood_world/models/purchased_village_manifest.json).
Its 15 converted near/mid families (30 GLBs) use 447.92 MiB of conservative RGBA8
image storage including mipmaps, under the 544 MiB architecture budget.
Fourteen families are active (432.17 MiB); the
15.75 MiB castle composite is retained for source review and not loaded.
All six near houses share original source-UV wood/plaster PBR tiles and red
paint/mask layers. Only residual decals/windows/metal retain 1K/512/256 atlases.
Mid houses retain their previous maps and geometry byte-for-byte. Close-market
shops retain 2K/1K/512 maps; other shops, gates and walls use 1K/512/256 maps;
the castle uses 1536/768/384 and small transport props 512/256/128.
Near houses retain original repeating roof/stone UVs with shared source PBR
maps; source façade UVs, tangents and paint are preserved as well. Near models
use one to six primitives; all mid LODs retain one primitive and their previous
corrected façade atlas byte-for-byte. Source façade density improves 19.7–36.9×
on A–D relative to the prior repack; all shared image bytes remain original
prepared PBR data. The source HeightLerp reconstruction remains approximate,
and this material pass does not add POM or subsurface rendering.
Source material-instance UV tiling and the actual RGB/vertex-paint channels are
restored. Thatch bulk remains opaque; its fringe alone uses source masked
coverage. Source-derived albedo, tangent normal, AO, roughness and metallic
channels are retained; lighting is not baked into base colour.

See [material fidelity and validation](PURCHASED_VILLAGE_MATERIAL_FIDELITY.md)
for source evidence, measured texel density, LOD palette checks and the remaining
bounded HeightLerp approximation.

| Added family | Source | Near / mid triangles |
|---|---|---:|
| House C | House Two | 17,910 / 4,108 |
| House D | House Three | 17,903 / 4,020 |
| House E | House Four | 8,091 / 3,482 |
| House F | House Six | 6,834 / 2,985 |
| Shop C | Shop 01 | 6,464 / 1,705 |
| Shop D | Shop 03 | 3,496 / 1,393 |
| Castle (retained, inactive) | Castle | 6,657 / 2,984 |
| Side gate | Castle Boundary Gate 01 | 2,016 / 1,790 |
| Wall | Castle Boundary Wall 01 | 106 / 106 |
| Barrel | Barrel 01 | 1,294 / 746 |
| Crate | Crate | 540 / 540 |

`assets/purchased/medieval_village/previews/expansion_contact.png` records all
11 added families under neutral offline lighting. These previews check source
materials and geometry; live engine captures establish the final lighting.

Regenerate selected architecture without recooking other families:

```sh
python3 tools/build_purchased_village.py --prepare
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
  --python-exit-code 1 --python tools/build_purchased_village.py -- \
  --families house_c,house_d,house_e,house_f,stall_c,stall_d,castle,gate,wall,barrel,crate
python3 tools/build_purchased_village.py --validate
```

The four goods families are bread, cheese wheel, roasted chicken and wooden
beer tankard. Each near model has 3,822 triangles and each mid model 930; every
model has one primitive. Their 11 shared texture policies use at most 1024px
images and approximately 58.667 MiB of conservative RGBA8 storage with mipmaps.
The eight GLBs together occupy 21,719,856 bytes. The cooker preserves the source
standard PBR texture graph and UVs independently of Blender's material export;
optional source specular multipliers unsupported by the native renderer are
omitted. No presentation lights or HDRI scene is imported.

Measured near-model dimensions in metres, X/Y/Z:

| Goods | X | Y | Z |
|---|---:|---:|---:|
| Bread | 0.4202 | 0.1609 | 0.4204 |
| Cheese | 0.3010 | 0.1539 | 0.3001 |
| Chicken | 0.4017 | 0.2051 | 0.4002 |
| Tankard | 0.2582 | 0.2402 | 0.1852 |

The individual source models, physical sizes, map statistics, hashes and geometry
validation are recorded in
[`purchased_market_goods_manifest.json`](../projects/valwood_world/models/purchased_market_goods_manifest.json).
Regenerate the offline goods with:

```sh
python3 tools/build_purchased_market_goods.py --prepare
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
  --python-exit-code 1 --python tools/build_purchased_market_goods.py
python3 tools/build_purchased_market_goods.py --validate
```

CPU/parser and software OpenGL scenery/village checks passed, including actual
imported bounds, product sizes, closed collision crossings, all 3,224 actual-model
main-route walking sweeps and 4,836 CPU three-lane sweeps. Merchants Lane adds
1,032 actual-model and 516 CPU three-lane walking sweeps, and the terrain fixture
checks the full paved lane and its matching grass exclusion. The current dense-street checks are retained at
`docs/performance/reference-village/dense-street-cpu-swgl-tests.log`, including
64 residents over 600 simulated seconds and the complete scene-before-population
texture-cache integration (232 scenery keys plus 40 pose image keys; 337.36 MiB
encoded source, zero unmanaged uploads). The earlier paving log remains at
`assets/purchased/medieval_village/authored-lane-paving-tests.log`;
`authored-lane-paving-shoulders.log` adds 266 samples beyond the former narrower
paving edge. The prior expansion log remains in `scene-expansion-tests.log`. These correctness checks do not
establish hardware frame rates; native visual and performance profiles are
separate.

The six coloured stall GLBs can be regenerated independently:

```sh
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup \
  --python-exit-code 1 --python tools/build_castle_village_assets.py -- \
  --families village_stall_red,village_stall_blue,village_stall_gold
python3 tools/build_source_market_canopies.py --stage --publish --validate
```

The historical `produce-preservation-check.json` records preservation during
cone removal. The subsequent source-cloth update's `VALIDATION.json` verifies
that all other 16 village GLBs remain byte-identical, structural wood positions,
normals and indices are exact, and all stall collider boxes still match.
Wood UVs and cloth geometry/materials intentionally change in that update. The added
144 food pieces fill twelve stalls' three bins in four-piece groups; their
actual footprints remain inside the bin walls and separated from one another.

The final close-market regression log is
`docs/performance/reference-village/final-market-cpu-swgl-tests.log`.
The eight market browsers add 19,200 body probes over ten simulated minutes,
with at least 1.800 metres between market travelers; overall crowd separation
remains at least 1.400 metres. No new model or texture family is added by this
last market pass.
