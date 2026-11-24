# Runtime authoring editors — 29 October 2026

The custom streamed-world engine now has working NPC/navigation, particle, water and model shader-graph tools. All four use the existing draft → Apply → Save workflow and undo/redo. Production `world.studio`, purchased source placements and graphics preferences were preserved; the test examples are separate documents.

## Implemented

| Tool | Runtime connection | Budget / scope |
| --- | --- | --- |
| NPC / Nav | Existing appearance library, added residents and source overrides, stationary actors or waypoint patrols; swept capsule/water/slope checks and local A* detours baked on Apply; animated walking, eased starts/stops/turns and viewport overlays | 32 additions/overrides, four local areas, 32 authored points / 128 baked corners per actor; no per-frame pathfinding or model loading |
| Particles | Existing smoke, dust and purchased fire/embers; source overrides and new emitters; bounds, intensity, timing/size/wind controls; optional synchronized fire practical light | 16 editor records, shared 512 smoke/dust instances, 56 flame emitters and eight active practical-light slots; no added shadow passes |
| Water | Ocean/pool/moat appearance controls and new sculpted elliptical ponds; water queries, cover/weather/navigation, refraction and environment reflection | Eight water records, 1–32 m custom radii, 220 m nearby rendering; existing single planar-reflection budget |
| Graph | Interactive typed node canvas/pins/connections and numerical controls; generated GLSL for model PBR channels, emission and bounded visual vertex motion; color/LOD/shadow integration; atomic program replacement | Eight alias targets, 32 nodes each, 16 node types, ±15 cm vertex offsets; intermediate math bounded to avoid overflow |

Version 3 documents round-trip all new records; versions 1/2 still read. Invalid documents, route endpoints, disconnected paths, floating pond edges, graph cycles and shader compilation failures preserve the committed scene. Structural Apply pauses editing to join workers, prepare collision/grounding and rebuild required caches. These loading/compilation pauses are excluded from claims about walking performance.

Shader program-location caches now carry a generation counter. This prevents stale uniform locations after the driver reuses a deleted program ID during repeated reloads, across static models, characters, animals and training/camp renderers. Unassigned graph aliases retain their source material behavior. QA captures no longer write the user's Studio window layout.

## Verification

- `make -j4 rpg test-world-studio test-world-studio-tools test-world-runtime-editors`: full executable build and CPU contracts passed. Includes obstacle detours/disconnected routes/water rejection, bounded navigation, sloping/seam-crossing pond carving, malformed graph/stage validation, v3 round trip and recycled-program cache invalidation.
- Native AMD tests: shader graph, population CPU/GL, scenery CPU/GL, smoke/dust (default and purchased art), water and weather passed. Graph tests execute production shader variants, test actual tint/emission/motion pixels, unassigned identity, and an extreme repeated-multiply chain staying finite. Population regression includes original resident trajectories, continuous gait, fixed batches, atomic configuration rejection and no hot allocations.
- Native editor workflow: four successful Applies, four rejected edits (submerged route, invalid pond, graph cycle and intentional GLSL failure), undo repair, v3 save/reload, and actual PID/executable-verified clicks on **Compile graph / Apply** and **Expand graph workspace** passed. Runtime inspection reports 3 resident records, 1 local navigation area, 2 particle records, 2 water records and 2 graphs. Reload retained these counts. The intentional GLSL error in the workflow log is the rollback test.
- Real scene captures show the committed pond, navigation test area and fire. All captured sessions finish with zero GL errors and zero remaining shared texture references. No new model/image purchases or imports were required.

Build/test logs: [final build](final-build.log), [CPU contracts](cpu-contracts.log), [native regression contracts](native-contracts.log), [graph GPU execution](shader-graph-native.log), [editor workflow/reload](native/results.json). [Protected placement source hashes](source-proof.json) match the existing demo manifest. [Saved scene/settings hashes](saved-state-proof.json) also match their before snapshots. [Implementation hashes](implementation-sha256.json) identify the tested sources and executable.

## Walking performance

RX 5300 native hardware renderer, 1600×900 display / 1120×630 scene (saved 0.7 scale), 2048 shadow maps, reflections at 0.5, contact shading, volumetric fog and clouds enabled. Automatic quality stayed off and the render scale never changed. Runs were sequential without compilation or asset processing alongside them.

| Completed route | Average FPS | 1% low FPS | Frames below 45 / measured | Worst frame (ms) |
| --- | ---: | ---: | ---: | ---: |
| Jungle, first person | 58.63 | 48.66 | 1 / 7,580 | 22.312 |
| Jungle, third person | 58.37 | 47.56 | 2 / 7,580 | 24.157 |
| Castle streets / tavern, third person | 59.95 | 52.69 | 6 / 11,880 | 25.043 |
| Castle arcade, first person | 59.95 | 45.52 | 12 / 3,880 | 23.847 |

21 of 30,920 measured frames fell below 45 FPS (0.068%). Both jungle runs completed the 432.63 m return walk; all four walks completed with zero blocked/slope-blocked steps, GL errors or remaining texture references. These measurements support the 45–60 FPS target for the tested routes; individual brief dips remain, so they do not establish a universal 45 FPS floor. Explicit Apply and shader compilation pauses are separate loading operations.

Raw profiles, per-frame CSVs, route state and captures are in [routes/](routes/results.json).

## Captures and example documents

![Graph workspace in the real engine](native/workflow.png)

[Water controls](native/water.png), [NPC/navigation controls](native/npc.png), [particle controls](native/particles.png).

![Committed sculpted pond](native/water-world.png)

[New resident placement](native/npc-placement.png), [navigation test area](native/npc-world.png), [fire scene](native/particles-world.png). The placement inspection temporarily holds the added actor at its first waypoint so it stays in front of the fixed camera; the profiled example keeps both added patrols.

`native/editable-example.world.studio` is the isolated workflow fixture. `native/edited-demo.world.studio` combines those test features with the existing 13 material overrides and seven surface patches for representative route checks. These are QA examples, not an automatic replacement for the player's scene.

To repeat the workflow with the live engine closed:

```sh
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/validate_runtime_editors.py \
  --output /tmp/valwood-runtime-check

env VALWOOD_TEST_NATIVE_GL=1 python3 tools/validate_runtime_editors.py \
  --output /tmp/valwood-runtime-routes \
  --document docs/studio/runtime-editors-2026-10-29/native/edited-demo.world.studio
```

The harness uses a temporary document and the native renderer. It does not cook assets or change saved graphics settings. Compile before profiling; do not run competing engine/test contexts or asset processing alongside walking measurements.

Studio was reopened at the castle using the production document and normal PulseAudio. [Launch proof](live-launch-proof.json) records the verified executable/PID/window and clean native startup. Use **F11 → Tool** to select the new editors.

## Practical limits

These are first working tools in the custom engine, not a completed commercial editor. Navigation is static/local, without dynamic crowd avoidance or global navmesh streaming. This resident editor does not retarget animations or change the independent bandit, animal or training AI. Particles expose three supported effect families. Water is surface/basin authoring, without fluid simulation or spline rivers. Graphs target authored model aliases, without arbitrary texture nodes, terrain/NPC/water targets, new render passes or collision deformation. Wind volumes, arbitrary texture decals and project packaging remain future work.

The 3 GB GPU budget still applies. More visible actors, particles, graph work or large imported assets can change frame rate. A measured route does not guarantee a 45 FPS minimum at every location or with arbitrary edits. See the [Studio guide](../STREAMED_WORLD_STUDIO.md) for controls and limits.
