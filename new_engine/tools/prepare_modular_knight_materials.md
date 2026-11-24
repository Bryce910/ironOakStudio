# Purchased Modular Knights material derivatives

Run from the repository root with Python 3, PyYAML, Pillow, NumPy and system
libtiff installed:

```bash
python3 new_engine/tools/prepare_modular_knight_materials.py \
  --selection new_engine/assets/purchased/modular_knights/selection.json
```

The selection is `{"pieces":[{"mesh_name":"...","material_paths":["Assets/...mat"]}]}`.
The default output is `assets/purchased/modular_knights/derived/materials` inside
`new_engine`. `materials.json` maps each exact selected Unity material path to
absolute derived image paths, preserved material factors, alpha/culling policy,
source texture bindings and warnings. `--output-dir`, `--source-root` and
`--inventory` can select alternative locations. Outputs inside the purchased
source directory are rejected. Source files and metadata are never modified.

The default `--profile balanced` keeps head base color at 2048 and head normals
at 1024, with other base colors at 1024 and other normals at 512. ORM is 512
throughout. This fits an initial runtime asset under the current import/texture
budgets. `--profile high` uses maximum sides 2048/1024/1024 for every material.
The final log includes total compressed image bytes and estimated RGBA8 storage
with full mip chains, counting each output image path once.

The converter reads every Unity YAML document with `yaml.safe_load_all`, finding
the Material even when an unrelated MonoBehaviour comes first. It uses the
actual Unity Standard `_MainTex`, `_BumpMap`, `_MetallicGlossMap` and
`_OcclusionMap` bindings, not stale HDRP texture slots present in these files.

- Opaque base color becomes JPEG at the profile's maximum side, quality 94, with the
  original color tint retained as a separate factor. Masked base color stays
  RGBA PNG, preserving source coverage.
- Tangent normals become RGB PNG at the profile's maximum side, are renormalized after
  resizing, and apply `flipGreenChannel` from source metadata when requested.
  The authored `_BumpScale` remains a separate normal scale.
- ORM becomes RGB PNG at the profile's maximum side: R is source AO green blended toward
  one by `_OcclusionStrength`, G is `1 - smoothness`, B is source metallic red.
  Smoothness is source metallic-map alpha times `_GlossMapScale`, or source
  base alpha when `_SmoothnessTextureChannel=1`. With no metallic texture,
  `_Metallic` and `_Glossiness` supply their ordinary scalar values. Data
  channels resize independently, avoiding color/alpha premultiplication.
- These source TIFFs can store smoothness in an *unspecified extra sample*.
  Generic readers silently drop the fourth sample or reject grayscale plus
  a second sample. Raw libtiff decoding preserves the purchased data in both
  cases; no smoothness is invented to replace it.
- Opaque/cutout settings and culling come from the source. Standard alpha
  blending is explicitly downgraded to MASK at the authored cutoff because the
  current engine lacks BLEND support; the manifest records that compromise.
  Missing referenced base/metal/normal images fail. A referenced AO GUID outside
  the package produces AO=1 and an explicit warning. Non-default UV transforms
  and height-to-normal imports fail instead of silently changing appearance.

No artistic roughness correction, shader approximation to Unity HDRP,
retargeting, game launcher change or gameplay integration is performed here.
Source dimensions, channel ranges, original GUIDs and full paths remain in
the manifest for review.
