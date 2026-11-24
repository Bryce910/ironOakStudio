# Readiness for 3 November designer orientation

**Ready for learning the editor, exploring the three environments, and small-prop practice in a separate workspace. This is not approval for unrestricted structural art production or a declaration of a finished AAA demo.** The designer's first acceptance task is to open the intended projects on their own Windows computer, practice editing/save/reopen, and return a small reviewable change.

## What was repaired and checked

This pass repaired two actual authoring defects: an asynchronous texture reimport could overwrite a changed Undo target, and moving/hiding an imported source object could discard its existing pitch, roll, or independent axis scales. The fixes validate staged results before committing, preserve the effective transform, and have targeted regressions. Source picking, clipboard separation, transformed terrain snapping, undo/recovery/checkpoints, imports, materials, surface painting, VFX, controllers and graphics preferences also received focused CPU checks. See the [workflow review](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/workflow-review.md) and [material/assets review](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/material-assets-review.md).

Six disposable-project workflows passed on the native RX 5300: placement, visible snapped clipboard copies, End-key terrain snap, VFX controls, surface/material Apply with rollback, and coordinated save/previous/restore. A further workflow accepted actual Graph Compile/Expand clicks and reopened saved actor, water and graph changes. Two earlier automation attempts missed or rejected input during focus/pointer changes; the synchronized final attempt passed without an engine graph defect. The deliberately failed shaders and rejected edits in those logs test rollback. Evidence: [six workflows](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/native-editor/results.json), [graph/reload](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/native-runtime-editors-focus/results.json).

All three intended projects subsequently opened and rendered natively on Linux and as the rebuilt Windows executable under Wine using the AMD GPU. An independent review of 14 world sidecars and their logs found no unexpected model-streaming, material, texture-cache, physics or GL failures. Actual city, jungle and Grandstone images show textured geometry and editor UI. Grandstone finishes its short smoke with one optional HIGH request loading, but zero failed or critically pending assets. These are opening/resource checks, not full-residency, walking or performance certification. See the [independent evidence review](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/final-evidence-review.md).

The first Windows jungle capture exposed incorrectly escaped Windows paths in profiling JSON. The final change fixes JSON string serialization only; it does not alter rendering or editor behavior. All three final Windows sidecars parse. Exact provenance is:

- Six native workflows: Linux `acb2b124151ce3d2be79bae73e60c05728d1f552709f1129475edee1b6638c09`.
- Graph/reload and three Linux opening captures: `c0efcf5618f9f66cd3d62fe68d0296f629594cb3bfc72f9bbae00357110013b7`.
- Installed final Linux binary after export-only fix: `583a7f07a20d7790da9d232eb460c6fd82725c68e2bc8c13fbecb2af265c9a6c`.
- Final Windows binary, all three final opening captures: `264fb1fc9c2593850459136b8c0250b44791a8cfed3f244204310a85a5966717`.

The six workflows were not rerun on the export-only final Linux relink. [Installed runtime record](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/runtime-installed.json) and [Windows results](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows-final-orientation/results.json) preserve this distinction.

## Open the right environments

| Area | Current project / Linux launcher |
|---|---|
| City and countryside | `valwood_city_showcase` / [Launch_Valwood_City_Editor.sh](/home/computer/Desktop/final_engine/Launch_Valwood_City_Editor.sh) |
| Expanded jungle/gameplay world | `valwood_visual` / [Launch_Valwood_Visual_Editor.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Visual_Editor.sh) |
| Current Grandstone | `valwood_showcase` / [Launch_Valwood_Stronghold_Editor.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold_Editor.sh) |

Older `valwood_world`, `valwood_stronghold` and artist seeds are different revisions. In the new Windows delivery choose `Install_city.bat`, `Install_jungle.bat` or `Install_grandstone.bat`, then create an independent artist workspace and launch its editor. Start with [ARTIST_START_HERE](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows/ARTIST_START_HERE.md), [day-one assignment](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows/FIRST_ASSIGNMENT.md), [editor guide](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows/EDITOR_GUIDE.md) and [acceptance checklist](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows/ACCEPTANCE_CHECKLIST.csv).

