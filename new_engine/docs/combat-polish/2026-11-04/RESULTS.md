# Combat feel and contact feedback — November 4, 2026

The active Linux combat arena now has faster sword attacks, readable body versus weapon impacts, and corrected animation hit-stop. [Launch the arena](/home/computer/Desktop/final_engine/Launch_Valwood_Combat_Arena.sh); it starts paused. [Controls](/home/computer/Desktop/final_engine/COMBAT_ARENA.md).

## Behavior

- Cut commitment: 0.83 → **0.55 seconds**, with contact at 0.20 seconds; thrust commitment 0.52 seconds. Stamina remains finite; recovery improves without removing attack costs.
- Faster enemy cadence retains the 0.24-second parry window. Arena challengers approach before winding up, enter at 4.5 m/s and chase at 3 m/s. Player arena movement is 2.8 m/s.
- Actual health damage produces dark red, non-emissive blood, a body-impact sound and a red damage cue. Weapon guards/parries produce iron sparks, a metal sound and a steel cue, without falsely dealing body damage.
- Physical arrow blood uses its resolved collision point. Melee blood is anchored to the defender torso surface. Bow mode no longer samples a hidden sword for trails.
- New contact poses/reactions commit before hit-stop freezes subsequent frames. Enemy walking follows actual root distance. Faint neutral steel trails last 0.10 seconds and omit the grip end, keeping contact visible.

## Acceptance

[Final actual SDL/GPU rehearsal](native-v3/results.json): **24 checks**, 12,850 ordinary SDL inputs, 13 rendered checkpoints, real defeat/retry, a physical bow hit, **eight sequential victories**, 31 contacts and 20 parries. The deliberate blocked swing left enemy health unchanged and produced metal; body hits produced blood and damage. Body, block and parry captures include material, particle, sound and committed-pose evidence. No driver writes to health, poses, AI phases or outcomes were used.

Feedback totals were 46 body bursts/46 body sounds and 25 metal bursts/25 metal sounds. GL errors, streaming failures, texture references and arena-render failures were zero. Source-project, private-input and executable integrity checks passed.

[Dedicated native material GPU check](material-vfx-native.json) passed on the RX 5300: non-emissive red pixels, sparks, complete depth occlusion, warmed shader and caller texture ownership. This preceded the final trail-only alpha/lifetime/trim refinement; the final full-game run above renders that refinement.

[Core CPU evidence](/home/computer/Desktop/final_engine/new_engine/docs/combat-feel/2026-11-04/cpu-final.log): 7,378 feel checks, 92,092 arena checks, 1,132 stick checks and original training combat. Motion tests also cover same-frame contact commit, frozen-pose holds, resumed reactions, distance-driven gait and zero hot allocations. Material tests cover blood versus metal classification, independent simultaneous contacts, exact arrow anchoring, saturation, expiry and faint trail geometry. Sixteen preparation/evidence-gate Python tests passed.

## Build and scope

[Build review](/home/computer/Desktop/final_engine/new_engine/build/combat-polish/runtime-v3/BUILD_REVIEW.json) checks all 35 frozen inputs, actual compiler dependencies, retained objects and Windows DLL closure. It preserves 1,579 protected files, 68 prior binaries and 27 archive statistics. Linux SHA256: `fc52271c28c462e6ce5a718e972db9620fc9f474d76436d906fee32fc159d5f0`. Windows SHA256: `e5ce0ca1a46b0a580c3d19f6153bc075c6e75e2ae25d4f4e9474da164fe26890`.

[Activation](ACTIVATION.json) changes only the local arena launcher to its new private project/runtime. Previous arena and world projects, saves, model sources and archives remain available. Windows compiles successfully, but no updated Windows delivery or Windows hardware acceptance was performed.

The bounded arena run averaged 59.99 FPS, with 56.17 FPS at the 1% low and 1 frame(s) below 45 FPS at 1280 × 720 output / 80% scene scale. This includes loading and captures and does not certify sustained whole-game performance. Automated input verifies functionality; manual controller feel and audible balance remain for the user's playtest. The existing character assets remain in use.
