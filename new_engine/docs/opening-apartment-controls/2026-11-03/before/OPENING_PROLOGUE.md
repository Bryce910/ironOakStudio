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

## Apartment visual refresh

The apartment now uses quiet warm painted walls, a neutral ceiling, dry muted oak floors and a tiled kitchen area. The sofa faces the television, the rug has a realistic thickness, and the kitchen has full-size cabinets, a sink, hob and oven. The window has one frame with pleated curtains. Framed prints, books, an entry bench and properly placed lights replace the sparse layout. The bedroom retains the bed asset's own bedside furniture and lamps.

The television now displays an original colorful platform-adventure picture. The existing TV cabinet also holds a console and a second controller, with another controller, a remote and game cases on the coffee table. The TV picture is decorative. The notebook and walking route remain accessible.

There is also an enclosed bathroom beside the bedroom, with a toilet, sink vanity, mirror, shower tray and fittings, towel, toilet paper, tiles and practical lights. Its door is parked open. The bathroom fixtures are set dressing. The living-room window and radiator have moved toward the entrance, giving the bathroom a solid exterior wall.

Indoor shadow precision now focuses the existing map on the room, removing the diagonal bands on painted walls. Receiver-plane comparisons apply only inside the isolated home; exterior shadow coverage remains 160 metres.

The current project is [new_engine/build/opening-apartment-refresh/v6/project](/home/computer/Desktop/final_engine/new_engine/build/opening-apartment-refresh/v6/project). The dedicated launcher uses the apartment runtime and this project. The prior opening projects and their saves remain preserved. Graphics preferences and private progress are copied into the refreshed version.

The earlier apartment revision's complete native and Windows/Wine journeys each passed all 25 keyboard/controller, interaction, cinematic lock/skip, jump, checkpoint and retry checks at full 1600×900 render resolution. The current revision retains those runtimes, story configuration and shaders. Its models pass production decoding. Supplemental production physics checks cover the original opening path, bathroom doorway and aisle, and bedroom access in both directions. Toilet, sink and partition blockers are checked too. Full-resolution native views verify the bathroom, shower and moved window. Authored camera views are separate from gameplay acceptance runs.

The [updated Windows preview](/home/computer/Desktop/final_engine/Valwood_Windows_Opening_Apartment_2026-11-03_r4.zip) requires a fresh extracted copy of the original Windows Orientation bundle. Run `Install_Opening.bat` into a NEW apartment seed folder, then `Launch_Opening.bat`. It materializes about 15.4 GB. Earlier Windows packages remain available. The added gaming props and bathroom were visually checked on Linux; their Windows/Wine rendering, native Windows hardware and physical controller button feel still need rehearsal.

This pass reuses existing furniture and adds authored domestic surfaces and geometry. It does not replace the modern father, add acted dialogue, or implement the armor transformation. Existing combat and Grandstone previews remain available separately.

The current bathroom evidence is bound in [FINAL_REVIEW.json](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-bathroom/2026-11-03/FINAL_REVIEW.json). Previous [apartment](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-refresh/2026-11-02/FINAL_REVIEW.json) and [gaming](/home/computer/Desktop/final_engine/new_engine/docs/opening-apartment-gaming/2026-11-02/FINAL_REVIEW.json) evidence remains available. Earlier whole-route averages were 52.0 FPS on Linux and 54.6 FPS under Wine on the RX 5300 at full 1600×900. Those tests include loading, capture and explicit save/load stalls; the latest additions have not had a new sustained-performance run, and a strict 45 FPS floor is not claimed.
