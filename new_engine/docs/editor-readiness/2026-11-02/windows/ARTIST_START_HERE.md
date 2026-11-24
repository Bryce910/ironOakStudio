# Valwood designer orientation — 4 November 2026

Your first day is for learning the custom editor and looking at the existing
world. Start with a tour and practice in a separate workspace. This is the
owner's native SDL/OpenGL engine; Unity itself is not needed to run this editor.

## Open a safe working copy

1. Extract the entire new orientation ZIP to a local writable folder.
2. Run `Install_city.bat`, `Install_jungle.bat`, or `Install_grandstone.bat`.
   The installer checks the required space and creates a new package folder.
   Install one area at a time if disk space is limited.
3. In that installed package, run `Create_Artist_Workspace.bat`. Keep the
   installed package as your original seed; do your work in the new work folder.
4. Run `Launch_Editor.bat` from the work folder. Keep the console open while
   the scene loads. The default view matches the selected project.
5. If launch fails, send the owner `logs/editor-latest.log` and your GPU name.

The renderer needs the GPU manufacturer's OpenGL driver. Windows hardware,
mouse input, and performance still need acceptance on your actual computer;
Linux and Wine checks cannot establish that result.

## Learn the controls

| Action | Control |
| --- | --- |
| Editor / world | F11 |
| Fly the editor camera | Hold right mouse + WASD; Q/E down/up |
| Frame selection | F |
| Select / move / yaw / uniform scale | Q / W / E / R outside camera flight |
| Full rotation and separate axis scales | Inspector |
| Copy / paste / duplicate | Ctrl+C / Ctrl+V / Ctrl+D |
| Ground selected assets | End |
| Undo / redo | Ctrl+Z / Ctrl+Y |
| Apply and save scene | Ctrl+Shift+S |
| Save applied scene | Ctrl+S |
| Find a tool | Ctrl+P |
| Graphics settings | F10 |

Shortcuts are guarded while typing. A preview is not a save. Read EDITOR_GUIDE.md
for explicit Apply, whole-project checkpoints, recovery, and other tools.

## Tour all three areas

- **city:** the purchased castle city and BEP countryside, with the newest ground,
  housing, material, and grass fixes. It opens at `city-walk`. Inspect the town
  entrance, street, northern housing, castle backdrop, and meadow.
- **jungle:** the existing visual gameplay world with the expanded jungle,
  wildlife, VFX and presentation route. Use Scenic trails/travel to inspect the
  jungle, castle street and courtyard. Start the jungle view explicitly with
  `Launch_Editor.bat --scenic rainforest-milestone` if it is not selected.
- **grandstone:** the snowy mountain stronghold and publisher castle scene.
  The default is `stronghold-town`; other views include `stronghold-gate`,
  `stronghold-overlook`, and `stronghold-hall`.

These are separate projects. A scenic camera does not create a separate scene
or copy edits between projects. The new city and Grandstone are environment
reviews; the old quest and vendor coordinates are not fully relocated there.

## Opening story reference — v16 arrival redesign, 4 November

The opening starts in the father's home. He reads the household bills, turns on
the console, and confirms **New Game**. The v16 opening then brings him
to a small Meadows of Valois campsite with three travelers, a live cooking fire
and a roasting chicken. The three travelers now use distinct standing character
assets. Rowan is the camp cook, Bram the roadwarden, and Ellis
the traveling herbalist. These are the first people he speaks to; the waystone
objective follows their three introductions.

For the playtest, use E or A/Cross to begin and advance each exchange, releasing
and pressing again for every line. Escape or B/Circle leaves without crediting
an unfinished introduction. Completed introductions persist with checkpoints;
partial conversations reopen from their first line. Movement is available
between conversations. The father remains an ordinary parent trying to get home;
armor, modular equipment and facial/acted dialogue remain separate work.

This campsite is a separate opening project. The active release uses
`new_engine/build/opening-finward-reference/v16/project` and the frozen
`new_engine/build/arrival-redesign/runtime-v7/bin/rpg` on the owner’s Linux PC.
V7 terrain and art-v3 repair the previous hard terrain transition and stray water
band, vary the western ridge, layer the surrounding woodland, blend the worn
clearing and repair tent, bedroll, timber and cooking props. The standing actors
remain existing animated assets with text dialogue. These opening changes do not
automatically replace the separate city editor project.

At activation, the owner’s Linux checkpoint was retained **byte for byte**, including
`camp_spoken 2` (Bram introduced) and `room_lights 15` (all four room circuits on).
The visual update does not reset those introductions. The living-world save and
graphics preferences are retained separately; prior projects remain available.

The final [Linux journey](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/native-v16-final/results.json)
and [Windows/Wine journey](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/wine-v16-final/results.json)
each passed all **50 functional checks**. The [eleven actual environment views](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/views-v6/result.json)
and [environment review](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/ENVIRONMENT_VISUAL_REVIEW.json)
record the bounded visual cleanup; final workflow captures check the three
people and their conversation shots. These checks do not establish a minimum
frame rate or physical Windows/controller acceptance.

Use the **Windows Arrival Redesign r8 update** together with the **complete
original Orientation bundle**, and install it into a **new folder**. Follow the
[r8 opening guide](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/windows/OPENING_GUIDE_r8.md)
and use `Launch_Opening.bat`. The [r8 archive](/home/computer/Desktop/final_engine/Valwood_Windows_Arrival_Redesign_2026-11-04_r8.zip),
[SHA-256 sidecar](/home/computer/Desktop/final_engine/Valwood_Windows_Arrival_Redesign_2026-11-04_r8.zip.sha256)
and [release proof](/home/computer/Desktop/final_engine/new_engine/docs/arrival-redesign/2026-11-04/windows/FINAL_RELEASE.json)
bind the final delivery. The independent Windows seed starts with empty saves
and its own graphics preferences; the owner’s Linux progress is not included.
Historical r7/v15 and r6/v14 updates remain separate references. No Unity
installation is required. A city scenic camera will not activate the home or
opening story flow.

The existing large cliff/cut silhouette, distant atlas terrain and some character
and prop detail remain below an AAA art target. Treat this release as a bounded
cleanup and an editor-learning reference. On the receiving Windows PC, rehearse
graphics, sound, the physical controller, four room switches, the complete
opening route and save/restore before relying on it for a presentation.

During orientation, inspect the landing composition, three readable character
silhouettes, fire/chicken visibility, unobstructed talk approaches, and the path
to the waystone. Treat the landing, camera framing and character contact spots
as linked gameplay data. Suggest changes before moving these anchors or blocking
their routes. The owner tracks status in `OPENING_PROLOGUE.md`.

## Practice without altering the reference layout

In a work folder, add a small crate or rock in an open area, copy/paste it,
move and rotate it, press End, and try Undo/Redo. Add one continuous effect
from VFX Studio, then remove it again. Save, close and reopen to check that
your result persists. Keep a screenshot and notes about anything confusing.

For day one, record architectural changes as suggestions. City source-building
collision stays at its original positions after structural edits and needs
regeneration. Grandstone architecture is baked into spatial chunks; selecting
a pillar may select a whole chunk. Reshaping those modules needs source editing
and recooking. Some foundations and ground also depend on separate authored
height fields. These are real limits, not tasks to solve by repeated snapping.

When ready to send saved practice changes, close the editor and run
`Return_Changes.bat`. Send that new return folder plus screenshots and notes.
Returns are conflict-checked files for owner review, not an automatic scene merge.

The replacement father/knight hero, modular equipment and facial/dialogue work
remain separate content tasks. The proposed beach pack is not in this delivery.
