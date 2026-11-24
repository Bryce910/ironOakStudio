# Publisher showcase source bridge

The original `Demo Castle.unity` is retained under `.local-tools/unity-source/showcase-source/`. Its exact authored layout has 5,988 prefab instances from307 sources. This includes three ascending castle districts, a separate barbican, a complete temple, a wizard tower, terrain, 15 background mountains, and extensive rock retaining walls. The earlier custom Grandstone scene did not reproduce this composition.

## Conversion

Run `python3 new_engine/tools/stage_nature_showcase.py` to stage the data-only bridge. This preserves every original hierarchy/transform/material override. It restores sourceFBX geometry in place of5,415 author-painted per-instance mesh overrides, reducing the1.389GB scene to124MB. The802 bespoke scene meshes without a prefab replacement remain. The original painted meshes remain in the archived/full scene for later recovery; claiming the painted copy is already exact would be incorrect.

No purchased `.cs`, `.dll`, `.asmdef`, or executable code is staged. The importer reads the purchaser-owned geometry, materials, terrain and settings. Unity's private project is isolated from `/home/computer/My project`.

Unity command from repository root:

```
.local-tools/unity-source/editors/6000.3.27f2/Editor/Unity -batchmode -nographics -quit -projectPath .local-tools/unity-source/asset-bridge -executeMethod NativeShowcaseExport.Run -logFile .local-tools/unity-source/logs/showcase-export.log
```

Configuration: `.local-tools/unity-source/showcase-selection.json`. `groups=[]` exports all active source groups. Nonempty groups filter exact top-level hierarchy names, e.g. `Lower Castle`, `Medium Castle`, `High Castle`, `Temple_Complex`, `Barbican`, `Background Mountains`, `Rocks Walls`, `Rocks`, `Town Buildings`, `Dam`, `Wizard Tower`. Terrain and cameras export independently of this filter. `lod_mid_index=2` selects the third authored LOD, clamped to final available LOD. LOD0 and mid are selected once per LODGroup; ungrouped mesh renderers are included once.

## scene.json

Output directory `.local-tools/unity-source/showcase-native`. Meshes are shared by SHA256 of their complete binary geometry. Render placements reference shared geometry hash and individual source material bindings; do not instantiate both LODs simultaneously.

- `meshes`: unique local-space mesh records, `binary`, vertex count, submeshes, UV presence, source asset path.
- `placements`: one active MeshRenderer each. `lod_mask&1` is authoredLOD0; `lod_mask&2` is selected midLOD. A mask3 renderer participates in both levels and is one physical placement. `matrix` is a16-float **column-major Unity local-to-world matrix**. World-space bounds permit spatial culling. `chunk` groups by exact district and64m grid of renderer bounds center, while retaining source-local geometry and exact instance matrices. Oversized source meshes may span multiplecells; a chunk label is not a guarantee that the full mesh fits thecell. Mesh vertices remain Unity left-handed coordinates. Converting to engine coordinates requires mirroringZ and triangle winding (plus negative-determinant transforms). Material array slots correspond to submeshes.
- `materials`: exact source `.mat` asset path/GUID and complete raw serialized materialYAML. Use original purchased textures, not Unity's downsampled import cache. These retain source shader values/mapping modes. Empty bindings are surfaced in `warnings`.
- `lights`: original active point/spot/directional/area lights, source baked/realtime classification, color/intensity/range/shadows and worldmatrix. A realtime native budget can select or bake these instead of illuminating hundreds concurrently.
- `cameras`: original camera worldmatrix, FOV, clipping and orthographic data. The publisher's animation timeline is retained separately at `showcase-source/ModularCastleCameraTimeline.playable`; the exported camera transform is its scene value, not the whole animated flythrough.
- `settings`: original ambient sky/equator/ground colors, ambient intensity, fog and skybox asset reference. Postprocessing settings remain in the extracted source profile and audit.
- `terrains`: native height, alpha and hole data, size, transform, layer texture references and tiling.

## Mesh binary layout

No binary header. Little endian. Vertex count and submesh index offsets/counts are in `scene.json`. Each vertex has16float32 values (64bytes):

```
position.xyz, normal.xyz, uv0.xy, uv1.xy, uv2.xy, color.rgba
```

`uv1` is the authored lightmap UV; `uv2` is channel2 used by NatureManufacture material tiling/shape mapping. Missing UV channels fall back toUV0, missing vertex colors to white. Index buffers follow the complete vertex buffer. Each submesh lists byte `index_offset` and count; indices are unsigned32-bit triangle-list values (written from nonnegativeUnityint). `mesh.GetIndices` applies source baseVertex. The same geometry hash can have different material bindings on separate instances.

## Terrain binary layout

- height `.f32`: row-major `[z][x]`, normalized float32. Scale by `size.y`, apply terrain worldmatrix. Resolution is `height_resolution`.
- alpha `.f32`: row-major `[z][x][layer]` float32, dimensions in manifest.
- holes `.u8`: `[z][x]`,1means ground exists,0means terrain hole.

## Reference lighting and image processing

The exact original scene uses linear sky-blue fog, ambient intensity1.7, and baked indirect lighting with2048atlases,10texels/unit, AO1m and2bounces. The postprocessing profile enables ACES tone mapping, AO0.72, bloom1/threshold1.3/diffusion8, slight vignette0.153, auto-exposure key0.27 and screen-space reflections. These must be interpreted in the native renderer's units rather than blindly applying Unity intensity values. The current native flat or bright ambience is not a substitute for those shadowed crevices and bounce-light gradients.
