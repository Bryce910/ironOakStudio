# Designer orientation and Windows handoff review — 2 November 2026

Tomorrow's brief is to learn the editor and review the available environments on the designer's own Windows computer. The delivery needs three separately selectable, editable projects: current city/countryside (`valwood_city_showcase`), current Grandstone (`valwood_showcase`), and expanded jungle/gameplay (`valwood_visual`). An old tavern assignment or a single legacy-world snapshot does not satisfy that brief.

## Current delivery versus the older seed

The 1 November Windows ZIP is a historical, verifiable seed, not the current environment delivery. Its content ID is `0c7e8880a9f94759bb0d9fa13963239b212b52b6134ad24f8fef0de2b7264d3b`: 1,757 files, 7,845,180,763 runtime bytes. It contains the older `valwood_world` and its older renderer. Its Windows GPU/input/FPS acceptance was explicitly unverified. The separate `Valwood_Artist_Work` also represents an older working copy; retain it as an artist workspace rather than updating it in place.

The reusable delivery workflow is sound: verify a pristine seed, create a separate working copy/baseline, save and close, export a fresh return, and review it against the exact matching baseline revision in a staged copy. Return metadata pins the seed content ID, renderer SHA and baseline file hashes; it does not require the original generated seed's folder name. A verified archived seed can recreate the baseline. The source workspace, another revision's package, and a live gameplay project are not interchangeable review targets. Existing-output and independently changed-file conflicts must remain failures.

## Packaging fixes completed in this pass

- Runtime inventory includes the private city/Grandstone markers, scene/sky/views/fixtures/residents, native height/coverage/roof fields, and their declared panorama dependencies. Native VWHGT1 payload size, dimensions, finite values and coverage range are validated.
- Source projects may use trusted named sibling runtime banks. Their links are resolved through the explicit inventory trust boundary and materialized as ordinary delivered files. Arbitrary sibling source/archive directories remain excluded.
- Native package validation now receives an ordinary disposable validation tree with checked hardlinks or checked cross-device copies. Sanitized metadata is written separately. Source bytes are not chmodded, timestamped or edited, and temporary trees are cleaned after success or failure. The native validator's no-symlink authoring policy remains intact.
- Opt-in active managed-import closure retains every nonmanaged runtime file and every whole revision reached through Studio, prefabs or cross-revision dependencies. After that closure, only verified unused named BC7 caches nested inside managed revisions are excluded. The selector compares complete encoded image length/SHA256 against the original source bytes embedded in known native cache formats; unknown consumers/formats, explicit references and nonmanaged/global caches remain. Full inventory is still the default. A standalone content-addressed Windows bundle is being prepared separately; the new bundle's final manifest and installer results remain the delivery acceptance evidence.
- Windows editor actions select `city-walk` for the city, `stronghold-town` for current Grandstone, and retain an explicit requested scenic override. They no longer force a legacy castle scenic for every delivered project.

Focused Python regressions passed: 36 asset-database, 14 cache-selection, 15 package, 11 handoff, 1 Windows-action and 8 standalone-bundle tests (85 total). Reproduction from `new_engine` uses `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p '<test_filename>'` with `world_asset_database_test.py`, `world_package_cache_selection_test.py`, `world_package_test.py`, `world_artist_handoff_test.py`, `windows_artist_actions_test.py` and `windows_artist_bundle_test.py`. Inventory source hashing reuses a previously verified digest only while inode, size, mtime and ctime all match; a same-size in-place edit regression proves rehashing and rejection by the stale source snapshot.

Before nested-cache filtering, the conservative active-content estimate was 22,513,968,537 unique SHA bytes across the three scenes (15,889,686,193 missing from the older Windows seed), excluding refreshed binaries/helpers and metadata sanitization. Logical extracted sizes were much larger because managed revisions linked the same full texture cache repeatedly: approximately 15.33 GB city, 97.75 GB Grandstone, and 6.90 GB jungle. Archive content deduplication alone does not solve extracted duplicate files. The package plan needs the separately reviewed safe cache filter and a destination free-space preflight; do not promise these preliminary sizes as final bundle sizes.

The final cache-pruned project inventories passed against frozen selector SHA256 `1664e44f5054d21cdc1df2f1d0415c24e5449770dee90efb777e689c59746253`:

| Project | Files | Ordinary seed project bytes | Unique SHA payload bytes |
|---|---:|---:|---:|
| City/countryside | 1,714 | 15,325,101,815 | 12,246,680,827 |
| Grandstone | 3,103 | 24,446,838,605 | 16,601,776,019 |
| Jungle | 1,524 | 6,900,152,455 | 6,804,349,649 |

The three-project union is 22,513,983,810 unique payload bytes before shared Windows runtime/helpers/docs. Grandstone excludes 19,167 proven-unused nested cache files, avoiding 73,299,917,312 bytes of duplicate materialization. These are inventory totals, not a compressed ZIP size. Keeping the extracted bundle, one installed seed, and its independent working copy needs additional disk space. All 312 protected project authoring documents, height fields, shaders and scenery inputs had matching before/after hashes. The exact read-only plan is `/tmp/valwood-handoff-active-cache-plan.json`; no archive or source project was modified by that run.

## Independent editor code review

