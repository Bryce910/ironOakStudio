# Streaming Windows artist bundle

Implemented as three new files only:

- `tools/build_windows_artist_bundle.py`
- `tools/install_windows_artist_bundle.py`
- `tests/windows_artist_bundle_test.py`

No existing packaging modules, project data, assets or binaries were changed by
this implementation. No production multi-gigabyte archive was built here.

## Build

From `new_engine`, after freezing the intended runtime, project data and docs:

```sh
python3 -B tools/build_windows_artist_bundle.py \
  --engine-root . \
  --project city=projects/valwood_city_showcase \
  --project jungle=projects/valwood_visual \
  --project grandstone=projects/valwood_showcase \
  --active-imports-only \
  --output /NEW/PATH/Valwood_Windows_Orientation.zip
```

`--runtime` can override the existing Windows builder's runtime choice. Labels
must be unique portable ASCII names. Output must be a new ZIP outside every
source project. The active managed-import filter is opt-in and is delegated
unchanged to the existing validated dependency-closure implementation. Without
the flag, the conservative full inventory is used.

Validation reuses `build_windows_package.validate`, including the native
authoring validator, safe validation view, Windows filenames and PE DLL closure.
The builder reuses existing launch/workspace/return batch generation. It emits
the ordinary Windows package's asset database, provenance, native-dependency
report, build recipe, production wrapper and package manifest per label.

Files stream directly from the validated inputs into ZIP members. There is no
full asset staging copy. Original identity/content is checked while streaming,
including sources whose text is sanitized for delivery. Each SHA256 payload has
one canonical content member shared across projects. Embedded Python also has
normal `bin/python/...` entries so it can bootstrap after ordinary ZIP
extraction; its canonical content entries point to those normal files, avoiding
a second copy in the blob store. Duplicate bootstrap filenames required by
Python can still contain identical bytes. A partial archive is removed on
failure; successful publication is atomic and does not replace an existing ZIP.

## Artist orientation

Extract the entire ZIP. Its `READ_ME_FIRST.txt` lists the exact materialized
byte total for each label and says this is orientation, with one selected
project installed at a time. Double-click `Install_city.bat`,
`Install_jungle.bat` or `Install_grandstone.bat`. Alternatively:

```bat
bin\python\python.exe -B Install_Artist_Bundle.py --list
bin\python\python.exe -B Install_Artist_Bundle.py --project city --output "D:\NEW Valwood City Seed"
```

A project label is required; no default project is silently selected. Default
output for each labeled batch is a new sibling `Valwood_LABEL_Seed` directory.
Existing output is rejected. Installer checks byte totals plus a conservative
4 KiB per-file/directory allocation allowance and 64 MiB margin before copying.
Files are fully independent ordinary files: no symlinks, junction outputs or
hardlinks. Every source stream is hashed, the selected normal package is checked
with existing `build_world_package.verify_package`, and only then is it
published. A failed installation removes its incomplete temporary directory.

Use the resulting seed's `Create_Artist_Workspace.bat`; launch and review in
that working copy, then use its existing return workflow for saved changes.
Allow space for the extracted content bundle, selected seed and working copy.
The currently estimated Grandstone materialization is very large because
immutable cache bytes occur at multiple required project paths; ZIP dedup does
not reduce the size of independent installed copies. No unproven cache rewrite
or mutable-file hardlink optimization was added.

## Verification and limits

`python3 -B tests/windows_artist_bundle_test.py` passes eight tiny-fixture tests.
Fixtures use the real inventory and AMD64 PE import parser with small dummy PE
files; no Windows executable is run. Checks cover cross-project dedup, active
filter wiring, no staging-copy call, relocation with the source tree hidden,
independent installed files, final package verification, source preservation,
existing-output rejection, required labels/size listing, insufficient disk,
corrupt/symlink content, changed-source cleanup, Windows reserved names,
case/prefix collisions and forged-manifest traversal before materialization.

See `windows-bundle-tests.log` and `windows-bundle-source-hashes.json` beside
this report. Native Windows Python/bootstrap execution, complete large archive
creation/extraction, OpenGL/editor interaction and performance remain acceptance
steps controlled by the parent. CPU fixture success does not certify those.
