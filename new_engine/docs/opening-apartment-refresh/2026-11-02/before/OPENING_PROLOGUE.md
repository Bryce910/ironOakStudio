# Valwood opening prologue

Open [Launch_Valwood_Opening.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Opening.sh) for the separate opening playthrough: **run-down modern house → transport → Meadows of Valois**. Choose **Begin the Journey** on the title screen, or **Continue Checkpoint** after a checkpoint has been saved.

The opening uses first person. Examine the notebook on the coffee table, approach the front door, then follow the meadow path to the weathered waystone. The route through the existing countryside is about 92 metres. These neutral interactions establish the playable sequence; the cause of the transport and the father's larger story remain open for writing.

## Controls

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Move / look | WASD / mouse | Left stick / right stick |
| Sprint | Left Shift | LB/L1 or left-stick click |
| Jump | Space | X/Square |
| Interact | E | A/Cross |
| Pause | Escape | Start/Options |
| Journal | J or Tab | Y/Triangle |
| Choose menu item | Arrows and Enter, or mouse | D-pad or stick, then A/Cross |
| Skip a cinematic | Hold Enter or Space | Hold A/Cross |
| Graphics and display | F10 or pause menu | Pause menu |

The pause menu also provides look sensitivity, stick deadzone, inverted vertical look and reduced motion. Reduced motion uses stable cinematic compositions. Releasing the sticks after a menu or cinematic lets gameplay controls resume without carrying held movement into the next shot.

## Checkpoints

Progress is saved in this opening's private `saves/opening.progress`. Automatic checkpoints preserve the current completed story step and a supported starting position. **F5** saves that checkpoint, **F8** restores it, and **R** restarts it during gameplay. These commands restore an authored checkpoint rather than an arbitrary position or a partially completed cinematic. Pause also provides Save, Load and Restart Checkpoint.

The opening project has its own graphics settings and progress. The launcher preserves those settings and uses the same single-instance lock as the other Valwood launchers. It clears inherited playtest, showcase and QA flags before starting.

## Scope and verification

This is an opening playthrough built from existing modern furniture and environment assets, with a newly composed small house, cinematic cameras and a countryside objective. The replacement father character, visible modern body, armor transformation, equipment progression and combat are not part of this opening pass. The existing combat and Grandstone playtests remain available through their own launchers.

The house collision route, blocking checks, production CPU model loading and camera handoff continuity have passed. The native playthrough has passed 25 checks, including real walked traversal, keyboard/controller menu confirmation, held-stick handoff, skipping, pause, jump/landing, save/load and retry. The final corrected furniture-material build passed the same complete native run, and the exact Windows runtime passed all 25 checks under Wine. Physical controller hardware and an actual Windows computer still need rehearsal.

The project at [new_engine/build/opening-prologue/project](/home/computer/Desktop/final_engine/new_engine/build/opening-prologue/project) keeps private configuration, AI, shaders and saves, and shares large source asset banks through local links. Original city projects, source furniture and existing playtest saves are preserved. This local copy requires its dedicated runtime and launcher; use the original artist workspace for authoring. The verified [Windows opening add-on](/home/computer/Desktop/final_engine/Valwood_Windows_Opening_2026-11-02.zip) requires a fresh extracted copy of the original Windows Orientation package. Run `Install_Opening.bat`, then `Launch_Opening.bat` inside the new seed; installation needs about 15.4 GB. The Linux project links are not a Windows distribution.

CPU evidence and the generated scene contract are under [the opening review folder](/home/computer/Desktop/final_engine/new_engine/docs/opening-prologue/2026-11-02).

Final acceptance: [FINAL_REVIEW.json](/home/computer/Desktop/final_engine/new_engine/docs/opening-prologue/2026-11-02/FINAL_REVIEW.json). Both final input journeys walked about 102 m and completed all 25 checks. Linux averaged about 59.5 FPS at the fixed 75% test render scale; loading/save/retry/capture stalls remain. This is not a strict 45 FPS floor certification.
