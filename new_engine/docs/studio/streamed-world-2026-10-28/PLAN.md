# Streamed-world Studio implementation

The custom engine remains SDL2 / OpenGL 3.3. Target: the existing castle, tavern and jungle at 1600×900 output using the user's saved quality settings, with walking frame times within22.22ms (45FPS) and a60FPS cap. Average FPS alone is insufficient; record1% lows, frames exceeding22.22ms, GL errors and steady-state allocations. Loading/edit rebuilds are reported separately.

## Order and acceptance

1. **Materials and lighting:** preserve source normal-map strength, sharpen resolved texture detail with bounded highlights/edges, improve angled opaque texture filtering, expose persistent sun/sky/exposure controls. Retain temporal stabilization and the rainforest's cloudy, misty canopy response. Compare native captures and identical settings before/after. Stronger filtering must earn its GPU cost.
2. **Shared authoring document:** validated atomic saves,32 undo/redo snapshots, separate draft and committed copies. No disk access or document mutation on terrain workers. Join workers before replacing height callbacks. Failed scene preparation must leave the previous committed scene/collisions active.
3. **Terrain / foliage / lights in the real streamed world:** local raise/lower/flatten/smooth, density masks, model scatter and point/spot controls. The same committed terrain drives mesh generation, grounding, collision and weather. Keep source/model/light budgets bounded. Protect fixed demo foundations and NPC navigation from terrain sculpting.
4. **Level-design tools:** browse the cooked model library; place, move, rotate, scale, hide and delete instances; alias material overrides; linear and Catmull-Rom spline placement; bounded procedural projection boxes. Rebuild roof, canopy and lighting caches after accepted structural changes. Preserve purchased source placement files.
5. **Remaining commercial-editor systems:** general arbitrary-model import/reimport with material diagnostics and per-model LOD/collision authoring; arbitrary texture-decal import; HDRI import plus filtered environment lighting; a genuine node-based shader compiler with safe rollback; NPC/navigation authoring; particle/water/wind-volume authoring; transform gizmos/multi-selection, scene hierarchy and cross-project packaging. Existing legacy tools are preserved, but their presence does not imply streamed-world integration. These require additional implementation and validation; tabs alone are not completion.

## Budgets

Local sculpt edits:16 tiles of128m,2m samples and±32m displacement. Added models:512, within the existing8192 total /4096 per-alias caps. Foliage masks:32. Editor light definitions:24, sharing the existing eight active practical-light slots with torches. Material overrides:64. Surface patch definitions:32, nearest eight within160m render. Splines:16,32 control points each; baking respects the512 added-model cap.

## Verification

CPU contracts cover terrain seams, independent foliage layers, deterministic scatter, spline spacing, undo/redo, save/load round trips, malformed data and budget failures. Rendering contracts cover model materials, mip/sampler policy, post-processing, depth parity and ownership. Native tests compare castle and jungle at saved settings, then exercise a real commit, rejection, undo, save and reload in an isolated QA document. The ordinary user scene/preferences must not be overwritten by tests.

## Completion gates for the remaining editor suite

These are pending systems, not hidden features of the current tabs. Keep the existing streamed-world authoring usable while adding each one.

| System | Required behavior before calling it complete |
| --- | --- |
| Import/reimport and asset diagnostics | Import into the project without modifying purchased originals; show missing maps, color spaces, units, bounds, triangle/texture costs and incompatible animation rigs. Reimport must invalidate the matching cached asset, rebuild dependent instances safely, and restore the previous working asset after failure. |
| LOD/collision authoring | Preview each actual LOD, set transition distances and collision shapes, validate doorway/capsule clearance, and measure the resulting draw/texture budget. Saving must retain the settings across streaming and reload. |
| HDRI/skybox | Import a panorama/cubemap, preview rotation/exposure, generate diffuse irradiance and filtered specular levels, and match sky display to scene lighting. Test seams, roughness levels, linear color, ownership and the existing16-sampler budget. |
| Shader graph | A typed node document, real GLSL compilation, parameter/texture binding, normal/alpha/PBR outputs and safe failure rollback. Animation/time inputs must retain stable depth, shadow and motion-vector behavior. A graph preview alone is insufficient. |
| General decal textures | Import and validate albedo/normal/roughness channels, apply them through real projection boxes, handle receiver/layer rules and mipmaps, and measure overlap cost. Current procedural patches remain available. |
| NPC/navigation | Place actors, preview animation/retargeting, edit routes and states, validate terrain/building capsule clearance, and bind attachments to moved structures. Existing fixed demo routes must remain stable until explicitly converted. |
| Particle/water/wind authoring | Edit real emitter/volume/shoreline data, preview changes, save/reload them, and validate bounds, roof exclusions, collisions, shared light/particle budgets and weather-cache invalidation. Global atmosphere sliders do not complete these editors. |
| Selection and transforms | Mesh/bounds picking, viewport translation/rotation/scale gizmos, multi-selection, grouping/prefab attachments and search across source and added objects. Each drag is one undo transaction and must update grounding/collision after Apply. |
| Project and packaging | Dependency browsing, missing-reference diagnostics, versioned project settings, deterministic packaged builds and reload checks on a clean destination. Include authored documents, model/material dependencies and license provenance. |

After each system, rerun the complete432.63m jungle loop in both cameras and the castle route with unchanged settings; report averages,1% lows and individual budget misses. A new feature that fits the draw budget but introduces edit-time corruption, streaming stalls or unbounded allocations does not pass.
