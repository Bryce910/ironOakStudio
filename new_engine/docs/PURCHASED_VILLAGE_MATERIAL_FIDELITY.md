# Purchased village material fidelity

The UE4.26 source project supplied in Downloads was inspected as package data.
No executable, project script or Unreal runtime was launched, and the source
files remain unchanged. The original custom native renderer is retained.

The current published representation is the **recipe6 source-UV façade pass**.
The previous A–D façade-atlas softness assessment is superseded by this pass;
the residual atlas no longer carries their wood/plaster detail.

The largest loss in the earlier conversion was texture density: repeating
2K sources were squeezed into a small part of a whole-building atlas. The near
LOD now retains original repeating source UVs for roof, stone, timber and
plaster. Six houses share the prepared original PBR textures by content hash.
A bounded 1K/512/256 atlas remains only for residual windows, decals, metal
and other small surfaces. All mid GLBs keep their previous single primitive,
geometry and material/image bytes.

The source export contains continuous red paint on A/C timber and several
plaster panels; that paint is preserved as a scalar vertex attribute. The
native material samples the source timber wear texture through the original
green mask at 2× UV. Plaster's authored green paint is zero in all six houses,
so its color remains the original plaster tile; red paint controls the source
normal/ARM layers with their original coordinate connections. The cooker
rejects nonzero green/blue paint rather than silently dropping unsupported
source layers. Source normal maps keep the original UV tangent basis.

| House | Wood source RMS texels/m | Wood gain | Plaster source RMS texels/m | Plaster gain |
|---|---:|---:|---:|---:|
| A | 575.0 | 19.7× | 658.1 | 20.6× |
| B | 628.9 | 22.5× | 724.6 | 23.7× |
| C | 538.3 | 31.6× | 674.2 | 36.2× |
| D | 567.6 | 36.1× | 709.1 | 36.9× |
| E | 590.7 | 3.5× | — | — |
| F | 1378.7 | 11.4× | 417.6 | 2.7× |

Gains compare matched source faces against the prior façade-only repack. These
are area-weighted UV measurements, not image-quality scores or the density of
every panel. Original positions, triangle counts and physical bounds stay
exact; source roof/stone UVs and image bytes stay exact. Source façade UVs,
tangents, red paint and prepared PBR image bytes are independently validated.
Maximum broad matched linear RGB difference is 0.00585. Near models now use at
most six primitives instead of four (A/B/C/F:5, D:6, E:4). Only painted source
façade materials add layer sampling; ordinary model materials keep their
existing sampling path. All mid LODs retain one primitive.

This removes the A–D unique-atlas density limit without increasing source
texture sizes. It does not reproduce the vendor's complete renderer. The
external HeightLerp function remains an explicit bounded approximation, and
POM relief, cloth subsurface shading, full indirect lighting and source scene
grading are separate features. The result is not pixel-identical to Unreal or
a claim of AAA parity.

## Source evidence and corrections

`assets/purchased/medieval_village/unreal-material-source-audit.json` records
63 material instances, selected master graph connections, source hashes and
recovered texture provenance. `tools/audit_purchased_village_source.py` can
reproduce that audit. Its texture recovery decompresses the full chunked-zlib
TextureSource PNG and validates its declared dimensions, rather than exporting
the 256px editor thumbnail.

The supplied TextureSource format is **TSF_BGRA8**: its internal PNG payload
stores B,G,R,A bytes. Treating that payload as an exported RGBA image previously
reversed red and blue. The extractor now normalizes the storage order exactly
once, preserving alpha. A regression compares two actual source textures with
the matching archive-exported PNGs: every canonical RGBA pixel matches exactly.
The raw payloads are retained in `source_storage_png/`; audit records include
storage format, raw PNG hash and canonical PNG hash. No downloaded source is
modified.

This correction affects all nine recovered PNGs, including ground color,
normal and ARM data. The runtime paving now uses the canonical ground images;
its ARM contract stays AO/red, roughness/green, metallic/blue. Actual material
output connections confirm those channels, and the source ground master sets
metallic to zero. The blue normal channel again stores normal Z. Reinterpreting
the reversed payload as an MRA texture would leave its color and normals wrong.

The main architecture PBR maps were exported archive PNGs and already had
correct channels. Near houses received an image-only update for the two
recovered RGB masks; their geometry, UVs, tangents, paint and material parameters
remain exact. Wood's green mask is unchanged by an R/B swap. Near plaster now
uses the correct blue-mask height channel. All mid GLBs remain unchanged: their
historical baked plaster normal/ARM blend retains the preceding mask
approximation. Its source base color is unaffected because authored green
paint is zero. Correcting that mid blend would require a separately validated
image rebake; none was hidden in this pass.

