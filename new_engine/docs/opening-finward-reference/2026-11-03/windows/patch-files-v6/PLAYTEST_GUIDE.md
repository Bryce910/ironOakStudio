# Valwood Opening Finward Reference r4 — Windows

Run **Launch_Opening.bat** inside the newly installed seed. Choose **Begin the Journey** to see the entrance film and start the opening story. **Continue Checkpoint** resumes progress saved in this seed.

This candidate uses the purchased Finward Studios **House Props & Furniture Vol. 2 demo scene arrangement**: the supplied building shell, connected furnished rooms, resolved materials, furniture placement and converted source lighting. The opening now takes place within that full layout. The project also includes the private library of 15 furniture assemblies in near and mid GLB versions. Embedded images let the game run without Unity.

## Play the opening

1. Watch the father's arrival, or hold the cinematic skip control.
2. Walk to the small table and read the household bills with **E** or **A/Cross**.
3. Approach the console beneath the television and interact again. Release the interaction button before confirming the television menu.
4. Choose **NEW GAME** with **Enter** or **A/Cross** to begin transport into the Meadows of Valois.
5. Follow the meadow path to the waystone to finish this preview route.

The father is exhausted from work and struggling to keep up with ordinary living costs. The console starts the journey into Valois; the front door is not the transport trigger.

The connected rooms can also be explored before starting the television game. Living room, kitchen, bedroom and bathroom each have a wall light switch. Move close, face the switch plate, and use **E** or **A/Cross** when its prompt appears. The supplied source bake combines daylight and fixture lighting, so indirect illumination can remain when a room circuit is off. Furniture and bathroom fixtures currently provide the setting rather than additional quests.

## Controls

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Move / look | WASD / mouse | Left stick / right stick |
| Sprint | Left Shift | LB/L1 or left-stick click |
| Jump | Space | X/Square |
| Interact | E | A/Cross |
| Confirm television New Game | Enter | A/Cross |
| Step away from television menu | Escape | B/Circle |
| Skip cinematic | Hold Enter or Space | Hold A/Cross |
| Pause | Escape | Start/Options |
| Journal | J or Tab | Y/Triangle |

Release a held skip or interaction button before the next action. Pause includes graphics/display settings, look sensitivity, deadzone, invert Y and reduced motion. **F10** also opens graphics/display settings.

**F5** saves the supported story checkpoint, **F8** restores it, and **R** retries during gameplay. Pause provides the same actions. Checkpoints retain story progress and room-light states and restore authored safe positions. **Begin the Journey** starts the opening afresh.

## Keep each revision separate

Install r4 into a **new folder**. Existing destinations are rejected by the installer. Keep earlier artist/playtest seeds and their saves in place; this seed owns its own save and graphics settings. Start with **Begin the Journey** when reviewing the new arrangement. This add-on does not migrate earlier saves.

Use the runtime, project, scene meshes, shaders and physics delivered together in this seed. The new interior settings and runtime support the supplied scene's expanded room layout and lighting. Opening and transport remain a first-person preview; further character performance, recorded dialogue, equipment progression and combat encounters belong to subsequent gameplay work.

Source and purchase records are in `project/opening/reference.json`, `reference-build.json`, `finward-license.json` and the private furniture library manifest. Earlier art records retained in the project describe their historical passes. Purchased content remains subject to its Unity Asset Store license and is included for the purchaser's private game development and artist handoff.

## Candidate status and receiving-PC rehearsal

**Final native Linux and Windows/Wine functional acceptance for this revision are pending.** The rehearsal checks 40 gameplay conditions. The scene uses the source-light-probe lighting revision. This guide makes no frame-rate or final-quality claim. The completed transport verification will accompany the release; transport integrity alone does not establish rendering, sound or physical-controller behavior on the receiving computer.

On the Windows PC, check graphics and sound, use the physical controller, walk through all four room-switch interactions, complete bills → console → New Game → Valois, then save and restore a checkpoint. OpenGL 3.3 driver support is required. If startup fails or something looks wrong, keep `logs/opening-latest.log` and record the room, action and screenshot.
