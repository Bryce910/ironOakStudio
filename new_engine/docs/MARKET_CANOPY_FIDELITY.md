# Source cloth market canopy update

The three existing `village_stall_red/blue/gold` aliases now use coherent,
weathered cloth from the purchased Medieval Village kit. Their near and mid
models use the source cloth albedo, OpenGL tangent-space normal and ARM maps.
The red alias uses a red panel of `T_Tent_1`, blue uses the blue panel of
`T_Tent_2`, and the gold alias now uses muted linen from `T_Tent_3`. Source
weave, stains and wear replace the former bright generated stripe material.
There is no constant color multiplier or baked lighting.

The original posts, crossbars, counter, bins and supports retain exactly the
same positions, normals and triangle indices. Wood UVs now follow the longest
axis of each board or post at a physical scale, instead of placing horizontal
grain across vertical posts. The three wood maps are exact encoded copies of
the purchased source wood already used on house E. The texture cache can share
these with the architecture, including the same repeat/filter/color policy.

Only the cloth mesh changes. A smooth grid introduces restrained crosswise
sag and small wrinkles; an uneven straight hem replaces the regular scallops.
The overall bounds remain exactly 4 × 3 × 3.3 metres, all five collision boxes
are unchanged, and the counter and bin surfaces used by product-support tests
are unchanged. No placements, NPC files, animation data or shaders changed.

| Measure | Before | Current |
| --- | ---: | ---: |
| Near triangles per stall | 320 | 864 |
| Mid triangles per stall | 320 | 384 |
| Materials / primitives | 3 | 2 |
| New cloth images across all six GLBs | — | 9 shared images |
| New cloth RGBA8 plus mip storage | — | 21 MiB |
| Additional wood image policies | — | 0, shared with purchased houses |

Each cloth uses 1024² albedo, 512² normal and 256² ARM, shared between both
LODs and every instance. The complete prototype kit's standalone estimate is
127.33 MiB; that includes the 28 MiB of wood already resident with purchased
houses. The actual combined-scene increment is 21 MiB before driver overhead.
Runtime texture-cache integration and native frame timing remain separate
checks; an offline texture estimate is not a frame-rate measurement.

The material is standard, opaque, two-sided PBR. The source material's cloth
subsurface/transmission and physical cloth simulation are not reproduced.
The authored canopy geometry and selected panel crops are adaptations, not a
claim that its UVs and silhouette match the original vendor shop exactly.

`assets/purchased/market_canopies/PROVENANCE.json` records all source and
prepared image hashes, crop rectangles, reduction sizes and licensing.
`VALIDATION.json` records all six before/after model hashes and preservation
checks. The original generated stalls and manifest are retained in `before/`.
Purchased source originals remain unchanged. Converted proprietary material
pixels do not acquire a CC0 or redistribution license from the authored mesh.

Reproduction, after rebuilding the original generated village kit if needed:

```sh
python3 tools/build_source_market_canopies.py --stage
python3 tools/build_source_market_canopies.py --publish
python3 tools/build_source_market_canopies.py --validate
```

The stage is validated before publication. Validation checks the actual GLB
buffers, exact structural geometry, original bounds/collision records, normal
and triangle orientation, valid PBR channels, no color tint, shared LOD images,
resident wood content hashes, all unselected prototype hashes, and the complete
22-model prototype-kit budget.

The initial character art audit is `assets/characters/art-fidelity-audit.json`;
it describes the previous diffuse-only female placeholders. A subsequent,
separately validated update replaces both female roles with the freely
licensed Sintel game character while preserving all 76 pose aliases and 91
smooth runtime blend pairs. Its five source material primitives keep cloth,
skin, metal fittings, hair, opaque eyes and separate black alpha lashes.
The independent proof is `assets/characters/sintel-independent-validation.json`:
16 compatible replacement poses, source albedo pixels retained, correct
normal convention, secondary-UV AO remapping, and 31.31 MiB of shared images.
The full active crowd uses 45 image policies and 213.31 MiB estimated RGBA8 plus
mips. This improves clothing, face, silhouette and hair over the placeholders;
Sintel remains a stylized legacy game character. Scanned facial detail, source
subsurface scattering and hair anisotropy are not claimed.
