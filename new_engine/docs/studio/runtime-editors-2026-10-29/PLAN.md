# Runtime editor pass

Implement four connected tools in the custom engine, sharing the existing draft/Apply/save/history workflow. Preserve the current demo, purchased sources and saved graphics preferences.

1. NPC/navigation: bounded editable additions and source-resident overrides, appearance/name/speed/dwell, waypoint patrols, local obstacle-aware path baking and viewport overlays. Reuse loaded pose batches and distance-driven gait; no per-frame pathfinding, file access or actor allocation. Existing demo routes remain the default.
2. Particles: edit existing emitters or add smoke/dust/fire, size/volume/intensity/warmth/timing, with optional fire practical light. Reuse fixed staging and draw budgets; show authored bounds and clear source identity.
3. Water: existing ocean/pool/moat appearance plus bounded local elliptical ponds. Connect custom water to cover/navigation/weather and use opaque depth/refraction and the existing single planar-reflection budget. Fixed legacy shore geometry stays protected.
4. Shader graph: actual node canvas/connections, typed bounded DAG validation, PBR and bounded visual vertex-offset outputs, GLSL generation, model/LOD/shadow integration, atomic compilation rollback. Compile only during Apply; source-based shader reload remains available.

Version the document while retaining v1/v2 loading. Validate malformed inputs, disconnected routes and cycles, keep undo/load and failed Apply safe, and exercise saved reload in a private native workflow. Test meaningful CPU/GL contracts and native edited/full routes sequentially on RX5300, recording average/1% lows/budget misses and GL/ownership results. Do not build or run other heavy tasks during FPS measurements. Reopen the finished Studio at the castle.

This first working pass has explicit object/node/area and GPU limits. It is not a promise of arbitrary navmesh topology, a fluid solver, all particle types or a general rendering-language compiler. The controls must actually drive their runtime systems.
