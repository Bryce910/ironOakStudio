# Finalize the derived Modular Knights GLB

```bash
python3 new_engine/tools/finalize_modular_knight_glb.py \
  new_engine/assets/purchased/modular_knights/derived/modular_knight_hero.raw.glb \
  --output new_engine/assets/purchased/modular_knights/derived/modular_knight_hero.finalized.glb
```

The default material manifest is
`assets/purchased/modular_knights/derived/materials-balanced/materials.json`
inside `new_engine`. Override it with `--materials`. `--report` selects the JSON
report path; otherwise the output's `.report.json` sibling is used. Input and
output must differ, and purchased source assets cannot be overwritten.

An explicitly named, unmapped procedural material can be retained with repeatable
`--preserve-material EXACT_NAME`, for example `--preserve-material
Valwood_LongSword_Steel --preserve-material Valwood_LongSword_Leather`. This
whitelist is strict: the exact exported name must exist, and the material must
be opaque, texture-free fixed PBR with valid factors and no shader extensions.
All other unmapped names still fail. Unused manifest entries are allowed and
listed in the report; they do not create exported materials or images.

Each exported material must match an exact manifest `name`. The finalizer
rebinds the actual derived base, normal and ORM image bytes; assigns the source
tint, normal scale, alpha policy, cutoff, culling and baked material factors
exactly; and shares the same ORM texture for AO and metallic/roughness. AO
strength is one because source AO strength is already applied in the derivative.
It performs no additional normal green-channel flip. Source texture-import
flip metadata and explicit alpha-blending compromises appear in the report.

Only exactly constant LINEAR translation/rotation/scale sampler values can be
reduced to their two original endpoint keys. The tool uses exact equality, not
a tolerance, and verifies clip duration preservation. Nonconstant motion is
untouched. Unreferenced images, textures, accessors and binary buffer views are
removed afterward. All retained attribute/index values, inverse bind matrices
and node transforms must match the input exactly or the conversion fails.

The output is checked against current engine budgets: 32 MiB GLB, 256 nodes,
128 joints, one skin/mesh/node, four skin influences, no morphs/compressed
geometry, 200,000 vertices/600,000 indices, 128 primitives/images/materials,
64 clips, 768 channels per clip, 16,384 keys per channel, 262,144 total channel
keys, embedded images with maximum side 4096, and estimated RGBA8 textures plus
full mip chains within 256 MiB. The report records sizes, hashes, source material
bindings and every constant reduction.

These checks supplement the actual engine importer and rendered preview. They
do not approve retargeting, foot contact, equipment grips or gameplay flow, and
they do not change the game, launchers, controller, source rig or animations.
