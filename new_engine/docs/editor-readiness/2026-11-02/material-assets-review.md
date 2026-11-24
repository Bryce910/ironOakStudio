# Asset, material and VFX editor review

Reviewed 2026-11-02. This pass covers CPU asset/import diagnostics, material and
surface authoring, material graphs, and VFX authoring. It does not certify the
native viewport or the complete editor. Production projects, shaders, catalogs,
art assets and application binaries were not changed.

## Defect reproduced and fixed

A background texture reimport remembered only the selected image's numeric
index. Undo/Redo could change or remove that image before the decoding job
finished. Completion then replaced the new occupant of that index, or treated
a missing replacement as a new image without enforcing the library cap.

The editor now captures the original image name, immutable revision path and
role. `world_surface::stage_image_import` rejects a stale replacement and
validates the complete candidate before publishing it. Successful replacements
keep the image's name/role and material references; new imports receive a
unique name and obey the 16-image limit. Failed staging leaves the document,
selection and undo history unchanged. The decoded immutable source can remain
unused on disk after a rejected job; it does not replace any scene resource.

Changed files:

- `engine/world_surface_authoring.h` and `.cpp`: atomic CPU staging helper.
- `engine/world_studio_surface_view.inc`: capture import identity and stage a
  validated result before creating one undo checkpoint.
- `tests/world_surface_authoring_test.cpp`: regression for a reused/removed
  target index, role mismatch, full library, duplicate name and invalid path.

## Checks run

All eight focused checks passed. The exact compiler commands and output are in
`material-assets-tests.log`; tests compile to uniquely named `/tmp` programs.
No shared `make`, application build or GL/GPU context was used.

| Check | Evidence |
| --- | --- |
| Static GLB inspection | Real crate: 6,576 triangles, one material, three maps; bounds and actual triangle picking passed. |
| Immutable GLB cooker | Original bytes preserved, unit conversion, LOD copies, revision reuse, hardlink opt-in and unsafe-path rejection passed. |
| Surface authoring | Atomic codec/role validation, paint seams/erase/budget, oriented projections, wind and spline math passed. New asynchronous image-staging regression passed. |
| Surface image filtering | Wrapped seams, packed mip tail, linear-light albedo reduction, normal variance and rollback passed. |
| Derived surface fields | Road/river removal, grade, bank/water queries, seams and failed-bake rollback passed. |
| Image import | Actual decoding, roles, immutable reuse, original-byte preservation, corrupted revision and escaping-folder rejection passed. |
| VFX authoring | All 41 checks passed: eight presets, persistence, old-save migration, recovery, position/ceiling bounds and preview clock. |
| Runtime/material graphs | Typed graph cycles/stages/SSA, persistence, shader-program invalidation and existing runtime contracts passed. |

The changed surface test was recompiled and rerun after the fix. A separate
`g++ -fsyntax-only` pass of the actual `world_viewer.cpp` translation unit checks
the ImGui include integration without writing an application/object binary;
it passed with no diagnostics (exit0), recorded in
`material-assets-ui-syntax.log`.

## Designer workflow and limits to communicate

- Static scenery imports use embedded **GLB**, with UV0 and baked texture
  transforms. FBX, Unity packages and Unreal projects need the offline source
  conversion pipeline. Skinned heroes use the character pipeline.
- Use metre units or set the import's metres-per-source-unit field. Preserve
  originals and use staged reimport/Apply; this creates an immutable revision.
- The import inspector accepts at most 128 MiB per GLB, one million vertices
  and triangles, 128 triangle primitives and 128 embedded images. Large
  environments need sensible separate assets and LODs.
- Static material maps use albedo/emissive in sRGB and normal/ARM in linear
  space. ARM is **AO red, roughness green, metallic blue**. Separate AO is not
  an independent native material channel. Transparent BLEND uses a cutout
  fallback; sorted glass is not available in this importer.
- The surface library supports 16 images, eight PBR layers, 256 paint strokes
  and 64 local paint tiles. A model surface binding affects every instance and
  LOD of that alias. Choose the DirectX-green toggle per surface layer when
  importing a DirectX normal map into the image library.
- VFX Studio has eight starter presets and up to 16 authored effects. These
  are continuous emitters, with preview pause/restart/scrub/solo and saved
  appearance/motion settings. The game also has separate combat VFX; Studio
  is not an arbitrary particle-node or event-sequencing editor.
- Material graphs allow eight targets with 32 nodes each. They compile on
  Apply; visual vertex offsets do not move collision.

## Native verification still required

The parent owns native editor QA. The final application build and viewport
rehearsal must confirm texture import/reimport with Undo, material Apply and
rollback, terrain paint/projection appearance, and VFX preview/selection/save
and reopen. This CPU review does not measure GPU allocation, shader linking,
frame pacing, or interactive panel usability.
