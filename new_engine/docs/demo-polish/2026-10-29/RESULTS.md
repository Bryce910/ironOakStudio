# Castle/tavern and jungle demo polish

Final combined executable: `2e1ada962e698b9df92e87244ce7a662ccb9b4936ced0dafae296f45439311ec`.

## Implemented

The runtime has 146 measured additions to the existing 535-row polish source. Original rows, IDs and order were retained. Additions include correctly matched baker, herbalist and tavern signs; trade possessions and foundation plants; 18 supported tavern table objects; 90 composed jungle plants; 18 dry-bank reeds and four canopy trees. Six small near/mid sign models reuse resident source images. The deterministic composition recipe enforces measured support, path/actor clearances and a 150-addition budget. The private extra-palm comparison was rejected and was never published.

The jungle ridge material mask exposes smaller broken mineral patches and leaves soil/moss between them. All 58,101 sampled terrain heights remained bit-identical; the existing baked mask storage and fetch costs are unchanged. This does not add more landscape geometry or replace the broad ridge shapes.

The live knight has a retargeted breathing idle from the owner's existing motion library. The original purchased GLB remains unchanged. The new derivative retains its exact original mesh, skin, images and three source clips and appends an eight-second closed out-and-back captured-motion loop. Idle phases differ between actors. Movement now accelerates, brakes and turns smoothly; idle actors no longer trigger a combat clip automatically. Zero-contribution pose inputs and distant visual poses are skipped while logical lives continue. A swept-capsule contact check keeps live actors from approaching through the player and blocks player sweeps through those actors.

The combined build includes the coordinated designer/editor optimization and appearance work. Saved NPC appearance bindings and their profile dependencies are now included in package inventory. Existing shader/material batches support the bounded appearance features; this does not add facial animation, authored hairstyle variants or arbitrary rig retargeting.

Performance profiling caught appearance data and branches in the shared scenery shader. The repair compiles appearance only into separate crowd color/shadow programs; ordinary scenery and live skeletal/rigid programs have no appearance inputs. All main, reflection, shadow and loading paths retain their lighting/material bindings. Neutral appearance data is not streamed, repeated custom uploads are skipped, uniform caches hold all program pairs, and explicit program moves prevent stale-handle cleanup after a post-link rollback.

## Correctness and visual review

- Motion/contact CPU contracts pass, including source-clip equality, full skin bounds, conservative culling, rollback and swept-contact cases. The idle sole minimum is −4.2 mm and the measured full skin radius is 1.971 m.
- The final editor passed 67 workflow and 137 optimization checks, all 25 tool panels, keyboard/palette input and 1600×900 → 1280×720 resize. Its 1,000 steady frames averaged 59.96 FPS, 47.99 FPS 1% low, worst 24.06 ms, with one sub-45 frame and 0.168 ms mean editor CPU. Source/runtime metadata, AI/profile files, world and settings remained unchanged. See the coordinated `optimization-polished-verification.json` report and `polished-native-final` / `polished-input-final` cases.
- Five final native captures passed and were visually reviewed: castle arcade, tavern, jungle understory, pool and camp. The imagery shows modest improvements; repeated buildings, broad slopes and repeated NPC silhouettes remain visible. These captures are untimed visual evidence.
- Final combined third-person live workflow passed: conversation, training finish/restart, physics impulse and restore, animation restore, jungle travel and conversation. No rendering/resource errors, failed or pending streaming loads, physics errors or gameplay tick failures were reported. Source world, settings and saved AI files remained unchanged.
- Native camp motion/contact proof on the earlier combined editor binary `3a41…` captured 360 consecutive display frames and a silent six-second clip. It reported 550 NPC yields and minimum player separation 0.820 m, GL0/ref0 and unchanged player files. The relevant motion/contact source is retained in the final combined build. The failed first recording attempt is preserved separately with its zero-frame result; it is not counted as a passed video.
- Appearance owner reports passing rendering/CPU/memory contracts, 27 inventory tests, 11 packaging tests and native project validation. Evidence is under `docs/npc-memory/2026-10-29/appearance-runtime/`.
- The isolated character variant passed exact neutral RGBA/depth/shadow comparisons, custom→neutral→clear transitions and upload-count tests. Radeon native QA verified ten unique program owners, successful-link rollback, moved-from cleanup, failed link and successful commit with GL0/ref0. See `docs/npc-memory/2026-10-29/character-variant/`.

