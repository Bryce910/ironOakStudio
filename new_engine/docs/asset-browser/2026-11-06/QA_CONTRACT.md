Asset browser acceptance contract

Runtime integration

Include `engine/world_asset_browser_qa.inc` after `Studio` is defined. Construct
one `AssetBrowserQA browser_qa` inside `world_viewer_run`. After the normal
`studio.panel` and thumbnail `service`, before `editor_ui_render`, call:

```cpp
const auto result = browser_qa.tick(studio, render_cam, frame,
    launch->editor && launch->frames && !launch->benchmark,
    view_ready && !ui_loading && !destination_loading);
if (result == AssetBrowserQA::Result::Passed) running = false;
if (result == AssetBrowserQA::Result::Failed) {
    launch->exit_code = 1;
    running = false;
}
```

A bounded editor run with `VALWOOD_QA_ASSET_BROWSER` must exit unsuccessfully
unless `browser_qa.passed` is true. Call `report()` at finalization to retain a
partial failure receipt. Public fields `phase`, `checks`, `passed`, `failed`,
and `first_gl_error` may also be included in the normal world profile.

The include requires `Studio.designer_asset_ghost_draws`, incremented only
when the actual textured/depth-tested placement draw returns true. It uses
the existing cache `request`, `stats`, `invalidate_all`, and `shutdown` APIs.
No ordinary editor session runs the fixture.

Isolation and execution

`check_browser_runtime.py` is read-only by default. `--prepare` creates a new
project named `qa-project-asset-browser-PLATFORM-REVISION` under the private
asset-browser build folder and a separate evidence directory. The project
contains an explicit `.valwood-asset-browser-qa` marker. Its scene, shaders,
scenery, settings, workspace, and saves are ordinary private copies. Immutable
model/image/audio banks link to their exact existing source directories.

`--run` is reserved for the root agent. It binds the exact frozen executable,
source, READY receipt, and Windows DLL closure through the accepted
`qa_runtime.py` helper; acquires the existing nonblocking GPU lock; and focuses
only the new owned editor window. It never closes another application.

Use the same command twice, first with `--prepare`, then with `--run`:

```text
python3 docs/asset-browser/2026-11-06/check_browser_runtime.py
  --platform native
  --runtime /ABSOLUTE/PRIVATE/RUNTIME
  --plan-sha256 EXACT_FROZEN_PLAN_SHA256
  --source /ABSOLUTE/COMPLETE/CITY_PROJECT
  --revision UNIQUE
  --prepare
```

Repeat with `--platform windows` and the exact Windows runtime/plan. Root must
keep the display awake. Each run executes the workflow and then starts a new
process against the saved private project with
`VALWOOD_QA_ASSET_BROWSER_REOPEN=1`. The helper records sanitized QA flags,
commands, runtime provenance, focus ownership, source control hashes, logs,
actual tile PNGs, raw profiles, and separate workflow/reopen receipts.

Runtime sequence and evidence

1. Show the real browser at 1600×900 with a 300px content area. Request and
   capture the actual GL tiles for pine, purchased house A, fern0, purchased
   barrel, and starting bucket, using the catalog's exact `preview_stem`.
   Require Ready status, nonzero real textures, 192×192 dimensions, rendered
   image detail, and at least five actual model renders.
2. Clear the selected alias after measuring its visible card, then send
   tagged SDL/ImGui motion, button-down, and button-up events to that real
   card. Require selection of the original bucket alias.
3. Exercise production filter fields with actual browser rendering between
   each transition: category plus search, Favorites, and Recent. Require
   known included/excluded aliases and correct most-recent order. The CPU
   catalog suite separately verifies collection/tag matching and natural
   ordering, including 2 before10 and long numeric names.
4. Select an actual resident small prop using registry metadata and verified
   first-LOD bounds from the loaded scene. Prefer a bucket/barrel, then a crate
   or wheel; reject vegetation, invalid bounds, and props larger than6m. The
   chosen `ghost_alias` is logged and recorded. A library thumbnail alone does
   not prove that its alias has an authoritative grounding group in this scene.
   Arm the verified prop at a fresh production picker hit and move the pointer
   into the exposed viewport. Require `designer_asset_ghost_draws` to increase
   while the document count remains unchanged, then cancel. Protected imported
   props may be previewed here; this step never commits them into the document.
5. Arm bucket placement with a stale previous hit. Require no document change.
   Cancel and require no history/document change. Obtain a new actual viewport
   ground ray and commit through the production placement method. Require one
   added placement and one Undo transaction.
6. Wait for real Apply. Undo, wait for the committed removal, Redo, and wait
   for its committed restoration. Save with the production save method and
   require a codec reload to equal the complete live scene.
7. Resize the actual SDL editor to960×640 and the browser to its minimum112px.
   Require a usable scene viewport and preserved asset filter/selection.
8. Hide the browser, cancel placement, and call the normal deferred Refresh.
   After prior ImGui images have rendered, require no retained tile bytes.
   Explicit cache shutdown must clear tile/queue ownership before GL context
   destruction. The normal world profile must report zero GL errors and zero
   remaining texture references.
9. A fresh process must load the exact saved placement count, original alias,
   and pose, plus favorite IDs. Repeat the real GL tile/card/filter/ghost/
   narrow-layout/cleanup checks. The saved scene bytes must remain unchanged.

The fixture has a 12,000-frame overall bound and a2,400-frame per-stage bound.
It writes a structured failure receipt instead of relabeling an incomplete
stage as a pass. Source controls must have identical hashes before and after
both processes. Failed evidence is preserved; use a new revision to retry.

Scope that must remain honest

The card selection uses the actual ImGui button path with tagged replay
events. Filters, favorites/recent setup, Place arming/cancel, fresh-picker
placement, Undo/Redo, and Save use production methods directly. This is not
proof that every physical click, drag, text-entry, dropdown, or shortcut works.
The fresh-process check is distinct from the same-process codec reload.

Root must additionally inspect the captured real model tiles and normal/
narrow browser screenshots; click search, category/collection dropdowns,
Favorite, Recent, Place, Escape, drag-to-ground, and double-click Inspect using
ordinary input. Review the ghost's size, grounding, depth occlusion, and source
material appearance in the actual scene. Hardware Wine validates the exact
Windows PE and DLL set, while the designer's physical Windows computer remains
a separate handoff check.
