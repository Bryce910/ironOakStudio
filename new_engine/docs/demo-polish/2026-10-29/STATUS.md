# Demo polish — final review build

Current result: implemented, packaged and native-verified on executable `2e1ada962e698b9df92e87244ce7a662ccb9b4936ced0dafae296f45439311ec`. All four complete first/third-person jungle and castle walks meet the average/1%-low 45 FPS statistical target at unchanged settings. There were 30 isolated sub-45 frames of 35,920; strict floor acceptance remains failed. The final editor, art, interaction and relocated castle/jungle startup checks pass. No AAA completion claim is made.

Portable build: `builds/relocated/Valwood Demo Polish 2026-10-29`, 1,451 files, content ID `eca0576f674b149910beaad5cf73cf6a8371788b294371b3f11f900b816b7ac5`. Local shortcut: `/home/computer/Desktop/final_engine/Launch_Polished_Demo.sh`. See RESULTS.md, DEMO_GUIDE.md and readiness.json for the final evidence and limitations.

## Retained implementation history

The notes below describe earlier stages, failed measurements and their repairs. The current result above and RESULTS.md supersede their pending-work statements.

Original verified package is preserved unchanged. Before captures of castle arcade, tavern, jungle path, pool and camp passed GL0/ref0 with world/settings unchanged.

Implemented and CPU-verified: 146 measured additions appended to the 535 original polish placement rows (byte-identical original rows and source order), including three correctly matched physical trade signs, six trade possessions, seven small foundation plants, 18 triangle-supported tavern table objects, 90 composed forest plants, 18 dry-bank reeds and four canopy trees. Six small sign LOD meshes use resident source images and add no image cache policies. Routes, farms, water and earlier placement sources are preserved. Composition was regenerated and validated against production atlas support/clearance.

Live knight uses a derivative of the existing exact 24-joint / 8,434-vertex GLB with a retargeted owner-supplied breathing idle. Original mesh, skin, material images and Running/Skill_03/Walking clips are unchanged. New idle has an eight-second closed out-and-back captured-motion loop and individually offset phases. Preview was inspected. CPU skin sweep radius1.971m and idle sole minimum-4.2mm; old/new source clips sampled identically. Actual translation now accelerates/brakes and turns smoothly; idle actors no longer automatically fire the combat clip. Distant visual pose sampling is skipped conservatively while logical lives continue. Pure idle/walk/run cases no longer sample zero-contribution source clips.

CPU motion contracts and 26 inventory contracts pass; failed intermediate test logs are retained (the first radius check incorrectly measured mesh-local centimetres before applying the mesh node transform; corrected test uses rig/world metres). The native combined build and visual review remain pending. No FPS claim for this new pass yet.

The coordinated chat owns the concurrent designer/editor UX update and central build/viewer/Studio integration. This chat owns art, motion, its inventory dependency changes, new tests/tools/docs, and will perform after-captures and final timed routes in an explicitly released GPU window. No simultaneous builds/cooks/GPU tests during timed performance measurements.

The camp ridge material mask now leaves soil/moss between smaller stone outcrops. 58,101 CPU samples proved terrain-height values bit-identical; mask storage/fetch/allocation counts unchanged. Signs were matched to actual baker/herbalist/tavern locations; clothier/roper no longer carry tankard signs. Final native art review is pending.


Native after-captures on combined binary492b5beecbf1a7e61680de9c9570d33e6e843cb501719c0ada90aad68305804c passed all five views (GL0/ref0, protected world/settings unchanged). 7,923 authored instances / nine sources; polish source681 rows. Idle rig renders correctly; visual pose builds are2 in jungle and4 in castle, rather than6 everywhere. New signs and plant groupings are visible. These are modest art improvements, not AAA completion.

Art review found a live-NPC/player overlap at the camp first-person camera. A new swept-capsule helper and NPC approach separation were added with CPU contact regression cases. Player movement hooks are being integrated by the shared viewer owner. Final camp recapture/live workflow and walking measurements wait for that combined build plus the concurrent editor CPU optimization pass. Do not claim the original after camp capture is an accepted camera-contact result.

Contact integration is now native-verified on binary `3a41f589725e8e0fb5bd9c38b34e15d8e0f465e9f570bf22e458805f5f6bf003`: 600 camp frames, 550 NPC yields, minimum player separation 0.820 m, GL0/ref0, protected world/settings unchanged. The first video wrapper failed because it supplied `--profile`, which intentionally disables sequence readbacks. Its zero-frame result remains failed in `contact-motion/results.json`; the successful contact assertions do not make that a successful video capture. The tool-only correction removes profiling and uses the capture world report.

The first-person live interaction/rollback workflow passed on binary `ea160ae9fee69eeb265c8b67c6f54bb7906fcf0afc9c2ff0824caf037e94e27a`: conversation, training finish/restart, physics impulse/restore, animation restore and jungle travel. This is correctness evidence, not a frame-rate test.

The private four-palm camp proposal was rejected after in-game review. Production art remains at 146 measured additions; no global foliage density increase or terrain-height change. The camp's broad ridge surfaces and repeated building/character silhouettes are still visible limitations; this pass is not an AAA certification.

The corrected `contact-motion-final` native capture passed: 360 consecutive display frames, silent six-second video, GL0/ref0, 550 yields and minimum separation 0.820 m on the final editor build `3a41…`. Third-person live interaction/rollback also passed on that build.

The first full third-person route attempt is **rejected as final performance evidence**. Its source freeze detected renderer, population, appearance inspector, shaders and executable changes made by the concurrent appearance task, plus compilation during the interval. Raw results remain in `routes-third/`: jungle 57.40 average / 46.53 FPS 1% low, four sub-45 frames; castle 59.44 average / 44.01 FPS 1% low, ten sub-45 frames including a 119.49 ms frame. Both route correctness checks passed, but `freeze-after.json` is false. Exact changed inputs are retained in `changed-inputs.json`. No first-person route or package certification will use this interrupted window as proof.

The coordinated appearance build `f3ae7e18…` passed both stronger v2 editor runs, five art captures and live gameplay correctness with protected AI/profile files and source metadata unchanged. The frozen full third-person route test still **failed performance acceptance**: jungle 54.42 average / 41.74 FPS 1% low, 317 sub-45 frames; castle 59.59 / 48.19, eight sub-45 frames. These valid failed measurements remain in `routes-third-final/`.

A matched 1,800-frame GPU diagnostic measured mean forest 10.582 ms, total rendering 19.878 ms, reflection 3.527 ms, shadow 2.578 ms, terrain 1.482 ms; CPU update/render were 0.994/1.243 ms. Queries are enabled in this diagnostic, so its frame-time results are not final FPS acceptance. Audit identified that appearance exports and hair branches were added to the shared scenery shader even for ordinary foliage. The appearance owner is separating an explicit crowd program at compile time and bypassing neutral appearance uploads, with root ownership approval for central program/render helpers. All QA contexts closed before that repair window; packaging and first-person measurements remain held until the optimized build is frozen.
