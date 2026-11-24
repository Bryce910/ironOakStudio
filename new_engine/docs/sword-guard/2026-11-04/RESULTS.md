# Held sword guards — November 4, 2026

Left/right inputs already selected the correct gameplay direction, but the previous held poses came from high recoil clips. They kept the sword around or behind the head. The earlier V4 captures also occurred before the guard transitions settled, so their input-state assertions missed the visible defect.

The corrected bank holds the two-handed sword in front of the body with distinct left/right side guards, a high horizontal guard, a low guard and a central thrust guard. **YOUR BLOCK** now identifies the player's selected guard; **OPPONENT GUARD** identifies the practice knight's guard. Incoming-attack text retains the player's block label while guarding.

## Accepted evidence

- [Native V7 regression](native-v7/results.json): **43 checks, 126 SDL inputs, 15 captures; passed** on the RX 5300. Includes all five held guards in first and third person, the four directional attacks, a first-person attack, guard priority, release/rearm and pause/resume. Inputs use the normal virtual SDL controller and ordinary gameplay commands; no fight, player-pose or story-state assignments drive the run.
- Every guard capture requires the renderer's actual final slot, matching direction and full guard blend. Captures wait for the settled animation and record actual blade sockets. Root and an independent reviewer inspected the first/third-person left/right images. [First left](native-v7/captures/first-guard-left.png), [first right](native-v7/captures/first-guard-right.png), [third left](native-v7/captures/third-guard-left.png), [third right](native-v7/captures/third-guard-right.png).
- [Motion CPU regression](cpu-motion.log): all five actual rendered guard slots settle within 0.3 seconds at 30/60/144 Hz; existing pose ownership, attack transitions, pause and allocation checks passed. [Evidence-gate tests](cpu-evidence-gate.log): 11 passed, including rejection of selected-left/rendered-high mismatches.
- [Art validation](/home/computer/Desktop/final_engine/new_engine/build/sword-guard/v3/visual_longsword_mocap/HELD_GUARD_REVIEW.json): 30 guard derivatives updated; 186 models preserved byte-for-byte. Neutral endpoints, materials, embedded textures, topology and UVs retained. Wrist targets, two-hand grip and planted feet are within 1 mm. Existing asset banks remain intact.
- [Linux and Windows build review](/home/computer/Desktop/final_engine/new_engine/build/sword-stick/v7/READY.json): both compiled; exact corrected motion objects and socket data retained; Windows DLL closure passed. Windows hardware testing and redistribution packages were not updated.

The native run used an isolated city project. Source settings/saves, the candidate bank, private configuration and executable were unchanged by testing. This verifies directional controls and animation; it is not a new full victory/defeat rehearsal or sustained-performance certification. Physical controller feel needs the player's hands-on review. First-person upper-sleeve framing remains conspicuous during high guard and is separate camera-art polish.

## Activation and retained trials

[Activation manifest](ACTIVATION.json) records the accepted V7 runtime and the new bank. The shared Linux city/jungle/Grandstone launcher uses `build/sword-stick/v7/bin/rpg`; the three local model aliases use `build/sword-guard/v3/visual_longsword_mocap`. The opening launcher, opening pose alias, original source banks, old executables, archives and user saves/settings were preserved. [Previous aliases](before/model-aliases.json) and the prior launcher/guide are retained under `before`.

V5's first-person trial failed because keyboard V handed control to keyboard input while LT stayed held. V6 changed this replay to an actual controller camera click, then correctly failed because the QA filter discarded its translated camera command. V7 allows that tagged command during the bounded sword-stick replay. Those final replay corrections leave normal gameplay and the V5 art fix unchanged. Both failed trials remain available for review.

Native SHA256: `4e959115b9f51174fd4ec234f5ccf3be9a457396518793fa1a63c52996ef2f4a`.

Windows SHA256: `acda1a401a9bf4aa9e4ae6f7ae65630ca822d0277f9eb9ece8f0f43b97f24152`.

## Reproducing the art derivative

[Recipe fingerprints](ART_RECIPE.json) identify the opt-in cooker and bank assembler. From `new_engine`, always choose fresh explicit output folders:

```bash
env OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --disable-autoexec --factory-startup --python-exit-code 1 --python tools/build_combat_motion.py -- --longsword --held-guards-only --output build/sword-guard/repro/raw
python3 build/sword-guard/prepare_guard_bank.py --raw build/sword-guard/repro/raw --base projects/valwood_world/models/visual_longsword_mocap --output build/sword-guard/repro/visual_longsword_mocap --sockets engine/world_combat_weapon_sockets.inc
```

The assembler preserves the accepted texture/topology bank and non-guard poses, then changes only the held/intermediate guard geometry and corresponding socket rows. A newly cooked candidate needs its matching compiled socket data and another rendered review before activation.
