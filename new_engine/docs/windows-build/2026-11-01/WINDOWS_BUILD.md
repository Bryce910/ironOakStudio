# Windows x64 build and packaging

The native Windows target lives in `platform/windows/CMakeLists.txt`. It compiles the existing engine, ImGui and CPU Jolt physics. Windows platform services provide UTF-8 file access, atomic replacement, locking, temporary files and Python workers. The renderer loads required post-OpenGL-1.1 entry points through SDL after context creation. It uses the same GLSL and render passes as Linux.

On the Linux development host, with the pinned project-local MinGW and SDL SDKs prepared under `build/windows`, run:

```sh
python3 tools/build_windows_editor.py --jobs 3
python3 tools/build_windows_package.py --engine-root . --project projects/valwood_world --output builds/Valwood_Windows_Artist --mode development
```

The builder reads the Makefile source list, generates the GL loader, configures CMake and builds native AMD64 PE executables. C++/GCC runtimes are statically linked; winpthread and SDL application DLLs are bundled. No global compiler or system-library changes are required. SDK versions: MinGW GCC 13 posix, SDL 2.32.10, SDL_mixer 2.8.1, SDL_ttf 2.24.0, Python 3.12.10 embedded x64. Source URLs and hashes are recorded in the development evidence; licenses accompany the delivery.

On a delivered Windows workspace, Production's packaging worker uses the bundled Python and Windows builder. CLI equivalent:

```bat
bin\python\python.exe -B bin\tools\build_world_package.py --engine-root bin --project project --output "..\Valwood_New_Seed" --mode development
```

Every output must be a new directory. The selected project is validated and runtime assets are materialized from the explicit allowlist; saves, credentials, recovery drafts, source archives and unrelated Downloads are excluded. Windows reserved names and case collisions are rejected. DLL metadata is inspected without executing candidate binaries. The output is relocatable and needs no developer tools.

Keep OpenGL for this delivery to preserve the working renderer and prioritize Windows compatibility. A future DirectX 12 backend should sit behind a rendering abstraction and be measured against the same image-quality and performance targets. DirectX 12 requires explicit resource/queue management; an API rewrite is not an automatic FPS or visual-quality upgrade.

Native Windows hardware acceptance remains necessary before production rollout. Use the saved-work exchange and save/reopen walkthrough, then measure the actual target GPU at matching settings. Do not equate successful cross-compilation or Wine checks with a 45–60 FPS guarantee.
