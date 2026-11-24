# Verification and receiving-PC check

This delivery is a Windows x64 environment editor with a project picker, bundled runtime libraries and bundled Python helpers. It includes separate City, Jungle, Grandstone and Apartment & Valois Arrival projects. The assignment is orientation and environment authoring.

## Build-specific status

**The Windows functional checks passed, including the exact editor's copy/paste, terrain-snap and VFX Save/reopen workflows and all four current environment openings.** The receiving Windows PC still needs the first-session check below.

| Check | Recorded result |
| --- | --- |
| Existing Windows platform test | 27 checks passed under Wine: Unicode/space paths, repeated save/reopen, recovery, project journal rollback, locks, workers and a real owned GLB import |
| Current Windows authoring validator | All four current private projects passed; required typed-component, scene-ID and skeletal capabilities were present |
| Native picker executable self-test | 14 Windows checks passed for argument handling, action arguments and instance identity |
| Final Windows Python dispatcher | 28 checks passed with the final editor/helper bytes: installation, independent work copies, preserved edits, saved returns, conflicts, file locks, process identity and baseline/runtime guards; a separate real CLI check verified Unicode-path status output |
| Exact editor: copy/paste and reopening | 44 scripted workflow checks passed under Wine on the RX 5300; four visible house copies, row spacing, colliders, reveal, undo/redo, Save and a fresh-process reload were checked |
| Exact editor: terrain snap and reopening | 21 scripted workflow checks passed for raised and tilted/nonuniform models, End, undo/redo, Save and fresh-process reload |
| Exact editor: VFX and reopening | 32 scripted workflow checks passed; a fresh process reloaded all eight saved runtime emitters |
| Exact editor: four current areas | City, Jungle, Grandstone and Valois Arrival each opened in Studio with an actual hardware-rendered capture; zero GL errors, leftover texture references, asset-stream failures or critical pending assets |
| Package transfer gate | The package is released only after streamed CRC/SHA, index, manifest, dependency and runtime verification; a SHA-256 sidecar accompanies the ZIP |
| Designer's physical Windows PC | First-session acceptance still required |

The platform-test and validator executables are the existing November 2 tools. An earlier core handoff fixture used gameplay R11 renderer bytes. The final dispatcher fixture pins the editor-only runtime, which repairs a scenery transaction size limit exposed by the expanded jungle. CPU fixtures copy and verify renderer bytes without launching them. The executable self-test does not certify physical clicking or display layout; process/UI boundaries in the dispatcher fixture were isolated.

The graphics workflows ran the final Windows editor under Wine on the RX 5300, using disposable authoring files. The four current projects were materialized as ordinary independent files for the opening checks. Source files and saved evidence were preserved. These are functional checks, not sustained performance measurements or full walking-route approval. Terrain painting, lighting and other tools retain their documented existing capabilities; this pass does not certify every possible edit in every area.

## Check on the designer's Windows PC

Use [ACCEPTANCE_CHECKLIST.csv](ACCEPTANCE_CHECKLIST.csv) and retain notes/screenshots.

1. Extract the complete package locally. Open World Designer, choose the city and create the first workspace. Confirm there is enough storage for the bundle, seed and working copy.
2. Confirm the intended scene renders with textures on the hardware GPU, with responsive mouse/keyboard controls and readable UI at the chosen display scale.
3. Complete the small-prop placement, three-copy row, End snap, cancel and undo/redo exercise. Confirm the actual meshes move and remain visible.
4. Apply and Save, close, reopen the same workspace and confirm persistence.
5. Export **Return Saved Changes** after closing. Send the return with a screenshot and notes. The owner checks it against the exact matching seed in a separate review copy.
6. Tour the other areas one at a time. Record missing textures, blocked passages, confusing selection chunks or performance problems.

A Linux or Wine test is evidence for that tested environment. Physical Windows GPU drivers, display/DPI, mouse input and sustained performance need this receiving-PC check. The environment editor uses keyboard and mouse; game controller playtesting is a separate check.

## Known production boundaries

City architectural collision is separate from scenery transforms. Grandstone architecture is combined into source chunks, and full navigation acceptance remains separate. Terrain snap does not flatten plots or rebuild paving. Custom-family capacity is 128; the opening already uses all slots. These boundaries are detailed in [EDITOR_GUIDE.md](EDITOR_GUIDE.md).

For failures, preserve the working copy and record the area, action, screenshot, GPU/driver and logs. Editor: `<workspace>/logs/editor-latest.log`. Frontend: `<bundle folder>_Work/Logs/`. Keep the original seed unchanged while investigating.
