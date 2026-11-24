# Verification and receiving-PC check

This edition retains the four Landscape environments, Studio version 8 authoring limits, bundled Python helpers and matching authoring validators. The renderer changes terrain/model material response, weather wetness, grass movement and local reflection filtering. Generated vegetation remains visual scenery without generated solid collisions.

The new executable and shader bytes are bound by the release manifest and RENDER_UPGRADE_STATUS.txt. Current-render evidence accepted for this exact revision is:

- Actual RX5300 hardware rendered matching native before/after campsite, purchased town street and authored apartment views, plus the exact updated Windows executable under Wine at1600x900, scale1, automaticqualityoff. Root inspected captures; still-image improvements are subtle, with no AAA quality certification.
- Exact final native and Windows/Wine C21 first journeys passed using ordinary SDL input: apartment cinematic, bills/TV NewGame, all three safe travelers, weapon loan, actual defeat/retry/block/victory, supplies, one-time reward, approximately490m walking and atomic town save/restore. No test pose/story/health writes.
- Actual Windows/Wine selected opening, city, jungle and Grandstone seed loads passed at1600x900, scale1, automaticqualityoff with0GLerrors,0failedassets and0unresolvedtextures. These untimed scene-load checks do not claim fresh editor mouse-workflow coverage; jungle includes presentation menu over rendered scenery.
- Production shader hardware fixtures passed30full compile/link cases,2796GLSL material cases and1800animated rooted-grass TRS cases across all shader variants. Local GGX probes passed HDR/mip/lobe/SH/state-restoration and200steadybind allocation checks after the portable Windows link repair.
- Six separate1800frame native timing runs retained/recomputed all1680post120warmup intervals, with no capture readbacks: campsite39.80to39.61FPS,street38.55to38.46FPS,apartment59.952to59.950FPS. No material mean-frame-rate regression; apartment1percentlow40.22to38.96FPS. Outdoors remain below45FPS at these full1600x900 settings; sustained performance is not certified.
- Paired baseline/candidate motion runs captured32consecutive full production display frames with profiling disabled. Root reviewed first/middle/last frames; motion readbacks make no FPS claim. Earlier rejected profiling/capture harness evidence is preserved.
- Native and Windows authoring validators and saved Studio8 authoring documents remain byte-identical to accepted Landscape baseline. Prior v7 editing/save/reopen/clipboard/snap/VFX/dispatcher evidence remains identified as prior evidence; physical Windows receiving-PC GPU/input acceptance remains pending.

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
