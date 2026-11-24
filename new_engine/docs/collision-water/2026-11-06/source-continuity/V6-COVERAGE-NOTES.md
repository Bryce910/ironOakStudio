# Collision/water v6 acceptance provenance

This note distinguishes inherited coverage from final-binary testing. It is not an acceptance record. The root agent owns final runtime results and installation; `CURRENT.json` is written only after those steps. A `READY.json` proves a completed build and exact plan linkage, not gameplay or visual acceptance.

The exact v6 source delta from v5 is three files: authored-pond uniform dispatch in `world_viewer.cpp`, pond creation absorption defaults in `world_terrain_authoring.cpp`, and authored-pond optical absorption in `world_water.fs.glsl`. The water vertex shader, collision geometry/physics service, structural history/source pose reconstruction, source-floor NPC navigation and playtest support lifetime code remain byte-identical to v5. `../structural/V6-BUILDER-REVIEW.json` records the delta and both read-only build graph inspections.

The completed parent-v5 checks, reported by the root agent, include native City structural move/Apply/Undo/Redo/save/reopen, native Grandstone NPC source-floor authoring, and the full nine-route Grandstone obstacle run. These are parent-v5 runtime checks; they must not be presented as nine-route tests of the final v6 binary. Parent evidence lives in:

- `../structure-native-city-v5/`
- `../npc-native-grandstone-v5/`
- `../obstacles-native-grandstone-v5/`
- `../smoke-native-jungle-v5/`

The final-v6 runtime plan targets native water editing/reflections and visual review, a four-route Grandstone subset retaining all 24 source collision worlds, and the connected journey. The Windows plan additionally targets structural history/save/reopen, water, NPC authoring, the four-route Grandstone subset, Jungle startup, and the connected journey. At this note's creation these are planned checks, not completed results. Root must reference their actual receipts, outcomes and remaining limitations in CURRENT/delivery records after execution.

The independent CPU source-geometry audits remain relevant to unchanged v6 collision/navigation code: all source architectural inventories, 11,542 dense movement steps, 1,559 floor/solid-leaf probes, eight NPC source-floor routes, and 24-partition support were exercised. These CPU checks are separate from GPU/runtime coverage. Their versioned receipts retain their original source hashes and must not be silently relabeled as new v6 runs.

Physical Windows hardware acceptance is separate from a Windows executable running under Wine. Delivery notes must identify which environment actually ran the final executable.
