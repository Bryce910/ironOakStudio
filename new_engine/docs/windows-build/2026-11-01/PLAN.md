# Windows editor delivery

User requests: build a native Windows package for the hired world artist; preserve the custom engine. User also asks OpenGL vs DirectX. Working decision: retain existing OpenGL rendering for this delivery; consider a separate Direct3D 12 backend behind a render interface later. A graphics API change alone does not establish AAA fidelity or improve measured FPS automatically.

## Build and verification

1. Cross-compile x86_64 Windows executable and authoring validator with project-local MinGW, SDL2/SDL_mixer/SDL_ttf, Jolt and Win32 compatibility functions. Keep Linux objects and the owner's world files separate.
2. Preserve atomic file replacement, file-level locking, private temporary directories, immutable imports, bounded worker capture and OpenGL function loading. Use Windows UTF-8/long-path application manifests. Windows uses flushed files and MoveFileEx WRITE_THROUGH; POSIX directory-fsync/power-loss semantics are not certified.
3. Package all runtime project assets, Windows DLLs, embedded Python, launch/import/validation/artist-return tools, license notices and Windows-specific guide. No original source archives or personal saves.
4. Verify PE architecture and DLL closure, run Windows validator/functional tests through an isolated Wine prefix, exercise editing/save/reopen/import where supported, and verify the final archive and owner-state preservation. Wine is a compatibility test, not a real Windows GPU/performance certification.
5. Deliver a ZIP and an explicit Windows machine acceptance checklist. Report actual limits and every unresolved test honestly.

## Working paths

- SDK/toolchain downloads: `build/windows/deps`, extracted MinGW: `build/windows/toolchain`, SDL SDKs: `build/windows/sdl`.
- CMake entry point: `platform/windows/CMakeLists.txt`; builder: `tools/build_windows_editor.py`.
- Output: `build/windows/editor/bin/rpg.exe`, `valwood_project_validate.exe`; logs in this folder.
- Isolated Wine prefix: `build/windows/wine-prefix` (not the user's default prefix).
- Existing Linux artist kit remains available in `builds/relocated/Valwood Artist Handoff 2026-10-31` and workspace `../Valwood_Artist_Work`.

Dependency downloads are recorded with official source URLs and SHA-256 in `sdl-dependencies.json`; embeddable Python is the official Python 3.12.10 amd64 ZIP. MinGW Debian packages were obtained with apt's package verification and extracted locally, without installing system packages.
