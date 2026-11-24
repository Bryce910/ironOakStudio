# Verification and receiving-PC check

This edition retains the four Landscape environments, Studio version 8 authoring limits, bundled Python helpers and matching authoring validators. The renderer changes terrain/model material response, weather wetness, grass movement and local reflection filtering. Generated vegetation remains visual scenery without generated solid collisions.

The new executable and shader bytes are bound by the release manifest and RENDER_UPGRADE_STATUS.txt. Current-render evidence accepted for this exact revision is:

- The actual RX 5300 shared-material fixture passed 30 full shader compile/link cases and 2,796 actual GLSL direct, wetness and metallic-indirect cases.
- The actual RX 5300 rooted-grass fixture passed 1,800 transformed-instance cases.
- Final exact-runtime scene/render approval remains pending. This preview does not authorize an archive.

The native and Windows authoring validators and saved authoring documents are byte-identical to the accepted Landscape baseline. The existing project authoring-validation reports are retained. Earlier v7 Landscape mouse/save/reopen, clipboard, End-snap, VFX, dispatcher and Unicode checks are prior-edition evidence; they are not relabeled as new rendered tests of this executable.

No physical Windows receiving-PC test or sustained gameplay performance certification is claimed. Complete the receiving-PC checks below with this edition. Keep older archives, seeds and work folders unchanged.

## Check on the designer's Windows PC

Use [ACCEPTANCE_CHECKLIST.csv](ACCEPTANCE_CHECKLIST.csv) and retain notes/screenshots.

1. Extract the complete new package into a new local folder. Open World Designer, choose the city and create its workspace. Keep older deliveries and work folders separate.
2. Confirm textures render on the hardware GPU, with responsive mouse/keyboard controls and readable UI at the chosen display scale.
3. Follow [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md): Landscape → Find open ground, one small shape stroke, a Meadow/Forest patch, cancel, Undo and Redo. Try a short river in a later exercise.
4. Place a small existing prop, make three visible copies and use End to seat a raised copy. Check actual mesh movement and clearance.
5. Save, close and reopen the same workspace. Confirm the landscape, growth, river if added, and props persist.
6. Close and use Return Saved Changes. The owner validates the return against this exact edition's seed in a separate review copy.
7. Tour the other areas one at a time. Record missing textures, blocked passages, confusing architectural chunks or performance problems.

The environment editor uses keyboard and mouse. Game-controller combat and full gameplay rehearsal are separate checks.

## Production boundaries

Sculpting supports 64 local 128m tiles and ±32m total sculpt/path displacement. There are 32 generated-growth areas and 16 road/river paths. These are validation limits, not recommended performance budgets. New growth uses Studio version 8; older editor/return helpers cannot consume it safely.

Generated trees and rocks are visual scenery without generated solid collisions. Terrain and river height collision are separate from tree/rock collision. Use individually placed collision-enabled models or Physics obstacles where a solid obstacle is needed.

City architectural collision is separate from scenery transforms. Grandstone architecture is combined into source chunks. Imported ground, fixed foundations and navigation routes have protections. Custom-family capacity remains 128; the opening already uses all slots. Read [EDITOR_GUIDE.md](EDITOR_GUIDE.md) before structural edits.

For failures, preserve the work copy and include the area, exact action, screenshot, GPU/driver and logs. Editor: `<workspace>/logs/editor-latest.log`. Frontend: `<bundle folder>_Work/Logs/`. Keep the original seed unchanged.