`engine/world_surface_authoring.cpp:103` stages image imports in a copy, checks the asynchronous target's original name/path/role, preserves the map role on replacement, ensures new names are unique, and validates before assigning either output. Invalid/stale requests leave the current document and selected result intact. In `engine/world_studio_surface_view.inc:23`, the caller captures the original image before the worker starts and checkpoints only after successful staging. Changing image selection while the worker runs cannot silently redirect the result. No new defect was found in these paths.

`engine/world_studio_tools.cpp:39` now copies effective source pitch/roll/independent axis scale with `vw_transform::extra` when creating or updating a source transform override. New yaw, uniform scale and offset remain the requested changes. The source hide path at line65 initializes the same extra transform data; an existing override is only marked hidden. Both operations validate a staged document before assigning it. The added tilted/stretched source regressions cover transform and hide preservation. No further edit was needed.

## Current Windows CPU verification

The rebuilt `build/windows/editor/bin/windows_platform_test.exe` and `rpg.exe` were dated 2 November, 14:17. Tests ran under the existing `build/windows/wine-prefix` with no game/SDL/GPU launch and no use of `render-prefix`:

```text
WINDOWS_PLATFORM_PASS 27 checks: Unicode/space paths, repeated save/reopen/TRS,
recovery, checkpoint/journal rollback, locks, workers and imports
```

The optional actual import used `tools/import_world_asset.py` and the owned `assets/purchased/medieval_village/material-fidelity-before/purchased_village_crate_near.glb`. An initial test invocation mistakenly supplied the engine directory instead of the importer script; the corrected invocation passed. This was a test invocation error, not an importer fix.

Bundled Windows Python also ran `tests/windows_handoff_smoke.py`: workspace creation, one-file return, staged review, unchanged seed, existing-output rejection and conflict rejection all passed. It used a tiny temporary fixture and the current renderer bytes; it did not execute the renderer.

The new content-bundle installer separately passed a real Windows-Python CPU smoke using a 175,375-byte temporary fixture. It materialized 35 manifest files (155,247 bytes) plus the manifest into a fresh folder with spaces. The installed package verified, with all 36 files ordinary and independently owned (`st_nlink==1`, no symlinks). This exercised Windows staging, verification and publication, without executing the fixture EXEs. Fixture evidence is retained at `/tmp/valwood-windows-bundle-review-bgnoxd79`; its installed content ID is `0b8336842ab6339cd078ea809a62aff53836cdf83c05b85b069e27f06163520b`. The actual environment bundle still needs its own final inventory/install acceptance.

Reproduction on this host:

```sh
WINEPREFIX="$PWD/build/windows/wine-prefix" WINEDEBUG=-all wine \
  "$PWD/build/windows/editor/bin/windows_platform_test.exe" \
  'Z:\home\computer\Desktop\final_engine\new_engine\tools\import_world_asset.py' \
  'Z:\home\computer\Desktop\final_engine\new_engine\assets\purchased\medieval_village\material-fidelity-before\purchased_village_crate_near.glb'
WINEPREFIX="$PWD/build/windows/wine-prefix" WINEDEBUG=-all wine \
  "$PWD/build/windows/editor/bin/python/python.exe" -B \
  'Z:\home\computer\Desktop\final_engine\new_engine\tests\windows_handoff_smoke.py'
```

Wine requires host syscalls blocked by the command sandbox, so these CPU checks used approved host execution. Native Windows graphics-driver, input, DPI and performance acceptance on the designer's actual PC remains separate. Wine CPU success does not certify that hardware.

The subsequent Windows jungle render exposed a real sidecar serialization defect: its raw `C:\users\...` authored-source paths made `.world.json` fail at line136 with an invalid `\u` escape, despite a successful render. `engine/world_json_string.h` now produces complete JSON string tokens, and all seven dynamic string fields in the world sidecar use it. Four C++-to-Python parser regressions pass (`python3 -m unittest discover -s tests -p world_json_string_test.py`), covering the exact failed Windows path, quote/injection handling, every JSON control byte including NUL, and UTF-8. Passing all ten original report paths through that production helper makes the entire original report parse with `gl_error==0`; the recorded artifact itself was left unchanged. Acceptance of the rebuilt Windows renderer/report is parent-owned and must be recorded after its native rerun.

## Orientation limits to state clearly

Current city structural UCX collision is separate from Studio scenery transforms. Moving or copying a city building requires a collision rebuild and walking recheck before approving the result for gameplay. Snap to Terrain seats transformed bounds against the height service; it does not rebuild paving or flatten a plot. Grandstone's imported source modules are combined into chunks, so selecting an ornament may select its entire chunk. Grandstone has121/128 custom families; its recorded route is a visual flight, not a complete navigation rehearsal. The existing gameplay/vendor journey remains in the visual/jungle world and has not been relocated throughout the new city.

The orientation guide should explain these boundaries before asking the designer to make structural changes. Preserve the original baseline and return changes as a reviewable delta. The final Windows package should include an owner/source register for the purchased village, NatureManufacture/BEP and character content plus engine/CC0 credits. Current model/source manifests preserve provenance, but city and Grandstone project roots had no `CREDITS*` document at this review; do not treat a copied asset as a new redistribution license or include raw purchased archives.

See `PROJECT_STATUS.md`, `workflow-review.md` and `material-assets-review.md` alongside this report for the current scene inventory, editor bounds, and focused test evidence. A refreshed bundle is ready for delivery only after its actual manifests, ordinary installed project files, launch choices, staged return and Windows renderer acceptance have been checked.
