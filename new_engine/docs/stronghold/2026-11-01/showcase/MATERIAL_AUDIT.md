# Grandstone source material fidelity audit

The purchased architecture is considerably more detailed than the current presentation. The static conversion made deliberate compromises for the 3 GB GPU, and several material conversions are incorrect. Adding copies of the same buildings cannot recover these losses.

## Confirmed conversion differences

| Item | Source shader/material | Current static bake | Impact / next action |
| --- | --- | --- | --- |
| Texture density | 4096² carved shape/normal atlases; 2048² marble grain; grain often tiled 3× or 4× | Every source image resized to 1024² before sampling, then tiled into one 1024² atlas | A 4× repeated grain receives about 256 pixels per tile versus the original 2048. Shape carvings lose 16× source pixel count. Retain dual UV layers natively, or use selected 2K/4K near atlases. |
| Tangent coordinate frames | Shader explicitly transforms UV3 tangent normals into the UV0 tangent frame | Base and shape normals are added as though both use UV0 | Wrong directions on rotated/mirrored UV islands. Bake tangent transforms or retain both UV streams and derivative tangent frames. |
| V coordinate orientation | Source normal maps use the Unity UV0 tangent frame | Geometry exports UV as (u, 1-v), then engine generates a new bitangent; normal green is unchanged | Vertical normal direction is reversed. Invert tangent-space Y during this specific bake, or preserve source tangent frame explicitly. |
| AO remaps | Uses BaseAORemap and ShapeAORemap min/max | Multiplies raw base and raw shape AO | For M_Marble_floor_grey the source shape AO min=max=1 deliberately ignores a leak mask; the bake darkens it. Honor both remaps. |
| Gloss | Source smoothness maps/remaps cover their full range | Static bake clamps roughness to at least .32; game shader also clamps to .25 | Polished marble and metal are dulled. Remove the cooker clamp; independently evaluate the game BRDF lower limit and antialiasing. |
| Snow cover | Source cover albedo/normal/mask, shape AO, slope, vertex paint, and world-space projection | Generic winter extension with constant .86 coverage; no source cover maps | Snow looks uniform and hides stone trim. Preserve cover-layer data, keep vertical masonry exposed, and add spatial variation. |
| Planar materials | Shader uses world XZ / BaseTilingOffset.x when BaseUsePlanarUV is true | Ignores planar switch and samples UV0 with vector tiling | M_Marble_path_snow is incorrectly mapped. Requires actual world projection or a per-instance bake; a single shared UV atlas cannot reproduce it exactly. |
| Stained glass | Glass shader is double sided and emits source albedo × thickness mask × ThicknessEmission (about 5.25) | Treated as generic opaque stone PBR, no emission or double-sided marker | Windows lose the colored light source and visual focal point. Preserve emissive texture/factor, emissive strength and source face policy. |
| AO curvature reduction | Source remaps shape AO before curvature reduction | Uses raw shape AO | Subtle but demonstrable tonal error on ornaments. Honor remapped AO. |
| Leaks UV selection | Independent Leaks_UV0_UV2_1 switch | Always chooses the base UV layer | Wrong stain placement for differing UV switches. Honor source selector. |

## Isolated corrective candidate tool

`tools/bake_nature_material_fidelity.py` writes only its requested output directory. It keeps source pixels at native resolution and samples derivative-selected mip levels while baking, honors AO remaps, leaves source roughness unclamped, transforms UV3 normals into UV0 via `tools/nature_fidelity_raster.cpp`, flips tangent Y for the existing exported UV convention, and writes the glass emissive mask/factor. Source planar materials require an explicit period bake or are rejected instead of silently approximated. `make(path, planar_period=6)` and `world_xz_planar_vertices(vertices14, 6, source_world_matrix)` preserve the 2 m base / 3 m shape tile periods of M_Marble_path_snow on horizontal source-world XZ floors. Apply the UV helper to each instance before relocating the scene; independent original-UV leak placement remains approximate in this mode.

Example:

```sh
python3 tools/bake_nature_material_fidelity.py --material M_Pillar_stone_01 --material M_stone_guard_01 --material M_stained_glass_02 --size 2048 --arm-size 1024
```

