# Streamed-world Studio and material/lighting pass

The custom SDL2/OpenGL engine now has a working authoring path into the large Valwood world. The earlier Seldonne editor is preserved in `Launch_Legacy_Studio.sh`; `Launch_Studio.sh` opens the new Studio at the castle. See [the guide](../STREAMED_WORLD_STUDIO.md) and [the implementation plan and remaining systems](PLAN.md).

## Material and lighting changes

- Imported models now retain glTF `normalTexture.scale`; the purchased library contains materials authored at 0.55/0.65 strength. Previously the model loader ignored this field. Normal variance is measured before strength scaling so roughness filtering remains consistent.
- Layered opaque facade color maps use owned 8× anisotropic sampler objects. Shared image storage, original wrapping/mip policy and data-map filtering are preserved. This avoids the duplicate texture storage produced by a broad texture-cache-policy change.
- Resolved detail sharpening is bounded by local contrast and relative highlight limits, reusing the existing five post-process taps. It does not recover detail absent from a source image or the chosen rendering resolution.
- The castle look receives a small sky-fill/exposure lift. Studio exposes persistent sunlight, sky fill, exposure, time/bearing, cloud, rain, wind and mist controls. The rainforest retains its local overcast, fog and canopy response.
- Optional material overrides, projection boxes and spot-light code compile only when the authored document uses them. The additional lights share the existing eight practical-light slots and do not add shadow passes.

The source texture cache returns to 357 image entries / 405.823 MiB at scene load. This is source-cache accounting, not total GPU memory. Characters, render targets, terrain, compressed images and other resources have separate costs.

## Editing and safety checks

Terrain raise/lower/flatten/smooth, separate grass/tree/rock/fern density masks, cooked-model placement/scatter/transforms, point/spot lights, alias material overrides, placement splines, procedural projected surface patches and atmosphere controls commit into the live streamed scene. Save/load and 32 undo/redo snapshots use a versioned authoring document.

Structural Apply joins terrain workers, validates/prepares the replacement scene, reuses unchanged model geometry, rebuilds grounding/collision and environmental masks, invalidates weather heights and local lighting, and resumes streaming. The edit-height component follows the rendered triangle diagonal; saved tile edges must match. Source-scene identity prevents stale override indices from hiding/moving a different purchased object after the base scene changes. Light/material-only changes avoid model reimport and full geometry rebuilds.

Native QA accepted two commits, rejected an invalid model and an invalid shader, exercised Undo, and saved/reloaded an isolated document. It verified real scene instance counts, hidden-source changes, a changed terrain height and the independent foliage mask. Reload retained the terrain, five added objects, two lights, a material override, a projected patch and a spline. A deliberately changed source identity correctly retained the base scene and staged the saved draft for repair.

The workflow reused 204 model groups. Its largest explicit Apply frame was 1077.73 ms; this pause is editor work and is excluded from the walking results below. The earlier full geometry import path took roughly 3.5 s in the same workflow. This is an observed improvement, not a universal bound on edit time.

## Native walking results

The final measured table is recorded in `release/results.json`; per-frame CSVs, sidecars, logs and captures are alongside it. Tests use the AMD Radeon RX 5300, 1600×900 output, saved 70% render scale (1120×630 scene), 2048 shadows, half-scale reflections, contact shadows, volumetric fog and clouds. Auto quality stays off. Automated capture uses dummy audio and frame-time-only profiling; the normal Studio launch restores the ordinary audio driver. The first 120 frames are excluded. The 1% low is the inverse mean frame time of the slowest 1% of measured frames.

| Scene / camera | Average FPS | 1% low FPS | Slowest frame | Frames below 45 FPS |
| --- | ---: | ---: | ---: | ---: |
| Full jungle loop, first person | 58.86 | 48.94 | 22.51 ms | 2 / 7580 |
| Full jungle loop, third person | 58.64 | 48.42 | 22.03 ms | 0 / 7580 |
| Castle route, third person | 59.95 | 54.89 | 18.51 ms | 0 / 1780 |
| Reloaded edited jungle, stationary Studio view | 49.66 | 44.70 | 22.52 ms | 2 / 240 |

These are scripted route measurements, not a guarantee for every location, future edit or other application workload. The jungle loop is 432.63 m and completes in both camera modes with zero collision/slope-blocked steps; this is the complete authored return route, not only its initial segment. Individual frame times above 22.22 ms remain visible in the results. A strict always-above-45-FPS floor is not established.

All native cases report zero GL errors and zero remaining shared texture entries/references after shutdown. Tests use private temporary authoring documents; the user's source scene and saved graphics preferences are preserved. See `release/preferences.sha256` for the saved-preferences fingerprint.

## Repeat verification

Close the live engine first. Build and run contract tests before native profiling; avoid compiling or running another game instance while collecting frame times.

```sh
make -j4
make -j2 test-world-studio test-world-cover test-world-scenery test-world-models test-world-material-samplers test-world-post test-world-weather test-world-lighting
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/validate_world_studio.py --output /tmp/valwood-studio-validation
```

The targeted CPU/rendering tests passed, including terrain seams/triangles, malformed persistence, independent masks, geometry-transfer ownership/abort behavior, source normal strength, sampler ownership/reset, weather invalidation, post-processing and production lighting shaders. GL contract tests use software rendering for deterministic validation; the walking results above use the native GPU. The lighting test's path-specific fixture now supplies contrasting soil instead of identical gray soil/grass; the production assertion remains intact. This is not a claim that every historical repository test passes.

## Remaining limits

AAA visual fidelity is still a production target. The full commercial editor suite remains unfinished: general asset import/reimport diagnostics, LOD/collision authoring, HDRI environment import, a true shader graph, arbitrary texture decals, NPC/navigation, particle/water/wind-volume authoring, viewport transform gizmos, multi-selection/prefab attachments and packaging. The [plan](PLAN.md) lists completion gates. Current projection boxes are procedural patches, current object selection uses roots, and terrain sculpting is bounded to 16 local 128 m tiles while protecting fixed demo foundations/routes.

Moving a source building does not move its separately authored NPC routes, FX/torch sockets or paving. The existing assets remain adequate for testing this authoring foundation; importing more meshes alone does not complete the renderer, editing workflow or visual art direction.
