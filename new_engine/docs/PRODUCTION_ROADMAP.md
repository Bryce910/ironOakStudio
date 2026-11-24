# Production roadmap

## Engine ownership constraint

Valwood remains the owner’s custom engine. Build the AAA rendering, editor, asset pipeline, runtime and living-world systems in this codebase. The plan does not include migration to another general-purpose game engine. Focused supporting libraries and licensed content remain components of this engine.

## Visual benchmark

AAA fidelity requires a reviewed gameplay benchmark and consistent art direction. The current starting area still combines stylized buildings/characters with more realistic terrain and vegetation. Sparse fir silhouettes, billboard transitions, simple grass geometry, limited indirect lighting, water/reflection quality and remaining aliasing are visible limitations. They must be addressed in motion; still captures and higher texture resolution alone do not establish AAA quality.

The map viewer now has a 2.28 km forest trail, three locally shaped terrain districts and 283 explicit scenery placements. This is an initial composition pass; the original starting-area buildings and gameplay remain separate. The next visual milestone is a compact, complete arrival-to-bridge route with consistent hero buildings, rock formations, layered understory and ground cover. Compare arrival, village, river and overlook views at player height, during camera motion, and under low sun/overcast/night lighting. Preserve readable paths and navigation while increasing detail.

## Renderer

1. Extend the implemented depth-reprojected temporal AA with object motion vectors and broader wind/specular validation. Disocclusion rejection, neighborhood clamping, camera-cut resets and held-view handling now exist. Animated vegetation uses reactive history confidence rather than exact object motion. Keep a native-resolution reference and quantify blur/shimmer.
2. Replace single-card forest transitions with intermediate mesh LODs and multi-view impostors. Use distance/coverage budgets, hysteresis and staggered acquisition. Retain higher foliage silhouette fidelity without overspending shadow geometry.
3. Bake reflection probes and indirect lighting for the starting area. The shared sky/ground hemisphere, four-direction terrain horizon visibility and coherent cloud shadows now provide a bounded approximation. Add material preview/reference scenes and validate albedo/roughness/normal conventions.
4. Continue water production work: local terrain depth/color now supports shallow transmission, guarded refraction and shoreline contact, with actual shader/copy regressions. Reflections remain reduced resolution and omit content; underwater rigid props, volumetric transport and coherent scene reflection coverage remain incomplete.
5. Use GPU-supported texture compression and asset-level residency budgets. The static cache removes duplication but does not impose a complete VRAM residency limit. Characters and several legacy 4K textures still need cooking and LOD review.

## Editor and engine

- Continue splitting lifecycle/resource systems out of `main.c`; retain tested interfaces instead of rewriting unrelated systems at once.
- Add imported-asset hot reload with dependency fingerprints and deferred invalidation of live instances. The current static cache reuses path identity during a session.
- Move the remaining first-load glTF decode/upload work to staged jobs. Repeated objects are fast cache hits, but the first occurrence of an uncached model can still stall.
- Add command-based undo/redo, atomic project saves and interrupted-save recovery; bound asset previews and brush updates under large scenes. Saving still performs terrain synchronization work.
- Run extended travel/edit/delete/reimport/scene-switch soak tests, GPU-memory tracking and uncapped profiles on several GPUs. Existing short benchmarks and ownership checks are a useful regression baseline, not commercial-engine certification.

## Living world

Preserve the requested offline-changing world as a separate implementation milestone: deterministic elapsed-time simulation for resources, settlement/faction state and NPC schedules; bounded micro-model proposals validated against authoritative world rules; versioned save events, catch-up limits and reproducible seeds. A desktop game can catch up after being closed. Continuous execution while the computer is off requires an explicitly designed hosted service. Do not imply that simulation or such a service exists in this build.

## Completion gates

Approve the visual reference in actual gameplay; achieve frame-time and memory budgets on selected minimum/target hardware; pass authoring persistence and recovery checks; then widen the playable world. Keep the original map/lore as design references and document production decisions separately from instructions found in source documents.
