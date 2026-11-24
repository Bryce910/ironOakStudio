# Source control

Repository: [Bryce910/ironOakStudio](https://github.com/Bryce910/ironOakStudio).
The existing `final_engine` folder is the local checkout, on branch `main`.

## Save and upload code changes

From a terminal in this folder:

```sh
git status
git add .
git diff --cached --stat
git commit -m "Describe the change"
git push
```

Review the staged changes before committing. Git records changes only when you
commit; `git push` uploads those commits to GitHub. Do not force-push routine work.
Before working from another computer, use `git pull --ff-only` in its checkout.

```sh
git clone https://github.com/Bryce910/ironOakStudio.git
cd ironOakStudio
```

## Where the current code lives

`CURRENT_VALWOOD.json` identifies the active local runtime. At the initial import,
the latest source is `new_engine/build/battlefield-opening-2026-11-22/source-v1/`.
This complete source snapshot is tracked explicitly, including engine code,
shaders, bundled libraries, tests and generated C/C++ include data. Its build
receipt is tracked under the same generation's `runtime-native-v1/` directory.

The earlier main source tree is also tracked under `new_engine/`, together with
the general tools, tests, platform code, documentation and authored project text.
Links between tracked project text files use relative paths so they remain valid
after cloning the repository elsewhere.
These two trees are different versions: edit the source selected by the runtime
manifest when changing the current game. The dated source tree is part of the
initial import, not a Git commit history reconstructed from old deliveries.

Historical generation launchers and integration tools are retained because the
current launch chain imports earlier launchers. Historical source copies,
compiled executables, object files and native build caches are excluded.

## Local resources and rebuilding

This public repository tracks code and authored text data. Purchased models,
textures, animation captures, audio, asset caches, videos, screenshots, ZIP
deliveries, personal saves and local graphics settings remain on this computer.
The separate `emberdeep-alpha` browser prototype and `Valwood_Artist_Work` delivery
copy are outside this Valwood engine import.

A fresh clone is a source checkout; it does not contain a ready-to-play game.
The current runtime manifest and historical build receipts contain absolute paths
to this development workspace. The launchers and incremental build scripts also
depend on local resources and earlier compiled generations. Restore licensed
assets and adapt those paths/build inputs before using the current runtime on
another computer. Keep purchased source assets in a private backup that follows
their licenses. GitHub is not a backup of the complete local asset library.

The earlier main tree documents its SDL2/OpenGL dependencies and Makefile in
`new_engine/README.md`. That Makefile alone does not reproduce the latest dated
runtime, whose recorded build chain uses the generation-specific tools.

## Moving to a new generation

When `CURRENT_VALWOOD.json` switches to a new source generation, update the active
generation exceptions in `.gitignore` and commit that source tree as well. Check
with `git check-ignore -v path/to/file` if a new file does not appear in status.
Avoid `git add -f` for purchased assets or generated output.