## Boundaries for production work

City structural collision remains separate from scenery edits: moved/copied buildings require collision regeneration and walking checks. Snap to Terrain seats bounds; it does not flatten plots or rebuild paving. Grandstone architecture is combined into spatial chunks, with 121/128 custom families already used; individual module editing needs source recooking. Its complete triangle navigation/walking rehearsal remains unfinished. The existing quest/vendor journey is not fully relocated into either new environment.

No locked 45 FPS floor is established. Opening captures include loading/Apply pauses; historical benchmarks use other revisions and settings. Windows hardware drivers, DPI/input, physical controllers and sustained performance require acceptance on the designer's PC. Optional legacy audio notices persist; the current audio bank loads, but these captures use a dummy audio driver. The modern father-to-knight hero, compatible modular gear, facial/lip/finger animation, recorded dialogue and finished ocean content remain separate tasks. [PROJECT_STATUS](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/PROJECT_STATUS.md) records their scope.

## Source protection and delivery

Purchased originals were not edited or included as raw archives. QA used disposable projects. The [40-file source-state audit](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/production-state-audit.json) found only the expected city `world.living` shutdown autosave when closing the original preview; scenes, settings and recovery documents had no unexpected changes. Packaging retains dependency-closed active import revisions and excludes only proven-unused nested encoded caches; unknown or explicitly referenced consumers remain. The [handoff review](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/handoff-review.md) records the safe exclusions and Windows CPU/installer checks. Installation creates ordinary independent files, checks available disk space, and rejects existing destinations.

**Transport verified:** [Valwood_Windows_Orientation_2026-11-02.zip](/home/computer/Desktop/final_engine/Valwood_Windows_Orientation_2026-11-02.zip) is **20,998,555,613 bytes (21.00 GB /19.56 GiB)**. All **2,311 ZIP members** passed streaming SHA256, length and CRC checks; index IDs, project manifests, asset dependency closure and Python/DLL/bootstrap paths passed. [Verification result](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/orientation-bundle-verification.json) and [SHA256 sidecar](/home/computer/Desktop/final_engine/Valwood_Windows_Orientation_2026-11-02.zip.sha256) record the exact artifact. ZIP SHA256: `b0c6930073421971f4f8632bf388726a46a8a2afa0feed52558939811c7bb1db`.

| Project | Seed files | Installed seed bytes | Seed plus separate work copy |
|---|---:|---:|---:|
| city | 1,811 | 15,388,754,147 | 30.78 GB |
| jungle | 1,625 | 6,963,894,023 | 13.93 GB |
| grandstone | 3,200 | 24,512,506,163 | 49.03 GB |

Allow another 22.59 GB for the extracted transport and 21.00 GB for the ZIP, plus filesystem margin. Keep a short local writable path and install one area at a time. Approximately 100 GB free is a practical allowance for Grandstone, its work copy and transport.

The complete archive was verified without extracting or materializing the full production bundle on this space-limited host. The installer and ordinary-file publication passed real bundled Windows-Python fixture checks; Windows workspace/return/conflict handling also passed fixture checks. All three real current scenes rendered in the final Windows executable under Wine. Full-size installation and native Windows GPU/input/save/return acceptance remain the designer's first checklist. Keep the seed untouched and return changes from a separate workspace for staged owner review.

Only completed disposable Wine render-test cache and previously verified duplicate generated seed staging were removed to recover disk space. Original asset archives, the prior Windows delivery, actual artist work, binaries and all QA evidence remain. See `wine-cache-cleanup.json` and `generated-seed-cleanup.json`. The host still has little free space; keep the large transfer archive on another drive after delivery rather than deleting original models or artist work.
