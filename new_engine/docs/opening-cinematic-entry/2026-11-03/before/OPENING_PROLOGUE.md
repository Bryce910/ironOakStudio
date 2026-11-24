# Valwood opening prologue

Open [Launch_Valwood_Opening.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Opening.sh) for the opening playthrough: **modern apartment → bills → game console → New Game → Meadows of Valois**. Choose **Begin the Journey** to see the revised opening. **Continue Checkpoint** preserves your previous progress, including a checkpoint already in the meadows.

The father comes home exhausted from another long shift, struggling with rent and everyday costs. Read the household bills on the coffee table, then approach the console beside the television. Press E or controller A to turn it on. The TV shows **VALOIS / NEW GAME**. Release the interaction button, then press **Enter or A/Cross** to start a new game; this triggers the transport. Escape or B/Circle steps away from the console menu. The front door remains closed and no longer triggers travel.

The transport pushes the camera toward the television while its loading scene changes, then a continuous pale glow leads into the meadow reveal. Thought subtitles and first-person journal entries establish his exhaustion and surprise. These are written lines, not recorded dialogue. In Valois, follow the existing meadow path about 92 metres to the weathered waystone. The larger family story, first substantial quest and timing of the knight transformation remain unwritten.

## Controls

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Move / look | WASD / mouse | Left stick / right stick |
| Sprint | Left Shift | LB/L1 or left-stick click |
| Jump | Space | X/Square |
| Interact | E | A/Cross |
| Room lights | Look at the room's switch, then E | Look at the switch, then A/Cross |
| Start the game on the TV | Enter, or click New Game | A/Cross after releasing it |
| Step away from the TV menu | Escape | B/Circle |
| Pause | Escape | Start/Options |
| Journal | J or Tab | Y/Triangle |
| Choose menu item | Arrows and Enter, or mouse | D-pad or stick, then A/Cross |
| Skip a cinematic | Hold Enter or Space | Hold A/Cross |
| Graphics and display | F10 or pause menu | Pause menu |

The pause menu also provides look sensitivity, stick deadzone, inverted vertical look and reduced motion. Reduced motion uses stable cinematic compositions. Releasing the sticks after a menu or cinematic lets gameplay controls resume without carrying held movement into the next shot.

## Checkpoints

Progress is saved in this opening's private `saves/opening.progress`. Automatic checkpoints preserve the current completed story step and a supported starting position. **F5** saves that checkpoint, **F8** restores it, and **R** restarts it during gameplay. These commands restore an authored checkpoint rather than an arbitrary position or a partially completed cinematic. Pause also provides Save, Load and Restart Checkpoint.

The opening project has its own graphics settings and progress. The launcher preserves those settings and uses the same single-instance lock as the other Valwood launchers. It clears inherited playtest, showcase and QA flags before starting.

## Apartment visual refresh

The apartment now uses quiet warm painted walls, a neutral ceiling, dry muted oak floors and a tiled kitchen area. The sofa faces the television, the rug has a realistic thickness, and the kitchen has full-size cabinets, a sink, hob and oven. The window has one frame with pleated curtains. Framed prints, books, an entry bench and properly placed lights replace the sparse layout. The bedroom retains the bed asset's own bedside furniture and lamps.

In this story version the television starts in standby, shows an original animated fantasy title screen when the console is switched on, then plays its loading/transport effect after New Game. The console is a story interaction. The older apartment preview retains its looping platform-adventure animation. The existing TV cabinet also holds a second controller, with another controller, a remote and game cases on the coffee table. Rent reminders, a long-shift record, a takeaway meal and a worn work badge establish everyday pressure without changing the walking route.

There is also an enclosed bathroom beside the bedroom, with a toilet, sink vanity, mirror, shower tray and fittings, towel, toilet paper, tiles and practical lights. Its door is parked open. The bathroom fixtures are set dressing. The living-room window and radiator have moved toward the entrance, giving the bathroom a solid exterior wall.

The living room, kitchen, bedroom and bathroom each have a working wall switch. Aim at its plate from nearby and press E or A/Cross. Each switch controls its room's electric lights; window daylight and the TV remain independent. Switch states are saved with the opening checkpoint. Existing checkpoints load with all four circuits on. The cleaned bathroom uses neutral porcelain, a straight vanity cabinet and a continuous cloth towel.

Indoor shadow precision now focuses the existing map on the room, removing the diagonal bands on painted walls. Receiver-plane comparisons apply only inside the isolated home; exterior shadow coverage remains 160 metres.

The current project is [new_engine/build/opening-game-start/v1/project](/home/computer/Desktop/final_engine/new_engine/build/opening-game-start/v1/project), using the dedicated story-opening runtime. The prior opening projects and their saves remain preserved. Graphics preferences and private progress are copied into the story version.

The story playthrough tests bills, the actual console interaction, held-input protection, menu cancellation/reopening and a fresh New Game selection before reaching the meadows. It also walks all four rooms and tests E/controller A, pause safety, room isolation, checkpoint save/load and retry. Production model and capsule checks cover the same 45 collision bodies. Actual native stage captures include the TV menu and a later loading/glow frame. Earlier animation GIFs show the older looping TV preview.

The [Windows story preview](/home/computer/Desktop/final_engine/Valwood_Windows_Opening_Story_2026-11-03_r2.zip) requires a fresh extracted copy of the original Windows Orientation bundle. Run `Install_Opening.bat` into a NEW opening seed folder, then `Launch_Opening.bat`. It materializes about 15.4 GB. Earlier Windows packages remain available. Native Windows hardware and physical controller button feel still need rehearsal.

This pass reuses existing furniture and adds authored domestic surfaces and geometry. It does not replace the modern father, add acted dialogue, or implement the armor transformation. Existing combat and Grandstone previews remain available separately.

Current story evidence is bound in [FINAL_REVIEW.json](/home/computer/Desktop/final_engine/new_engine/docs/opening-game-start/2026-11-03/FINAL_REVIEW.json). Previous [controls and animation](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-controls/2026-11-03/FINAL_REVIEW.json), [bathroom](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-bathroom/2026-11-03/FINAL_REVIEW.json), [apartment](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-refresh/2026-11-02/FINAL_REVIEW.json) and [gaming](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-gaming/2026-11-02/FINAL_REVIEW.json) evidence remains available. Whole-journey timing includes loading and explicit save/load stalls; a strict 45 FPS floor is not claimed.
