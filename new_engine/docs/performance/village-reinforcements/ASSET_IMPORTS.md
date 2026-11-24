# Village reinforcement imports

Three user-supplied packs provide the new farm tools, shield watch and mounted guards. Original downloads and extracted sources remain unchanged; manifests retain source hashes, material conversions, geometry counts and output hashes. The toolkit and shield-knight `.blend` files were opened with source-script execution disabled. The horseman converter imports FBX geometry and evaluates its rigs offline.

| Pack | Converter | Manifest / provenance |
| --- | --- | --- |
| Medieval tools | [import_medieval_tools.py](../../../tools/import_medieval_tools.py) | [Runtime toolkit](../../../projects/valwood_world/models/village_toolkit_manifest.json), [source provenance](../../../assets/purchased/medieval_tools/PROVENANCE.json) |
| Knight with shield | [import_village_shield_knight.py](../../../tools/import_village_shield_knight.py) | [Runtime knight](../../../projects/valwood_world/models/village_shield_knight_manifest.json), [source provenance](../../../assets/purchased/shield_knight/PROVENANCE.json) |
| Adam Knight Horseman | [import_village_horseman.py](../../../tools/import_village_horseman.py) | [Runtime horseman](../../../projects/valwood_world/models/village_horseman_manifest.json), [archive inventory](../../../assets/purchased/horseman/archive-inventory.json) |

The shield import uses the valid **`knight-with-a-shield(1).zip`** (968,753,530 bytes). The similarly named unnumbered file is an HTML download page, not a ZIP. The numbered archive supplied the original knight rig, sword, shield and texture sets; its contents were recovered without changing either download. The 2.6 GB horseman ZIP was streamed in bounded chunks, with only relevant nested source packs extracted.

## Geometry and materials

| Family | Runtime geometry | Shared texture estimate, including RGBA8 mipmaps |
| --- | --- | ---: |
| Twelve tools | 24 GLBs: source near meshes total **8,088 triangles**; mid meshes total **3,818**. One primitive per model. | **16 MiB**: three 1024² maps |
| Shield knight | Eight compatible idle poses at **20,174 triangles each**. Static catalog near/mid: **20,174 / 7,999**. One primitive per pose. | **16 MiB**: three 1024² maps |
| Horseman | **76,798** selected source triangles reduced to **55,000 / 15,999** near/mid. Complete mounted silhouette, 24 source components, 3.12 m total height; one primitive per model. | **16 MiB**: 2048×1024 base, 1024×512 normal and ORM |

The three families add approximately **48 MiB of unique texture storage**, not that amount per instance, pose or LOD. Embedded image bytes are shared within each family through the runtime cache. Tools retain their original atlas and roughness/metallic channels. The knight converts original metallic/smoothness and AO maps into ORM. Horseman legacy specular maps receive a documented conservative roughness/metallic conversion; source diffuse and normal detail remain in the atlas.

## Motion and placement

Four shield-watch NPCs interpolate adjacent samples of the genuine source `Idle` action, frames 1–105 at 30 FPS: **104/30 = 3.4667 seconds** per cycle. Roots and facing remain fixed at the generated stations; color, reflection and shadow passes share the uploaded pose weights. The raw source endpoints differ by approximately **31 mm RMS / 161 mm maximum**. The eight-sample loop interpolates the 7→0 transition continuously; the original clip is not claimed to be seamless or to contain additional unsampled motion.

The two horsemen are **static mounted guards**, sampled from aligned source frame 1. Their original horse and rider animation timelines are preserved in the FBXs; runtime horseback animation is not implemented.

[The placement recipe](../../../tools/build_village_reinforcements.py) adds **38 scenery records: 30 tools, six supporting furniture items and two horsemen**, while retaining the original **2,640-record prefix**. The four animated watch NPCs are separate runtime actors, without duplicate static knight placements. [Stage evidence](stage.json) records the layout and asset hashes.

## Verification and previews

[CPU watch tests](watch-runtime-cpu.log) pass all 24 injected load/batch/target failures, 600 seconds and 144,000 fixed-root probes, continuous cyclic weights, culling/reentry, pass parity, zero hot allocations and complete cleanup. [Toolkit reproducibility](../../../assets/purchased/medieval_tools/REPRODUCIBILITY.json) and [knight encoded-asset validation](../../../assets/purchased/shield_knight/validation.log) cover their cooked outputs.

These CPU renders load the actual cooked GLBs with neutral test lighting; they are asset previews, not in-game captures: [toolkit](../../../assets/purchased/medieval_tools/cooked-toolkit-preview.png), [knight phase 0](../../../assets/purchased/shield_knight/cooked-idle-00-cpu-preview.png), [knight phase 4](../../../assets/purchased/shield_knight/cooked-idle-04-cpu-preview.png), [horseman near LOD](../../../assets/purchased/horseman/horseman-preview.png).
