# Valwood Windows artist edition

This is the owner's custom engine and native Studio editor, built for Windows x64. Start with the castle street and tavern frontage in `FIRST_ASSIGNMENT.md`. `EDITOR_GUIDE.md` covers tools; `SURFACE_GUIDE.md` explains roads and material blending. Read `TESTING_STATUS.md` before production work.

## Open your working copy

1. Extract the **entire ZIP** to a local writable folder, for example `C:\Valwood\Valwood_Windows_Artist`. Do not run inside the ZIP, Program Files, a cloud synchronization folder, or a network share. Keep the folder path reasonably short.
2. Use Windows 10/11 **x64**, with your GPU vendor's current OpenGL driver. The engine needs OpenGL 3.3 core. Python, SDL and application runtime DLLs are included; no Python installation or administrator access is needed. This build is unsigned.
3. Keep at least 30 GB free for the seed, a complete work copy and a review copy, plus space for imports and recordings. Copying and the first launch can take time.
4. Double-click **Create_Artist_Workspace.bat**. It verifies the seed, creates a separate sibling folder with a dated name, and opens that folder. Keep the original seed unchanged. The process reports errors in its console.
5. In the **new working folder**, double-click **Launch_Editor.bat**. It starts the castle arcade in Studio. On failure, send `logs/editor-latest.log`. A seed cannot be launched through this shortcut until a working copy is created.

F11 switches editing/play. Hold RMB + WASD to fly; Q/E changes height while flying. F frames a selected model. Drag Assets onto ground; the actual model should appear and move during manipulation. W/E/R selects move/yaw/scale. Escape cancels; End grounds the object. Use Inspector for precise transforms.

For another view, close the editor and use Command Prompt in the working folder:

```bat
Launch_Editor.bat --scenic castle-west-street
Launch_Editor.bat --scenic rainforest-purchased-understory
```

Presets change the starting view, not the saved scene. Keep one game instance open for performance measurements. The owner's reference GPU is an RX 5300 with 3 GB VRAM. Better artist hardware does not change that delivery budget.

## Save and check your work

Apply previews after terrain/material edits. Ctrl+Shift+S applies and saves the scene and surface definitions. If you use typed NPC, gameplay, physics or other project tools, also use **File → Save project checkpoint**. Regions metadata has a separate save. A preview is not a save. Close and reopen to verify the result.

The seed includes the owner's saved authoring state and runtime assets. Unsaved recovery drafts, personal player saves and purchased source archives are excluded. The editor's recovery system handles interrupted sessions; Windows uses flushed files and write-through replacement, without a certified power-loss guarantee.

Static import accepts bounded embedded GLB assets. Do not expect arbitrary FBX/Blender rigs to become animated actors through static import. Use the existing character pipeline and supported aliases. Imported runtime assets belong in the managed project folders. Original purchased assets retain their licenses and may be shared only as those licenses permit.

## Return saved changes

Save and close the editor. Double-click **Return_Changes.bat** in the working folder. It creates a new sibling return folder and opens it. Send that folder using your agreed delivery method, plus the completed acceptance checklist, change notes, screenshots and benchmark evidence. The tool itself sends nothing.

The return is a file-level delta: changed/new supported authoring files and removals. It excludes unsaved recovery, personal saves, graphics preferences, logs, editor layouts and original source archives. A return with no changes is rejected. Review `changes.json` if a saved file appears absent. Keep your workspace until acceptance.

Do not run Verify_Seed.bat on an edited workspace expecting it to pass. The original manifest identifies the seed revision; edited files intentionally differ. One artist owns the scene at a time. Region locks are local organization tools, not network collaboration locks. Use `VAL_Street01` group prefixes and preserve NPC paths, training, entrances and the moat outside the outer wall.

## Owner review

Use the original **Windows seed of the same revision** as the review target. In Command Prompt from that seed, run:

```bat
bin\python\python.exe -B tools\world_artist_handoff.py review --packet "..\Valwood_Artist_Return_DATE" --workspace . --output "..\Valwood_Street_Review_01"
```

The tool verifies seed/binary identity, packet hashes and original file hashes. Conflicting files require deliberate reconciliation. It stages a new complete copy and never writes into the owner's source project. Without `--output`, review is read-only. Never use an unrelated Linux seed or the source-engine directory as the review target.

Validate the staged project before launching:

```bat
bin\python\python.exe -B bin\tools\build_world_package.py --project project --engine-root bin --validate-only
Launch_Editor.bat
```

On Linux, the same Python review tool can stage returns against the same Windows seed; open the resulting `project` with the native Linux engine only after validation. Keep review separate from the live owner project. After acceptance, build a new verified seed through Production or the packaged builder using a new destination. Complete `ACCEPTANCE_CHECKLIST.csv`, including save/reopen, collision, real model movement, imports and measured 45–60 FPS on the intended Windows PC. Performance has not been certified on that machine.

**Launch_Memory_Editor.bat** runs the bundled local browser memory editor when NPC profile work is assigned. Export browser drafts before importing them into the game. Stop its console with Ctrl+C when finished.
