# Small Windows playtest add-on

The add-on is extracted into the already-extracted orientation bundle folder.
It adds `playtest-index.json`, `Install_Playtest_Bundle.py`, separate install
batches and only new SHA-named payload blobs. It never replaces
`bundle-index.json`, the original installer, original blobs or an existing seed.
The new installer requires the exact original index SHA/content ID and creates
a new ordinary-file `Valwood_LABEL_Playtest_Seed` folder.

The builder reads bounded, hash-checked metadata from the previously fully
verified base ZIP. It does not extract, copy or rehash all original asset
payloads. The successful base verification report supplies the archive anchor;
the original metadata is checked fresh. At installation, every selected reused
and new payload is hashed and length-checked, followed by the existing complete
package verifier. Native Windows GPU/input acceptance remains separate.

```sh
python3 new_engine/tools/build_windows_playtest_overlay.py \
  --base Valwood_Windows_Orientation_2026-11-02.zip \
  --base-verification new_engine/docs/editor-readiness/2026-11-02/orientation-bundle-verification.json \
  --patch-map /absolute/playtest-patches.json \
  --output Valwood_Windows_Playtest_2026-11-02.zip

python3 new_engine/tests/windows_playtest_overlay_test.py -v
```

Patch maps use schema `valwood.windows-playtest-patches`, version 1 and a
`projects` list. Each project has `label`, `base_label`, package-relative
`patches` mapping names to source files, and `project_dependencies`. Dependency
keys and values omit the `project/` prefix. Every newly added project file
requires an explicit dependency entry, including an empty list when appropriate.
Existing dependencies are preserved unless explicitly replaced. Unknown
dependency references fail the build. Package manifests, asset database IDs,
versions, file lengths/hashes and dependency closure are regenerated; updated
PE imports are inspected and checked against the final DLL inventory.

The add-on is small to transfer, but an independently installed seed still
needs its full materialized disk size. Install one area at a time when needed.
The orientation archive, extracted orientation data and artist work remain
independent of each new playtest seed.

The versioned installer additionally passed a tiny two-seed installation under
the actual bundled Windows Python 3.12.10 in Wine. Both seeds used independent
ordinary files; all 43 original fixture transport files stayed unchanged.
Existing-output and canonical `..` containment checks rejected unsafe targets.
Evidence is in `windows-overlay-installer/results.json`. This fixture did not
materialize production seeds or launch a game/SDL/GPU process.
