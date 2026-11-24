# Landscape editor build pipeline

The final compiled candidate is `build/world-designer-2026-11-06/editor-runtime-v5`, built from ordinary independent `landscape-source-v4` files. Source revisions v1–v4, earlier candidate runtimes, their plans and failed test evidence remain separate and preserved. No canonical engine, gameplay executable, accepted R11 object or earlier release archive was overwritten.

## Exact source and build

`build_landscape_runtime_v5.py` froze the complete private source snapshot, all actual R11 compilation dependencies and 1,430 protected input hashes. Root independently reviewed and approved exact `BUILD_PLAN.json` SHA256 `1ee75d5c33a55f763fdb85cc6c68a6632fc4113b91d09f3b2d9862694ac34eea` before compilation. Native and Windows each rebuilt 13 affected engine translation units and the private validator main, retaining accepted unchanged linked objects. Quoted include closure also tracks new headers and `.inc` files; emitted compiler dependencies reject accidental canonical-source includes.

Native editor/validator outputs live in `editor-runtime-v5/native-bin`; Windows runtime files live in `editor-runtime-v5/bin`. The Windows inventory contains exactly 41 ordinary files and 37 PE binaries. The validator uses the same rebuilt Studio/schema objects as the editor, preserves the three existing capabilities and adds `landscape-v1`.

The private Windows validator reads its project argument through UTF-16 `CommandLineToArgvW`, uses UTF-8 paths for engine readers and diagnostics, and links the exact accepted `valwood.rc.res`. That resource supplies the UTF-8 active code page and long-path manifest needed by narrow CRT/fstream loaders. Source and resource fixes are recorded in `VALIDATOR_UNICODE_REVISION.json`, `VALIDATOR_UNICODE_DIFF.patch`, `SOURCE_DIFF.patch` and the reviewed link command.

All output paths are new revisions. Re-running a build over existing objects/proofs is refused; a subsequent source revision requires a separately reviewed new plan and output path. Root owns all graphics execution.

## Completed CPU evidence

`editor-runtime-v5/CPU_ACCEPTANCE.json` SHA256 `8d19b2a82552b719dc91ac403e09c41aaa0e7eb4aef8c076f6905f04c4d4f8a5` binds:

- 17 native and 17 actual Windows schema checks, accepting v8 with 64 terrain tiles and 32 ecology patches and rejecting over-budget or malformed documents.
- 37 actual Windows dispatcher checks, including accented/CJK working folders, the private v8 inventory helper, a saved landscape, return export, independent review and validation of both saved/reviewed copies.
- 56 independent Windows Unicode checks across Living, typed components and nested assets, recursive Unicode filenames and the actual shipped knight GLB, including corruptions that must fail.
- PE closure, the embedded UTF-8/long-path manifest, source-diff preservation and unchanged hashes for all 1,430 protected inputs and all Windows runtime files after CPU execution.

No game or GPU ran in this build lane. Root's `check_landscape_projects_v5.py --exercise-landscape` runs each of the four areas sequentially: one ordinary disposable project, render smoke, real input Landscape workflow and fresh-process reopen. The City temporary path contains accented and CJK characters. `check_landscape_regressions_v5.py` separately drives the existing clipboard, terrain-snap and VFX workflows sequentially. Their `--prepare-only` checks validate bindings without executing a game.

## Complete package

The new package uses a fresh ZIP_STORED filename and keeps the previous 30 GB archive immutable. The accepted archive contains 30,231,606,769 compressed bytes versus 32,324,289,116 uncompressed payload bytes; stored output avoids recompression for roughly 2.09 GB additional storage. Every stream is still rehashed, and the final independent verifier checks every ZIP member, manifest, sidecar, actual PE import table and the frozen editor/helper/shader/guide bindings without extraction.

The package planner must bind final runtime-v5, the private v8 asset helper in every seed, four explicit reviewed shader overrides, final CPU/GPU acceptance and seven frozen receiving-PC guides. Its exact final-plan hash requires a separate root GO before archive publication. An existing designer workcopy is never silently rebaselined to another binary/content revision; the new delivery creates a separate work folder and preserves previous work.
