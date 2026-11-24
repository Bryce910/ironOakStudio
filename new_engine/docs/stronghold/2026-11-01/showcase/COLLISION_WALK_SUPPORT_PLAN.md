# Source showcase collision and multilevel walking

Read-only implementation audit. No collision assets, viewer movement or physics limits were changed for this pass.

## Current state

The isolated project now has113 actual source chunk placements distributed over its nine scenery files (12 or13records each). The14th placement field is the absolute Y pivot. All113 `world.studio` asset definitions deliberately specify collision0. Thus source walls and upper floors are visual geometry; changing that number to1 would turn entire spatial chunks into solid footprint boxes and close their streets/doorways.

Current source catalog geometry:

| Category | Chunks | Mid triangles |
|---|---:|---:|
| Architecture/built |33|2,630,510|
| Geology |65|2,719,154|
| Backdrops |15|29,273|

The source already contains authored colliders. Of307 source prefab files,257 contain collider components; raw expansion of all prefab instances counts10,664MeshColliders and79BoxColliders, before active-hierarchy filtering and scene overrides. The original scene additionally serializes278MeshCollider and40BoxCollider components directly. These are useful source data, but cannot be registered as thousands of bodies under the existing512body budget.

TerrainData contains7,132hole cells among4,194,304cells (2048²). Their sourceXZ bounding rectangle is approximately[499.0,762.7]×[303.7,662.6]metres. The current height installation copies heights but does not carry hole validity. A heightfield alone cannot represent floors under bridges or cave interiors.

## Existing native APIs to reuse

- `engine/world_physics.h:24`: `Body` supports static triangle meshes with local vertices and double world translation.
- `engine/world_physics.h:43`: `load` / `validate` / `save` already support a bounded `VALWOOD_PHYSICS 1` document.
- `engine/world_physics.h:72`: `World::configure` creates a scene atomically; failures preserve its previous scene.
- `engine/world_physics.h:83`: `World::raycast` returns the closest hit position and world normal.
- `engine/world_physics.h:84`: `World::cast_capsule` performs a real Jolt capsule sweep. Shape casts collide with mesh backfaces (`world_physics.cpp:272`), which is useful for ceilings and bridge undersides.
- `world_physics.cpp:172-177`: existing eight collision layers filter queries using `query_layer` and reciprocal masks. No new Jolt query API is required to separate floor support from wall sweeps.
- `world_runtime_components.h:120`: the component runtime already forwards these queries. A dedicated static source `world_physics::World` is simpler here because imported scenery has no matching runtime entity bindings and should not consume gameplay bodies/IDs.

Keep existing limits:65,536vertices,131,072triangles,512bodies,16MiB serialized document. These geometry limits are **global per configured world**, not per mesh. Independent worlds and owner-thread static queries are already exercised in `tests/world_physics_test.cpp`; no dynamic simulation or per-frame `step` is necessary for a static query-only source scene.

## Smallest sound implementation

1. **Cook a dedicated collision proxy**, independent of render LOD and material data. Start with the33built chunks and active authored source colliders; discard decorative/backdrop/irrelevant geology. Preserve source world transforms and absolute elevations. Weld position duplicates, remove degenerate/duplicate internal faces, dissolve genuinely coplanar regions while retaining boundary loops, then selectively simplify decorative relief. Floors, stair landings, door arches, bridge openings and parapet boundaries are constraints. Replace source stair flights with measured continuous ramps where their original colliders do not already provide them. Whole-chunk boxes, whole-chunk convex hulls and a topmost2Dheight raster are unsuitable.
2. **Split proxy faces by function**, using their transformed geometric normals: walkable upward faces (normalY≥0.65) on one body layer; walls, ceilings and steep faces on another. Multiple bodies may share a spatial chunk;66bodies for33chunks remains well below512. Assign query layers for support-only, wall-only, and all-surfaces queries, with reciprocal masks enabled.
3. **Configure one optional static source world** while the project/destination loads. Keep source render aliases collision0. Query it in addition to existing runtime/scenery collisions. Do not increase generic Jolt budgets. A dissolve pass is not guaranteed to reduce2.63million triangles under131k; measure the result before claiming whole-scene coverage. If it cannot meet both geometry and passage-quality requirements, cook bounded district/presentation-route proxy sets and switch them only at explicit loading boundaries. Expose the physical coverage in metadata.
4. **Add a contextual support query** such as `support(x,z,currentFeetY,stepUp,maxDrop) -> {found,y,normal,body}`. Cast downward from just above the current feet (roughly0.35m step allowance), over the relevant vertical movement interval. A player below a bridge must not snap to a deck many metres overhead. Use the source support layer, then consider real terrain only where TerrainData says ground exists. Pick the nearest reachable support, retaining independent stacked floor levels.
5. **Integrate horizontal and vertical camera motion** as described below. Keep the existing capsule dimensions (radius0.37m, cylinder half-height0.50m, centre near feet+0.9m). First implementation should use ramp proxies for stairs; an unconstrained visual stair mesh would require a deliberate step-up/forward/step-down controller.

## Required movement integration points

`world_viewer.cpp:557` currently defines walking floor as `max(atlas_walk_height(x,z),0)`. It has no reference feet height and therefore cannot select a floor in stacked geometry. Keep that base terrain query for existing projects, and introduce a source-project contextual helper for camera motion.

`world_viewer.cpp:570-602` (`move_camera`) currently checks continuous terrain slopes and box/scenery collisions. It also makes a closest-hit runtime capsule cast, then ignores hits with upward normals. That is insufficient for a merged triangle scene: an initial floor hit can be the closest result and hide a wall farther along the same sweep. Querying only the proxy wall layer for horizontal motion avoids this. Test the support slope on the selected floor, and use the same support context for full and sliding-axis attempts. The forward slope probe must be allowed to query a corresponding larger rise window; otherwise its0.75mprobe can lose a legal ramp simply because the per-step window is smaller.

`world_viewer.cpp:604-622` (`move_camera_vertical`) currently checks only `world_scenery_can_move`; runtime Jolt geometry does not stop downward or upward motion there. Add a full vertical capsule sweep against all proxy surfaces, stop at the hit fraction with a small skin, ground on walkable upward normals when descending, and cancel upward velocity on ceilings. Resolve reachable support/terrain without forcing terrain back into source holes.

The ordinary gravity call at `world_viewer.cpp:4229-4233`, route grounding at approximately1104/1119, initial grounded spawns/teleports, and follow-camera obstruction callbacks also assume atlas-only floors. These must use the appropriate contextual source support when applicable. Snow and footsteps already consume the resulting camera feet/grounded state, so they should not separately invent a new floor.

The existing navigation `Query::ground(x,z)` is2D as well. Upper-storey NPC navigation requires a floor-constrained query or separate navigation zones; a camera walking pass alone does not establish whole-scene NPC navigation.

## Necessary correctness checks

- Walk across a bridge deck, then beneath the same bridge: two distinct support heights, no snapping between them.
- Climb and descend a source stair ramp/landing, including a diagonal approach and axis slide.
- Stop at walls and parapets while doorways/arches remain passable.
- Hit a low ceiling when jumping; land on an intermediate floor during a fall without tunnelling.
- Enter a terrain hole/cave without an invisible heightfield floor or ceiling.
- Preserve exact fallback outside physical coverage and reject corrupt/over-budget proxy data atomically.
- Run the native route on the final proxy and record collision/slope counts, grounded heights, query allocations and frame pacing. Report visual-only areas separately until their walk routes pass.
