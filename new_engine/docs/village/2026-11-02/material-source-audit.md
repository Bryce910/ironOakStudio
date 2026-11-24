# Purchased village material integration audit

This audit covers the original Unreal Medieval Village project and the first
native source-layout city conversion. The Grandstone project is unaffected.

## Confirmed texture and geometry conventions

- Original UVs and source tangents replace the former residual object atlases.
  In particular, the six former near-house window materials lost between
  15 and 189 times their source linear texel density in the residual atlas.
- The source FBX exporter preserves original Unreal-centimetre geometry,
  normals, UV0/UV1, tangent sign and RGBA. The city converter changes basis to
  `(X,Z,-Y)`, flips texture V once, and negates tangent handedness for that flip.
  A second tangent or normal-green inversion is not justified.
- Original DirectX normal green is inverted exactly once during texture
  preparation. Packed layer pages contain linear normal XY, roughness and AO;
  they are deliberately excluded from normal/ARM BC7 classification.
- Source ARM channels follow the material graph, not filenames. For example,
  `T_Moss_MRA` is actually sampled as AO/Roughness/Metallic. Some original ARM
  assets have sRGB enabled and Color samplers; the preparation reproduces that
  decode once into native linear ARM. Cloth ARM explicitly disables sRGB.
- The 773 raw FBX mesh records contain 3,771,897 corners. All source RGB values
  are canonical RGB8 to within `5.960464477539063e-08` of byte/255. Component
  paint overrides separately recovered from the map use canonical bytes.

## Loader blockers corrected or handed off

The cloth mode5 parser previously required source paint even though the cloth
graph does not consume it. Both direct and streamed loader branches now allow
mode5 without paint while retaining the UV/tangent requirements. Modes3/4 still
strictly require source RGB paint. Existing source-paint and baked-light tests
pass.

The real-asset CPU sweep also identified zero-area grounding helpers reusing
material0 from a layered source material while omitting its required streams.
These helpers are a converter defect, not a reason to weaken validation. The
publisher-layout agent owns the helper repair and full-catalog CPU sweep.

## Verified graph corrections in the isolated candidate

Original graph evidence is retained in
`.local-tools/village-source/candidates/combined-gi-materials/graph-correction/audit.json`.

1. `MM_Master_3_Blend` applies its final green-painted HeightLerp to normal,
   roughness and AO as well as color. Its FunctionCalls1/2/3 explicitly consume
   VertexColor output2. The first native shader omitted that final physical
   layer. For the used `MI_WallPlaster`, Normal3 equals Normal2 and ARM3 equals
   ARM1, so existing packed pages can be sampled at the original third-layer
   tiling. No additional images are needed. An exact zero-green branch avoids
   the extra samples on the many authored black-painted houses.
2. `MM_POM_3_Blend_Puddle` uses distinct heights for color and physical channels.
   Color green/puddle blend uses `TextureSample1` (`T_Fill_BC`, constant128sRGB).
   Normal/AO/roughness use `TextureSample0` (`T_SoilStoneMix_H`). The first native
   shader instead used moss roughness for the green height and one constant
   puddle weight for all channels. The candidate restores those input paths
   using the existing height texture.
3. Ground FunctionCall6 deliberately uses ARM Mask3's roughness for its first
   red roughness blend, while AO uses ARM Mask2. The candidate follows that
   original graph selection.

The external UE HeightLerp function remains an explicitly documented
approximation, and source POM depth is not yet executed. Source cloth masks and
opacity are preserved, with bounded native thin transmission approximating
Unreal's subsurface shading. These are remaining fidelity limits, not missing
asset purchases.

## RGB8 and baked lighting coexistence candidate

**Subsequent energy audit: do not integrate ambient replacement.** The original
registry has 1,349 of 1,363 HQ records with an analytical maximum diffuse value
of only `3.673733791253247e-08`; every architectural and ground binding is in
this group. All records contain zero baked light GUIDs. The other 14 records
belong entirely to instanced flowers, grass and debris. See
`baked-light-energy-audit.json` and `tools/audit_village_baked_light_energy.py`.
These coefficients decode to the original engine's logarithmic black point.
This is a source-data finding, not a native scalar/GI storage defect.