To repack source-identical native geometry with those maps, add `--asset hall_column` (or another name in the current citadel catalog). For arbitrary compatible scene exports the public APIs are `native_records(folder)`, `native_parts(folder)`, `FidelityBaker(parts_by_material, output, size, arm_size).make(material_path)`, and `emit_native_asset(doc, parts, material_results, output, winter=False)`. The last API writes fresh GLBs without retaining old embedded image payloads; the `--asset` repack path preserves the original source binary and can be larger.

Each result records a SHA256 fingerprint over source material, all sampled source image bytes, bake dimensions, UV/geometry parts, Python baker code, and C++ raster code. Candidates are recomputed and no old material-cache entries are reused.

This tool does **not** change production textures, import revisions, or the world document. Its outputs are review candidates. It still uses first coverage on shared UV0 atlases, so a native dual-UV architecture shader remains the preferred final representation. It does not implement source snow cover or world-space projection. Source fidelity does not imply that all 4K maps should be loaded on the 3 GB GPU at once; near texture tiers and streaming must be measured separately.

## Larger visible presentation gaps

The current town view has a broad, nearly featureless white foreground, a line of repeated facades at similar depths, exposed giant geometric mountain slabs, and bright neutral midday lighting. The publisher composition instead relies on carefully staged silhouette layers, believable rock/architecture joins, detail at foreground scale, local warm light against cold atmosphere, and coherent roof/structure relationships. Those need source-scene reconstruction and terrain/lighting changes alongside these material fixes.

The current main hall pillars are scaled roughly 4× wider than their source proportions; that stretches carved details and makes the great hall feel assembled from enlarged modules. Recreate source assembly dimensions first, then enlarge the settlement through additional bays/terraces rather than uniformly enlarging detailed props.


## Verification evidence

Synthetic geometry fixtures passed UV-frame identity, 90° rotation, reflected U, and V-flip normal parity. A fresh native-emitter column preserved 9028 near triangles and 1244 mid triangles, matching the current conversion. A three-material 512px smoke bake completed for the gray marble floor, pillar stone, and stained glass. Gray marble floor mean baked AO increased from .4898 to .9879 by honoring the source constant-one shape-AO remap.

`offline-column-before.png` and `offline-column-candidate.png` are matched CPU Cycles studio renders of the same source-scale column. They diagnose source material conversion only. They are **not game screenshots** and do not establish publisher-demo parity. Source snow cover is excluded from this comparison because Blender does not interpret the custom game winter extension.


The publisher-scene expansion exposed source-pipeline variants that were not present in the initial 39 conversions. The resolver now prefers GUIDs whose files actually exist. For the background snow mountain material, missing HDRP BaseColorMap/MaskMap references are repaired using the included BaseMap and MetallicGlossMap alternatives, with every repair recorded. Constant source albedo is retained for obsidian and the black background wall. Constant HDRP emission is retained for M_Emissive_Red; no nonexistent diffuse image is invented. Six-family smoke tests passed for these variants and the explicit 6 m floor projection. Results are in `projection-smoke/report.json`.


## Source terrain material delivery

`tools/prepare_nature_terrain_materials.py` prepares all eight purchased TerrainLayer albedo/normal/ARM triples at 1024px, plus optional raw height masks and two RGBA paint maps. The alpha channel order was recovered from the original serialized TerrainData by the publisher-scene pass: rock05, sand04, rock13, rock14, rock02, rock07, rock12, snow. Source tile periods are 8, 5, 8, 8, 15, 15, 15, 6 meters. The original alpha pixel sums range from .8745 to 1.5451; their ratios are normalized, then quantized jointly with a largest-remainder method, so all eight byte channels sum to exactly 255 at every pixel.

Material PNGs retain normal image top-row order and source normal green. Alpha PNG row0 is source terrain Zmin, because Unity GetAlphamaps was exported as [z,x,layer]. The native uploader must flip PNG rows once. The agreed terrain tangent frame is T toward engine+X, B toward engine-Z on flat terrain, matching Unity source+Z. These terrain normals therefore retain green; the architecture UV-flip correction above uses the different regenerated mesh tangent frame.

All 24 tile images were checked at 1024×1024, all paint weights were finite, and every quantized eight-channel sum was checked at 255. Manifest: `terrain-materials/manifest.json`.
