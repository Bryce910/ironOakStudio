# Cinematic asset sources

Powered by Poly Haven.

The 21 runtime assets below are in `projects/valwood_world/models/`. Their
sources are [Poly Haven CC0 assets](https://polyhaven.com/license), which permit
modification and commercial redistribution. Source URLs appear in the table.
The [public API terms](https://github.com/Poly-Haven/Public-API) request the
“Powered by Poly Haven” credit above. Asset preparation uses that API; the game
does not call it. No Poly Haven API implementation code is included.

| Runtime GLB, without `.glb` | Source | Triangles | Actual uninstanced height |
| --- | --- | ---: | ---: |
| `cinematic_pine_near` | [Pine Tree 01](https://polyhaven.com/a/pine_tree_01), variant b | 17,779 | 25.00m |
| `cinematic_pine_mid` | Pine Tree 01, variant b | 3,749 | 25.00m |
| `cinematic_pine_far` | Baked from the near pine | 2 | 26.50m canvas; 25m tree target |
| `cinematic_fir_near` | [Fir Tree 01](https://polyhaven.com/a/fir_tree_01), variant c | 17,766 | 22.00m |
| `cinematic_fir_mid` | Fir Tree 01, variant c | 3,879 | 22.00m |
| `cinematic_fir_far` | Baked from the near fir | 2 | 23.32m canvas; 22m tree target |
| `cinematic_cliff_01` | [Coastal Cliff 01](https://polyhaven.com/a/coastal_cliff_01) | 18,000 | 10.26m; 91.98m wide |
| `cinematic_rock_0` | [Rock Moss Set 02](https://polyhaven.com/a/rock_moss_set_02) | 6,000 | 1.80m |
| `cinematic_rock_1` | Rock Moss Set 02 | 5,999 | 2.50m |
| `cinematic_rock_2` | Rock Moss Set 02 | 6,000 | 3.20m |
| `cinematic_grass_0` | [Grass Medium 01](https://polyhaven.com/a/grass_medium_01), `small_a_LOD0` | 420 | 0.260m |
| `cinematic_grass_1` | Grass Medium 01, `large_b_LOD0` | 500 | **0.268m** |
| `cinematic_grass_2` | [Grass Medium 02](https://polyhaven.com/a/grass_medium_02), object c | 450 | 0.350m |
| `cinematic_broadleaf_near` | [Tree Small 02](https://polyhaven.com/a/tree_small_02) | 18,300 | 10.00m |
| `cinematic_broadleaf_mid` | Tree Small 02 | 3,249 | 10.00m |
| `cinematic_broadleaf_far` | CPU-baked from the near broadleaf | 2 | 10.020m canvas |
| `cinematic_pine_young_near` | [Pine Sapling Small](https://polyhaven.com/a/pine_sapling_small), variant b | 17,667 | 3.20m |
| `cinematic_pine_young_mid` | Pine Sapling Small, variant b | 3,100 | 3.20m |
| `cinematic_pine_young_far` | CPU-baked from the near young pine | 2 | 3.210m canvas |
| `cinematic_deadwood` | [Dead Tree Trunk 02](https://polyhaven.com/a/dead_tree_trunk_02) | 6,500 | 1.054m; 4.057m long |
| `cinematic_deadwood_mid` | Dead Tree Trunk 02 | 1,500 | 1.045m; 4.053m long |

Heights above were checked from GLB vertex positions with node transforms
applied, rather than Blender bounding-box metadata. In particular, grass 1's
manifest records a 0.32m cooking target, but its exported geometry spans
0.267912m vertically. The loader aligns its minimum Y to zero. The viewer
multiplies grass instance scale by **1.4**, giving nominal heights of
0.364 / 0.375 / 0.490m before each instance's existing scale variation.

## Adaptation and limits

Mature pine/fir retain source woody geometry and approximately 4,000 small
photographed shoots near / 1,200 mid, clustered over the source canopy's actual
3D vertex distribution. Near shoots target 0.62m pine / 0.50m fir, preserve the
photograph's aspect ratio and roll independently along outward branch
directions; approximately 30% use curved geometry. Mid LODs retain 45 major
source branch tubes with a separate trunk budget. These preserve aggregate
placement, not the full multi-million-triangle needle mesh. Shoot photos still
repeat and approximate individual needles. The newer broadleaf and young pine use
individual original leaf/needle components with their original photographed UV
islands, fitted to small quads in their actual source positions/orientations.
The broadleaf keeps 7,000 / 1,000 selected leaves; young pine keeps 7,734 / 1,250
needles (near / mid). Mid needles widen by at most 2.2×, while broadleaf cards
expand by at most 1.25× to retain distant coverage. Source woody branches are
separately reduced. These avoid whole regenerated needle-branch sheets, while
still approximating original curved, detailed leaf geometry.

Each far tree is one upright local XY quad, with Z=0 after Y-up export. The
vertex shader must turn it cylindrically toward the camera. Its photograph is
a single view baked from the near asset, with approximate baked lighting; it
cannot supply parallax or correct lighting from every direction. Transparent
canvas extends 0.75m below the pine root and 0.66m below the fir root; the viewer
compensates for the loader's minimum-Y alignment. New broadleaf/young-pine
canvases are alpha-cropped and ground-aligned, with **zero root padding**.
Tiny mip levels lose trunks
and silhouette detail, so the renderer fades cards by projected size and uses
terrain canopy shading for distant continuity.

The cliff and rocks retain source PBR with bounded geometric decimation. The
three grass models are individual 3D tufts, not presentation spheres or scatter
meshes. Their source diffuse and alpha are combined into RGBA; source normal
and packed material maps accompany them. Grass atlas UVs do not encode a
universal blade root-to-tip gradient.

All 21 GLBs have embedded images, UV0, finite static geometry, baked texture
transforms and no skins or animations. Images are at most 2048px per dimension;
rock maps are 1024px. Original far canvases are 1024×1536px; new far views are
CPU-rendered at 768×1024px then alpha-cropped. Runtime image payload
hashes match exactly within these groups:

| Sharing group | Identical encoded image payloads |
| --- | ---: |
| Pine near ↔ mid | 9 |
| Fir near ↔ mid | 6 |
| All three rocks | 3 |
| Grass 0 ↔ grass 1 | 3 |
| Broadleaf near ↔ mid | 9 |
| Young pine near ↔ mid | 6 |
| Deadwood near ↔ mid | 3 |

The engine can therefore share their texture storage when sampler/color-space
policies also match. New tree/deadwood maps use 2K primary albedos, 1K branch
albedos/normals and 512px material maps. Both new trees, both deadwood LODs and
their far cards together require approximately **157.3MiB** of unique RGBA8
texture pixels with mipmaps, assuming the shared cache works as tested. This
excludes mesh buffers and other engine allocations. Asset budgets alone do not
guarantee a frame rate.

## Reproduction and records

Run from the engine root with Blender 4.0 and Python with NumPy/Pillow/SciPy. On this
workstation the latter interpreter is:

```sh
asset_python=/home/computer/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3
$asset_python tools/build_cinematic_assets.py --download
blender --factory-startup --background --python tools/build_cinematic_assets.py -- --skip-tree-cook
blender --factory-startup --background --threads 2 --python tools/refine_mature_canopies.py
$asset_python tools/build_cinematic_grass.py --download
blender --factory-startup --background --python tools/build_cinematic_grass.py
$asset_python tools/build_cinematic_diversity.py --prepare
blender --factory-startup --background --threads 2 --python tools/build_cinematic_diversity.py
$asset_python tools/build_cinematic_diversity.py --finish-far
$asset_python tools/build_cinematic_assets.py --validate
```

The mature refinement renders far silhouettes with Cycles explicitly on the
CPU, two threads, 32 samples; the diversity cook uses CPU/two threads/24 samples.
The `--skip-tree-cook` flag omits the historical coarse-canopy/Eevee stage.
Grass cooking does not render. Rock
cooking uses the existing source cache under `assets/polyhaven/models/`.
`--billboard-far` can convert an older crossed-card far GLB to one quad on the
CPU without changing its image bytes. The default cook already emits one quad.

`projects/valwood_world/models/cinematic_manifest.json` records runtime triangle
counts, file SHA256 and cooking metadata. `assets/polyhaven/cinematic/` retains
unmodified API metadata, source glTF, selected geometry and PBR inputs, plus
`provenance.json` with source/derived file hashes. Tree `selection.json` files
record original binary URLs and HTTP byte ranges: only the chosen woody meshes
and canopy positions are downloaded from the large source binaries. Existing
rock-source hashes are in `assets/polyhaven/models/manifest.json`. Original
Seldonne runtime assets are not overwritten by these builders. The former six
mature-tree GLBs, manifest and hashes are preserved as historical comparison
data under `docs/performance/authored-landscape/before/models/`.
