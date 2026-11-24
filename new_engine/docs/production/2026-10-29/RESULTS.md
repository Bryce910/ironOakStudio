> The later live-world build and its current measurements are in [live-world/RESULTS.md](live-world/RESULTS.md). This page preserves the earlier Studio v4 checkpoint.

# Production tools integration — 29 October 2026

**The complete commercial / AAA engine and editor are unfinished.** This pass builds and verifies a usable first version of the production foundations. Automatic asset residency, scene-wide bindings for the new animation/physics/gameplay systems, broader authoring workflows, visual polish and performance acceptance remain open.

These results describe the frozen **Studio v4 build**, SHA-256 `fc9f68f614864b421a2dc2610a01cd0e861ba556d7d81785203c977c55d9cb25`. A coordinated surface/material pass may subsequently change the shared working directory. That pass needs its own build, package and measurements.

## Built and connected

Use **F11 → Tool** in Studio. Existing terrain, foliage, object, light, material, spline, decal, atmosphere, import, NPC/navigation, particle, water and shader-graph editors remain available.

| Tool / system | Verified behavior | Current boundary |
| --- | --- | --- |
| Persistent identities and hierarchy | Studio v4 saves IDs, parents, inherited visibility/locks and source identity keys; old documents load; compaction and Undo retain ID high-water marks | XYZ, yaw and uniform scale; raw source-object hierarchy transforms have narrower support than portable instances |
| Connected prefabs | Capture, save/load, instantiate with fresh IDs and move a model with explicit light, actor route, fire and local-water attachments | Source models become portable added placements; shared dependency conflicts reject |
| Production session | Recoverable drafts, bounded backup generations, explicit recovery, blocked Save during Play, Keep/Discard, normal Apply restoration and retained pre-Play unapplied draft | Separate authoring files have separate histories; this does not provide complete scene-wide transient simulation snapshots |
| Advanced terrain | 16-bit height maps, erosion, slope/height biome stamps, graded roads and river-channel/bank sculpting, previews and protected foundations | Bounded local tiles; sculpting does not itself create full road/river meshes |
| Living world | Persistent schedules, factions, finite resources, bounded offline catch-up and a local learned economic policy | Logical state; physical home/work routes and general NPC decisions still need bindings. Closing the application uses catch-up on restart |
| Cinematics / spatial audio | Camera keys, scrubbing, FOV, authored zones/cues, real PCM playback, zone transitions, stereo reverb and owned-channel teardown | Camera host supports yaw/pitch/FOV; roll and richer tracks remain. WAV preparation is explicit |
| Regions / layers | Stable membership, saved metadata, visibility and locks pushed through Apply | Async residency coordinator is CPU-tested; automatic scenery residency adapter remains unfinished |
| Animation inspector | Real GLB rig import, clips, blend/crossfade, joints, sockets, deformed bounds and bone overlay; existing knight supplied | General GPU skinning passes native tests but is not yet attached to existing NPC crowds, combat states or scene motion |
| Physics inspector | Actual Jolt dynamics, collision layers/shapes, kinematic doors, impulses, queries, own Undo/Save and selected collider wireframes | Isolated preview; existing player collision and rendered dynamic props are not replaced |
| Gameplay inspector | Typed dialogue choices, quests, triggers, encounters, conditions/actions and persistent deterministic CPU preview | Commands still need dispatch into live NPC interaction, entity state and combat/training |
| Project production | Asset identities, dependency/provenance inventory, native schema validation, Linux packaging and strict frame-budget audit | Full runtime project inventory; Linux ABI/driver compatibility and other platforms need further validation |

The copied inspection knight is an identical copy of the existing project asset: 26 nodes, 24 joints and three tested clips. [Provenance](animation-inspector-asset-provenance.json).

## Verification