| Surface | Source finding | Correction |
|---|---|---|
| Roof 1/2 | Material-instance UV tiling is 1.5 | Retain repeating source UV0 at 1.5× near; bake the same scale into mid |
| Stone/castle walls | Explicit material-instance scale includes 1.5× castle walls | Read each source instance instead of assuming scale 1 |
| Timber | Red vertex paint is multiplied by the green RGB-mask channel at 2× UV | Recover the missing mask and restore broken wear instead of broad raw-red darkening |
| Plaster color | Source master uses green vertex paint, not red | Restore the correct channel; the inspected House A export has zero green paint |
| Plaster normal/ARM | Source master uses red vertex paint, RGB masks and independently connected texture coordinates | Restore these channel/UV connections rather than leaving normal/ARM fixed at layer 1 |
| Plaster third layer | Source contains a third cracked-paint texture absent from the exported FBX pack | Recover its full 2048² PNG; activate only through the source graph's paint/mask logic |
| Thatch coverage | Bulk is opaque; fringe is masked at 0.3333, although both use the same BC image | Separate their near material policy; bake bulk alpha as fully opaque into mid |

The runtime loader now honors those glTF policies: opaque/default materials
set cutoff0 and ignore source alpha; masks use their declared cutoff. The
existing BLEND cutout fallback remains unchanged. A generated GLB with orange
RGB and alpha0 verifies actual color and shadow pixels for OPAQUE versus MASK
in `test-world-models`; this catches clipping that a material-metadata audit
alone would miss. Blender source BC images also use independent packed RGB/A
channels so utility alpha cannot premultiply valid opaque color during baking.

Both recovered RGB-mask Texture2Ds retain their source default sRGB setting and
Color sampler, so the native source-layer sampler and earlier offline
reconstruction use the same color decode. The runtime GL regression checks
that mask/albedo auxiliary textures use sRGB storage and normal/ARM auxiliary
textures use linear storage.
Normal maps use the prepared DirectX-to-OpenGL green-channel conversion;
packed ARM remains AO/red, roughness/green, metallic/blue. Tile image bytes
are validated against their prepared sources and shared by content hash.

The external Engine `HeightLerp` function asset is not included in the supplied
project. Its reconstructed native/offline blend preserves the source endpoints,
height/contrast inputs and graph connections but is explicitly a bounded
approximation. No claim of pixel-identical Unreal shading or parallax occlusion
is made. The original source has POM, cloth subsurface response, scene grading
and authored lighting that are separate rendering features. Epic's
[HeightLerp reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/texturing-material-functions-in-unreal-engine)
describes the height/phase/contrast roles used by this reconstruction.

## Validation and budgets

The final source-UV façade pass is reproduced with:

```bash
blender -b -t 2 --python tools/build_purchased_village_facades.py -- --prepare-source
python3 tools/build_purchased_village_facades.py --stage --validate
python3 tools/build_purchased_village_facades.py --publish
python3 tools/build_purchased_village.py --validate
make test-world-models
```

The source metadata export runs on the CPU and contains UVs, tangents and paint;
it does not bake six new whole-building textures. Original façade PBR and mask
PNGs are embedded byte-for-byte and shared through the texture cache. Validation
uses the saved pre-pass near files, so it remains reproducible after publication.

The earlier base-material conversion below is retained for source recovery and
rebuilding other architecture families. **A full base rebuild must be followed
by the source-UV façade pass above** to restore the final near-house materials:

```bash
python3 tools/audit_purchased_village_source.py --extract --ground
python3 tools/build_purchased_village.py --prepare
blender -b -t 2 --python tools/build_purchased_village.py -- \
  --families house_a,house_b,house_c,house_d,house_e,house_f,stall_a,stall_b,stall_c,stall_d,gate,wall,barrel,crate \
  --stage-only
python3 tools/validate_purchased_village_fidelity.py
python3 tools/build_purchased_village.py --publish-staged
python3 tools/build_purchased_village.py --validate
```

The complete 30-GLB architecture library uses 89 unique images and 447.92 MiB
of conservative RGBA8 storage with mipmaps, below its 544 MiB cap. The active
14 families account for 432.17 MiB; the 15.75 MiB background castle remains
unloaded. The vendor explicitly describes that castle as a background asset
with no interiors; its disconnected source masses are not an import defect.

`material-fidelity-validation.json` records matched original triangles, exact
source FBX hashes, unchanged bounds/triangle counts, near source texel density,
and broad linear RGB differences against the corrected mid atlas. Four
interior samples per original triangle are weighted by physical surface area;
masked coverage is respected. No exposure or lighting is involved in the
palette check. Original runtime GLBs are retained in
`assets/purchased/medieval_village/material-fidelity-before/` on publication.

