# Verification and receiving-PC check

This Landscape edition is a Windows x64 environment editor with four project choices, bundled runtime libraries and Python helpers. It adds one Landscape workspace for terrain shaping, generated vegetation and real rivers. The assignment remains environment design and learning the existing world.

## Current Landscape edition

The exact Windows v7 editor was tested through Wine on the RX 5300. These checks cover functional startup and editing; the designer’s physical Windows PC still needs the acceptance below.

| Check | Recorded result |
| --- | --- |
| Terrain and real-river algorithms | 14,086 strict native CPU checks passed, including all five brushes, a 25-tile brush, the 64-tile limit, exact shared edges, protected samples, deterministic hills, atomic failures, real water/channel queries and save/reopen |
| Combined terrain/path displacement | CPU checks reject ±32m component sums that exceed the atlas limit, including between-node overshoot; failed edits preserve the document, report and existing field |
| Existing terrain and surfaces | Terrain, surface-authoring and derived road/river CPU regressions passed; the private terrain capacity fixture follows the new 64-tile limit |
| Ecology, persistence and inventory | Native CPU ecology, legacy-cover, Studio, recovery/session and prefab regressions passed; version-8 inventory tests passed |
| Sloping-river shader override | 85 CPU source/coordinate/behavior checks passed for all four sealed projects; only local water-level depth/refraction handling changed |
| New runtime and matching validators | Editor-only v7 cross-build complete; byte-identical native and Windows validators retain their actual 17 schema checks each, plus 56 independent Unicode I/O checks |
| River placement protection | 38 CPU checks passed for the dry-run guard protecting existing model roots before channel publication |
| Authored model pivots | 37 parser, C API and real-atlas CPU checks passed for preserving explicit model pivots across terrain changes |
| Actual Windows Landscape mouse workflow | All four areas passed 31 rendered mouse/input checks and 3 fresh-process reopen checks each, retaining shaped terrain, generated growth and a real river |
| New editor: current four projects | City, Jungle, Grandstone and Opening each passed the 360-frame baseline and Landscape/reopen runs, with zero GL errors, leaked texture references, failed assets or critical pending assets; shader compilation/linking passed |
| Picker and Windows dispatcher | 37 actual Windows dispatcher checks passed under Wine, including a Unicode workspace, version-8 save with 64 tiles/32 growth areas, private-helper rejection of 33 areas, saved return and independent review through the exact new validator |
| Packaging and runtime rebase | 26 CPU fixture tests passed; explicit runtime-only rebase permits only the editor executable to change and retains all four validated project inventories, shaders and unchanged validators |
| Windows clipboard, terrain snap and VFX | The exact v7 executable passed 44 clipboard, 21 End-snap and 32 VFX checks, including fresh-process persistence checks |
| Package transfer gate | Release requires streamed CRC/SHA, index, manifest, dependency and runtime verification; a SHA-256 sidecar accompanies the ZIP |
| Designer's physical Windows PC | First-session acceptance required |

Graphics checks above used this edition’s exact Windows v7 executable through Wine on the RX 5300. Native-v5 UI tests were earlier development evidence; this delivery does not claim a new native-v7 rendered UI run. The matching native authoring validator is byte-identical to the tested validator used for owner review.

Functional startup, editing and still-image checks do not establish sustained gameplay FPS or complete walking-route approval. Hardware Windows GPU drivers, DPI, input and sustained performance need the receiving-PC check below.

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