Both current authored Sun and SkyLight are Movable. Original UE lighting adds
dynamic skylight diffuse independently of the lightmap; replacing all native
ambient with near-black static data would remove that required source term.
The previously staged replacement shader remains isolated as a reference and
requires revision or exclusion. Do not duplicate all architecture merely to
store these negligible values. Source graph corrections and RGB8 storage are
independently valid.

The corrected alternative is isolated under
`.local-tools/village-source/candidates/combined-dynamic-sky-gi/`. Reproduce it
with `stage_village_combined_gi_materials.py --include-foliage-gi
--keep-dynamic-sky --output <new-revision-directory>`. This version adds valid
baked indirect independently and keeps movable-sky diffuse, direct sunlight,
specular and existing native bounce. Neither candidate installs itself.

`tools/stage_village_combined_gi_materials.py --include-foliage-gi` stages all
runtime/header/shader files under the private `combined-gi-materials` directory.
It does not install them into any running project.

- The existing scalar stores `R | (G<<8) | (B<<16)` as float32. Every uint24 is
  represented exactly. The vertex shader decodes before interpolation;
  `uSourcePaintEnabled` distinguishes this interpretation from ordinary paint
  or lava. Existing wood shading receives the decoded red value.
- The existing extra RGBA remains independent linear HDR baked lighting plus
  validity. The vertex ABI stays 68 bytes, with zero additional vertex or texture
  GPU allocation. Source paint can coexist with GI, but remains incompatible
  with legacy scalar paint or lava on the same primitive.
- UE's decoded `OutDiffuseLighting` is irradiance/PI: multiply by the native
  diffuse factor without another PI, gamma or exposure conversion.
- The first isolated candidate kept native direct sun/specular while replacing
  diffuse ambient. The subsequent bound above proves that the dynamic-sky
  contribution must also be retained; this first shader must not be installed
  without that correction. Historical bake mobility is not proven.
- Valid source foliage GI also replaces its indirect term. Leaf transmission
  and direct sun remain independent. Far cards can use vertex GI without new
  texture samples.

CPU verification passed exhaustive encoding/decoding of all 16,777,216 uint24
values, canonical source-byte validation, malformed/ambiguous input rejection,
and actual native preparation of a GLB containing source RGB plus HDR GI
simultaneously. All GI channels and validity values, including 65504, remain
exact under the unchanged 768 MiB preparation budget. Production GLSL compilation,
full real-GI catalog preparation, native visual comparison and sustained frame
timing remain integration checks controlled by the parent agent's GPU lease.

## Production integration

The dynamic-sky-safe revision was installed into the production source-paint
header, source-paint unit fixture, and the two private city foliage shader
overrides. Both focused Makefile tests pass. The actual production CPU loader
preserves all 353,643 source-ground RGB/GI samples bit-for-bit and preserves
the independent fixture's 65504 HDR endpoint. No scene/catalog, image, vertex
layout or CPP loader replacement was required. Exact hashes and backups are
under `material-integration/`.

The explicit scalar contract also received a minimal compatibility patch in
five physical vertex shaders: generic, `valwood_visual`, `valwood_stronghold`,
`valwood_presentation`, and `valwood_showcase`. `valwood_world` and `seldonne`
inherit the generic file through existing symlinks. Each change adds only the
default-false source-paint uniform and guarded red-byte decoding for the old
facade varying. Existing source-free and legacy-red behavior is unchanged.
Other projects' fragment material graphs were not replaced. The source shader
preparer now emits the packed source RGB contract without duplicating that
uniform or reviving the obsolete location12 union; its check ran in `/tmp`.
Frozen initial staging images/shaders remain unchanged, so project builders
must preserve the upgraded city overrides when regenerating scene data.