Across matched roof/stone surfaces, retained source UVs provide 7.67–78.14×
the earlier atlas texel density. This measures texture coordinates per physical
surface area, not a rendered quality score. Area-weighted broad near/mid linear
RGB deltas are at most 0.0058; corrected thatch bulk is 0.0012 and fringe 0.0042.
All 28 updated LODs retain their previous triangle counts and physical bounds.
Published mid geometry/UVs are preserved when the source hash/budget is
unchanged, avoiding edge-collapse changes during a material-only recook.

The preceding atlas-only repack is retained as historical validation and can
be reproduced with:

```bash
blender -b -t 2 --python tools/build_purchased_village.py -- \
  --families house_a,house_b,house_c,house_d,house_e,house_f \
  --facade-near-only --stage-only
python3 tools/validate_purchased_village_facades.py
python3 tools/build_purchased_village.py --publish-staged
```

`facade-repack-validation.json` verifies identical near triangle counts, bounds,
primitive counts, roof/stone UVs and tile image bytes. All six mid GLB files,
including their material/image bytes, remain identical. The maximum broad
linear RGB difference against the previous matched façade is 0.00205. A
separate pre-repack snapshot is retained in `facade-repack-before/`.

`source-facades/validation.json` records source image/UV/tangent/paint equality,
exact geometry and mid-file checks, texel densities and broad palette. The
near files before this pass are retained in `source-facades/before/`.
`test-world-models` renders all six actual near houses and controlled red/green
layer fixtures. It rejects invalid layer versions, modes, image indices, UV
scales and paint streams and verifies mode0 restoration after model draws.

`source-facades/channel-source-proof.json` and
`channel-correction-validation.json` record the later BGRA normalization,
known-archive pixel equality, all nine raw/canonical image comparisons,
image-only near-mask publication and exact preservation of all mid GLBs.
The extraction regression is `python3 tests/purchased_texture_source_test.py`.

The native `VALWOOD_materials_facade` extension has one versioned numeric array:
`parameters: [1, mode, aux0, aux1, aux2, primaryUV, secondaryUV, maskUV]`.
Mode1 uses original wood PBR plus auxiliary wear albedo and sRGB RGB mask;
mode2 uses original plaster PBR plus secondary normal/ARM and sRGB RGB mask.
`_VALWOOD_PAINT` is an authored scalar in [0,1], not an albedo multiplier.
The loader validates all fields and binds auxiliary samplers at units8/9/10.
The shader preserves the glTF V-flip-after-tiling convention and uses explicit
texture gradients for painted branches, retaining stable minified filtering.
Ordinary materials and procedural draws use mode0; model draws restore mode0
before returning. No source resolution reduction or mip bias is applied to the
shared source façade tiles.

Recovered full-resolution ground PBR images are under `source_ground/` with
source hashes and original normal convention. `MI_POM_Ground` uses soil at
15× UV and irregular soil/stone mix at 25×, with moss and puddle layers. These
are UV multipliers, not a world-space metres scale; terrain integration must
choose the physical scale separately.

The live terrain integration now uses those exact 2048² `T_SoilStoneMix`
BC/normal/ARM images as `projects/valwood_world/ground/village_stone_*.png`.
`village_material_provenance.json` records their identical source/runtime hashes.
The terrain shader applies a four-metre world-space repeat through the authored
village paving mask and retains the existing soft shoulders. The source normal
green channel is inverted during upload. A three-layer texture array uses
linear-light albedo mip averaging, separate linear normal/ARM averaging and
normal-variance roughness filtering. These ground images account for 64 MiB of
RGBA8 storage with mipmaps, separately from the architecture-library budget.

Runtime texture compression was investigated on the native RX 5300 and was
rejected for this change. The driver's BC7 encoder produced valid 4:1 storage,
but the sampled normal atlas had a 3.10° mean / 17.83° 99th-percentile angular
error, and independent ARM channels had large edge errors. Generating mips from
compressed images also accumulated coarse-level color error. A follow-up that
compressed independent reference mips retained an unacceptable sRGB bias in
its strict quality regression. No BC7 option remains in the model loader, and
the source PNGs, texture dimensions and normal/ARM data remain unchanged.
`bc7-native-quality.log` preserves the native proof as diagnostic evidence;
its trial upload timings are not frame-rate measurements.

The memory fix instead preserves exact texture data and keeps shared NPC pose
images in the bounded texture cache. The earlier 256 MiB encoded-source cap
was smaller than the complete scene's unique encoded image set, so identical
pose textures could bypass sharing after the cache filled. Cache capacity and
overflow telemetry are verified separately by the full scenery-plus-population
integration regression. The RGBA8 material budgets above remain the appropriate
conservative texture-storage estimates.