- Full executable build and production CPU suite passed. Meaningful checks cover ID deletion/reorder/Undo, prefab transforms and rollback, recovery/restart, terrain, world-state conservation, model-policy boundaries, typed gameplay queues/save-resume, spatial audio, packaging and frame-audit failures. [Build](full-build.log), [CPU suite](cpu-suite.log).
- Animation and physics passed strict compilation and AddressSanitizer/UndefinedBehaviorSanitizer tests. Physics passed 899 core contracts and 243 inspector/overlay checks. Animation tests include the real knight and 10,000 allocation-free successful samples. [Runtime review](runtime-module-review.md), [animation inspector](animation-studio-test.log), [physics overlay](physics-overlay-test.log).
- Native skeletal rendering passed on the Radeon RX 5300: skin deformation, materials, alpha/depth, reflection/shadow consistency, rejected-upload rollback, bindings and texture cleanup. This is correctness evidence. [GPU log](runtime-skeletal-native-gl.log).
- All eight new tool panels opened natively with zero GL errors and zero remaining shared texture references. The final private workflow passed **39 checks, five actual Applies and one intentional invalid-model rejection**, including prefab duplication, Play/Discard, logical-world rollback and v4 reload. An earlier fixture on a protected forest trail was correctly rejected; its failed attempt remains in the evidence. [Native results](native/results.json), [successful workflow](native/workflow-retry1.log).
- The original `world.studio` and `graphics.settings` were preserved. [Hashes](native/saved-input-proof.json).

## Walking performance — requirement remains unmet

RX 5300, 1600×900 display, saved 1120×630 scene resolution (0.7 scale), automatic quality off, 2048 shadows, 0.5 reflection scale, contact shadows, fog and clouds enabled. Native runs were sequential without concurrent builds, asset publication or GPU tests. All post-warmup intervals were retained.

| Completed third-person return walk | Average FPS | 1% low FPS | Frames below 45 | Worst frame |
| --- | ---: | ---: | ---: | ---: |
| Jungle, 432.63 m | 58.31 | 46.85 | 8 / 7,580 | 23.90 ms |
| Castle streets / tavern, 585.07 m | 59.81 | 42.16 | 47 / 10,380 | 44.76 ms |

Both walks completed with zero blocked/slope-blocked steps, zero GL errors and zero remaining texture references. **Both strict 45 FPS floor gates failed.** The castle also missed the 45 FPS statistical 1% target. No reduction in saved scenery/effect quality was used to hide these misses.

A separate initial generic castle run covered 599.67 m of its 806 m route before its frame limit. It averaged 59.95 FPS with a 50.37 FPS 1% low and four dips; it remains labeled incomplete. Editor workflow runs include explicit Apply/loading pauses and are not walking FPS evidence.

Worst intervals include presentation waits and some CPU render spikes. GPU stage queries were disabled for these timing runs; the counters do not identify an exact GPU bottleneck. [All runs and raw profiles](routes/results.json), [strict jungle gate](routes/jungle-third.gate.json), [strict castle gate](routes/castle-streets-tavern-third.gate.json), [unchanged frozen inputs](routes/freeze-after.json).

## Package and source checkpoint

The local Linux package contains 1,430 verified files, approximately 6.43 GB, including eligible application libraries and license notices. It was moved to `builds/relocated/Valwood Demo 2026-10-29` and launched from `/tmp` with zero GL errors or remaining texture references. The source workspace remained mounted; this does not establish a clean-machine compatibility matrix. [Build](package-build.json), [verification](package-verify.json), [relocated launch](package-relocated-result.json).

The package is the tested v4 snapshot. Later surface/schema changes require a fresh package. [Source checkpoint](tested-v4-sources.tar.gz) contains compiled source dependencies, production tools/tests and pinned physics sources; model assets are supplied separately. [Source manifest](tested-v4-source-manifest.json).

## Remaining work

1. Diagnose and remove the measured frame-time dips, then repeat full routes and longer editing/traversal sessions.
2. Connect general skeletal animation, collision/root motion, foot placement, physics bodies/doors and gameplay commands to reusable scene entities and actual NPCs.
3. Connect region residency to real CPU/GPU asset loading, unloading and upload budgets; test long traversal and memory pressure.
4. Finish material/terrain/road/river and full-transform workflows, animation state authoring, integrated diagnostics, project migration and recovery across all separate assets.
5. Complete scene composition, lighting/material consistency, character presentation, visual review, compatibility testing and a fresh release package.

Additional asset purchases are not required to finish these engineering connections.