## Performance acceptance

Measured quality is the saved Custom configuration: 1600×900 output, 1120×630 internal rendering (70%), automatic quality disabled, reflections/contact shadows/volumetric fog/clouds retained. No quality settings were lowered for this pass. Full routes cover 432.633 m in the jungle and 585.070 m in the castle, with 120 warmup frames excluded. No builds, imports, encodes or competing native contexts ran in the accepted measurement windows.

| Route | View | Average FPS | 1% low FPS | Sub-45 frames | Statistical target |
|---|---|---:|---:|---:|---|
| Jungle | Third person | 57.39 | 46.47 | 8 / 7,580 | Pass |
| Castle–tavern | Third person | 59.88 | 49.68 | 5 / 10,380 | Pass |
| Jungle | First person | 57.83 | 46.58 | 13 / 7,580 | Pass |
| Castle–tavern | First person | 59.91 | 49.75 | 4 / 10,380 | Pass |

All four route correctness/quality checks passed, with zero blocked route steps, complete returns and GL0/ref0. Runtime inputs, binary, assets, world/settings and saved AI files remained unchanged in both camera windows. Actual first/third-person camera initialization was confirmed in each log. There were 30 isolated sub-45 frames among 35,920 measured frames (0.084%); the worst was 25.316 ms. No below-budget streak exceeded one frame. Strict zero-miss floor acceptance failed, and the raw gate reports preserve that result. These tests cover the demonstration routes, not every part of the 100 km world.

The matched 1,800-frame GPU diagnostic measured the variant repair at 19.878→17.820 ms total GPU, 10.582→9.209 ms forest and 3.527→2.904 ms reflection. Its 1% low rose 40.90→45.79 FPS and budget misses fell 310→2. This comparison pins `f3ae…` and `9bc3…`; the final build adds the last loading surface binding. Timer-query diagnostics are separate from final frame-time acceptance.

The first `routes-third/` attempt is rejected for final acceptance: another authorized appearance task changed renderer/shader sources and linked the executable during the interval. Its failed freeze, changed-input audit and raw frame data remain intact. It is not used to certify the final binary.

The subsequent frozen `f3ae…` run is valid failed performance evidence: jungle 54.42 average / 41.74 FPS 1% low, 317 sub-45 frames. It motivated the repair and is retained under `routes-third-final/`. Accepted optimized data is under `routes-third-optimized/` and `routes-first-optimized/`; `performance-summary.json` contains the exact statistics.

## Portable review build

Built 1,451 files / 6,596,470,793 bytes in a new directory, then renamed it to `builds/relocated/Valwood Demo Polish 2026-10-29`. All file hashes/modes and manifest closure verified after relocation. Package content ID: `eca0576f674b149910beaad5cf73cf6a8371788b294371b3f11f900b816b7ac5`. The copied executable matches the tested `2e1ada96…` hash. The earlier verified Live World Demo package remains unchanged.

The package includes selected runtime assets, provenance and eligible application libraries; it retains the Linux/OpenGL host requirements described by its README. No personal Downloads archives or private saves are gathered. Both relocated Radeon startup checks passed from `/tmp`: third-person castle and first-person jungle, GL0/ref0, zero failed tiles or pending/failed asset loads. Source and package world/settings/AI files stayed unchanged. See `package-native-final/results.json`.

Run [Launch_Polished_Demo.sh](/home/computer/Desktop/final_engine/Launch_Polished_Demo.sh) for this snapshot. It uses the existing single-instance lock and starts the castle at the tested window size. The portable folder's own `launch.sh` remains usable after relocation.

## Remaining visual limits

This is a bounded demo polish pass, not completion of AAA production. Broad terrain slopes, repeated building/character silhouettes, rigid hands/faces, and some plain ground transitions still need targeted art and animation work. A finite route measurement cannot guarantee an uninterrupted minimum frame rate across every scene, machine or desktop workload. The existing asset library is sufficient for the next focused art pass; another broad environment pack is not required to review this demo.

See [DEMO_GUIDE.md](DEMO_GUIDE.md) for controls and review locations.
