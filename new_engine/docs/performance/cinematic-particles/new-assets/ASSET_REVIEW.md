# Purchased environment pack: bounded source inspection

Original downloads were read only. No full archive extraction, model cook,
runtime model replacement, scene mutation, or renderer job was performed.
Archive inventories include central-directory sizes and CRCs; these are not
claims of full-content SHA validation. Source labels/metadata are data.

## Exact paths

- `/home/computer/Downloads/Building+Elements.zip`
- `/home/computer/Downloads/Buildings&PropsTextures.zip`
- `/home/computer/Downloads/Props.zip`
- `/home/computer/Downloads/Nature.zip`
- `/home/computer/Downloads/BEP1_U5_Fix+2021-12-15+02-00-04-69.bmp`

The supplied names `Buildings+Elements`, `ProptsTextures`, and `BEP1_US` differ
from the actual files above. The BMP is a 1920×1080 forest showcase image,
not a model, texture atlas, or import manifest.

## Immediately useful particle art

The following original PNG bytes were copied from `Buildings&PropsTextures.zip`
into this evidence directory. Identically named sprites also occur beneath
`Props/Torch&Campfire/Textures/` in `Props.zip`.

| File | Source size | Visible grid | Alpha policy |
|---|---:|---:|---|
| Torchfire_Sheet.png | 1350×2048 | 9 columns × 8 rows, 72 cells, 150×256 each | Meaningful straight alpha, 0–255. Transparent RGB can be white. Premultiply before filtering/mip generation to avoid white fringes. |
| Campfire_Sheet.png | 2700×4096 | 9 columns × 8 rows, 72 cells, 300×512 each | Alpha is uniformly 255; RGB is fire over black. Use additive RGB or derive coverage carefully. Ordinary alpha blending produces an opaque black card. |
| Fire_Smoke.png | 128×128 | Single smoke sprite | Meaningful alpha 0–253; neutral RGB, white transparent padding. |
| Spark_Sharp.png | 64×64 | Single warm spark sprite | Meaningful alpha 0–255; white transparent padding. |

`particle-sprites-contact.png` is a source-art contact sheet.
`particle-sprites.json` records exact decoded dimensions/channel ranges.
Grid layout is visually clear and divides the files exactly; row-major
chronological order and playback rate are still inferences. There are no
`.mat`, `.prefab`, sprite `.meta`, particle-system, or authored timing files in
these ZIPs to establish those settings. Choose a rate during actual engine
review and label it an adaptation. Keep source bytes preserved separately.

For the 3GB GPU, avoid uploading the full campfire sheet: its RGBA8 base alone
is 42.19MiB before mips. Bounded per-frame resampling, tile gutters or a texture
array, premultiplied filtering, interpolation between adjacent frames, and a
procedural fallback are compatible with the custom GL3.3 engine. A 72-frame
64×128 RGBA8 array is 2.25MiB base / approximately 3MiB with complete mips.

## Model candidates and materials

All runtime candidates require an offline FBX/Blender-to-embedded-GLB cook.
`engine/world_models.cpp` accepts GLB triangle geometry with supported embedded
images and UV0; it rejects skinning and transformed/alternate material UVs.
It supports base color, tangent normal, ARM, emission, masks, and shared image
cache policies. Model `BLEND` currently uses a bounded cutout fallback rather
than sorted model transparency, so flame cards belong in the particle module.

Useful first candidates:

- `Props/Barrel_Chest/{Barrel_A,Chest_A}.fbx`: compact existing prop meshes
  with an explicit shared map family `Barrel&Chest_{Dif,Nor,Spec,Aoc,Depth}.png`
  in the same ZIP or `Buildings&PropsTextures/`. The diffuse, normal, and spec
  source files are 4096². Good tavern/stolen-supply additions after a bounded
  atlas/LOD cook; source does not provide an explicit roughness map.
- `Props/Torch&Campfire/Campfire/{CampFire_01,Campfire_01_LOD_01}.fbx`:
  a supplied near/LOD pair and a separate collider export. Coal diffuse,
  normal, displacement, and emission maps are available at 2048². FBX texture
  references include obsolete absolute PSD paths, so explicit remapping is
  required; do not assume automatic FBX linking will work.
- `Nature/Fern_01{,_LOD_0,_LOD_01,_LOD_02}.fbx` and the corresponding Fern_02
  family: useful native near/mid/far source choices. Maps are
  `Nature/BlendFiles/Textures/Fern.tga`, `Fern_Nor.png`, `Fern_Spec.png`,
  `Fern_Tl.png`. The TGA supplies the diffuse artwork; inspect its alpha and
  bind an explicit mask. `Tl` is a filename label, not proof of shader semantics.
- `Nature/Reed_Plant_0[1–4]_LOD_0[0–2].fbx`: shoreline vegetation variety with
  supplied LODs and `Reed_Plant_Dif.tga`, `Reed_Plant_Nor.png`,
  `Reed_Plant_Spec.png` maps in the same texture directory.
- `Nature/Long_Grass_01{,_LOD_01,_LOD_02}.fbx` plus varied grass clumps:
  promising for authored close patches; preserve the current stable masked
  lighting and wind/root behavior during conversion.
- `Building Elements/C_Wall/`, `Castle_Tower/`, `TowerGate/`, `Bridge_01/`,
  `Bridge_02/`, `Stall/`, and `Well/`: relevant structural families, but most
  lack explicitly named LOD exports. Bounds, triangle counts, materials,
  doorway clearance, collider shape, source UV tiling, and offline mid LODs
  need inspection before replacing existing demo structures.

The texture set is predominantly older diffuse/normal/spec/AO/depth artwork,
not a directly usable modern metallic-roughness pack. Do not blindly invert
Spec RGB into roughness or assume DirectX normal orientation. Inspect source
material values/node connections and retain nonmetal values where appropriate.
Existing tools `build_purchased_village.py`, `import_medieval_tools.py`,
`import_medieval_torches.py`, and `bake_gltf_uv.py` demonstrate source-preserving
offline cooking, explicit UV0, normal handling, bounded embedded images,
near/mid geometry, provenance, and validation. Blender must open source scenes
with script execution disabled as the existing importers do.

## License facts

No separate license, EULA, README, copyright, or terms document is present in
the four inspected archives. The user identifies these as purchased content.
One Unity folder `.meta` contains `licenseType: Free`; that is Unity import
metadata and does not establish a redistribution or open-content license.
Preserve the purchase terms separately and make no public-license claim.

Relevant small original `.meta` files and FBXs were copied under
`source-references/`; extracted filename/material strings are recorded in
`source-material-references.json`. No embedded source scripts were executed.
