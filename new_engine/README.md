# Valwood Studio

Current Linux entry point: `../Launch_Valwood.sh demo|editor|arena|joust|world`. The main modes share one executable, material/lighting shaders and character texture bank. Demo, editor and free exploration all use `projects/valwood_current`; save and apply world changes there to use them in the next demo session.

The active generation is selected by [CURRENT_VALWOOD.json](/home/computer/Desktop/final_engine/CURRENT_VALWOOD.json), through [run_current_valwood.py](/home/computer/Desktop/final_engine/new_engine/run_current_valwood.py). See the [runtime guide](/home/computer/Desktop/final_engine/new_engine/build/unified-runtime-2026-11-16/UNIFIED_RUNTIME_GUIDE.md) for saves, graphics preferences, immutable resources and verification. No Windows package was produced in this integration.

## Engine and historical workflow references

Native C/C++ desktop engine with SDL2, OpenGL 3.3 and an ImGui authoring workspace. This is a playable and editable cinematic-fantasy prototype. Valwood will remain the owner’s custom engine; a migration to another general-purpose game engine is outside scope. AAA production fidelity remains the target; the current renderer and mixed legacy asset library have not reached it.

## Launch

**Streamed-world Studio:** run `../Launch_Studio.sh` to edit the castle in the 100.8 km Valwood world. Terrain sculpting, independent foliage masks, cooked-model placement, point/spot lights, material overrides, splines, procedural projection boxes and atmosphere controls now apply to the live streamed world. **F11** toggles Studio; **F10** opens graphics settings. See [the Studio guide](docs/studio/STREAMED_WORLD_STUDIO.md) for Apply, Save, Undo and current limits, and [the native performance report](docs/studio/streamed-world-2026-10-28/RESULTS.md) for measured routes. `../Launch_Legacy_Studio.sh` preserves the earlier Seldonne editor.

### Designer workspace

For the world artist, begin with [Artist start here](docs/artist-handoff/2026-10-31/ARTIST_START_HERE.md), the [first castle-street assignment](docs/artist-handoff/2026-10-31/FIRST_ASSIGNMENT.md), and the [current editor guide](docs/editor-completion/2026-10-30/EDITOR_GUIDE.md). Work in a separate package copy; the handoff tools export saved changes and detect file conflicts before staging a review copy. Current native delivery targets Linux x86_64. Artist-machine acceptance is still required.

Studio now surrounds a correctly projected 3D viewport with a resizable scene outliner, inspector and content browser. The editor has a separate fly camera; returning to the game restores the player camera. Hold **RMB + WASD/Q/E** to fly, use the wheel to adjust speed, and press **F** to frame selected objects or a connected hierarchy.

- **Q / W / E / R:** select, move, yaw and uniform scale. The inspector edits XYZ, yaw/pitch/roll and positive per-axis scale; supported connected attachments follow their root. Shear and unsupported typed transforms reject. **End** seats selected models on terrain; **Ctrl+C / Ctrl+V** copies and pastes with optional row spacing.
- **Ctrl+click:** extend selection. **Ctrl+D / Delete:** duplicate or delete. Locked parents and active component references are checked before a command can change the draft.
- Drag hierarchy rows to reparent while keeping world positions. Search the outliner without registering thousands of base-scene objects as authored entities.
- Search registered assets by category and multiple words; save favorites, use recent selections, and switch grid/list views. Drag an asset onto ground to place it, or use **Place**. Cards use type icons; **Inspect** opens actual source/LOD inspection. Animated NPC authoring remains in **Tools → NPC / Navigation**.
- **Ctrl+P:** find commands or any of the 25 authoring tools. Type a name and press Enter.
- **Ctrl+Z / Ctrl+Y:** undo/redo. A numeric or gizmo drag creates one undo entry; cancelling a drag creates none.
- **Apply** previews staged geometry/collision. **Ctrl+S** saves the applied world; **Ctrl+Shift+S** applies and then saves. Closing an unsaved session offers saving, keeping the latest recovery draft, or explicitly quitting without saving.

Panel sizes, asset preferences and the selected tool are stored separately in `world.studio.workspace`. **View** contains the content drawer, ground grid, immersive viewport and workspace reset. Run `make test-world-designer test-world-production-recovery` for CPU/ImGui, selection, transform, persistence and recovery checks. Native workspace captures and correctness reports are under `docs/designer-workspace/2026-10-29/`.

The editor now caches asset metadata, filtered hierarchy rows and unchanged inspector values. Numeric drags reuse their validated baseline, connected base-model groups compose source poses in one batch, and viewport selection uses a conservative spatial index before precise mesh tests. A worker-owned parsed-mesh cache retains at most 16 reports within a 32 MiB accounting budget; active inspector handles can outlive eviction. Import, inspection and registry generations invalidate cached data. `make test-world-editor-optimization` checks the hierarchy and picking contracts. [Final integrated optimization evidence](docs/designer-workspace/2026-10-29/optimization-polished-verification.json) separates CPU benchmarks, functional checks and native editor frame measurements, explicitly reports the remaining transient dip, and retains earlier pinned reports.

Performance evidence identifies the exact tested binary. See [current build validation status](docs/designer-workspace/2026-10-29/optimization-current-build-status.json) when later engine changes supersede that executable; earlier FPS results do not certify a newer build.

For the earlier Seldonne project, from this folder:

```sh
make -j4
./run-valwood.sh --play
./run-valwood.sh --editor
```

From the enclosing `final_engine` folder, run `Play_Seldonne.sh` or `Launch_Legacy_Studio.sh`. They resolve the project location automatically. `run-valwood.sh --view meadow` opens a closer grass inspection position; `--view river` and `--view overlook` open other views. The launchers reuse a running Seldonne window instead of starting another GPU-heavy copy. Use Ctrl+Enter to switch modes. The project saves its authored content in `projects/seldonne`.

Play: WASD moves, Space jumps, right-drag turns the camera, wheel zooms, E interacts. Ctrl+Enter switches between play and authoring. The editor provides Models, Landscape, lights, forest painting and Save World. Import a self-contained GLB through the browser or drag it onto the window. Imported copies live in `imports/models`; the originals are preserved.

## Starting area visual slice

Run `../Launch_Valwood_Starting_Area.sh` for the walkable Silverpine Waystation
in the custom atlas renderer. Its 1,639 scenery records include denser canopy,
connected ruin edges and planted camp clearings. The renderer now adds bounded
volumetric clouds, separate indirect-light contact shading, preserved fine
needle coverage and color-view culling that retains offscreen shadow casters.

See [the layout and editing plan](docs/STARTING_AREA_PLAN.md),
[latest native captures and frame-time results](docs/performance/visual-polish/RESULTS.md),
[sky implementation](docs/VOLUMETRIC_SKY.md) and
[contact-lighting implementation](docs/CONTACT_LIGHTING.md).
This is a visual environment slice; AAA production fidelity and finished
gameplay remain further work.

## Old Tanthoon rainforest

Run `../Launch_Valwood_Rainforest.sh`, or select **Old Tanthoon | Emerald
rainforest** under Scenic trails. **Rainforest heart** opens the deeper view.
The temperate rainforest has a broad regional ecology overlay and a dressed
216 m path with overlapping broadleaf/fir canopy, ferns, mossy rocks and fallen
timber. Local humid lighting and damp forest-floor materials blend into the
surrounding world. The existing starting area stays available.

[Biome, path and authoring details](docs/RAINFOREST.md) ·
[Native captures and frame-time results](docs/performance/rainforest/RESULTS.md)

## Full-map terrain project

The map-scale terrain foundation uses a separate `projects/valwood_world` project. Prepare it with `python3 tools/build_valwood_world.py`, then run `../Launch_Valwood_World.sh`. Add `--landmark callieut` to inspect a named location or `--tour` for landmark travel. The launcher shares the existing game's instance lock, so close Seldonne before opening the world project.

Atlas controls: WASD moves, right-drag looks and Shift sprints. Tab opens the atlas with 15 clickable gold markers; the Landmark travel panel has buttons for all destinations. Keys 1–9 select the first nine. F toggles preview flight, with Q/E moving vertically. Escape closes the atlas and releases the mouse.

The reference extent is 100.8 × 56.709 km. At standard walking speed of 3.5 m/s, the straight west/east width represents eight hours. An actual connected overland route has not been timed or authored; slopes and future crossing rules affect journey times. The physical scale is a production choice because the source map has no distance scale. The builder retains the original map and links supporting assets individually. Existing files are preserved on repeat runs, and Seldonne's authored world and saves remain separate.

See [the world terrain implementation plan](docs/WORLD_TERRAIN_PLAN.md) for the coordinate contract and original foundation gates. The runtime now supports third and first person, with fly inspection. Its sea-level surface permits water traversal for inspection; swimming, bridges and ferries remain gameplay work. The new [streamed-world Studio](docs/studio/STREAMED_WORLD_STUDIO.md) edits this atlas project; the older Seldonne toolset remains separate. Base terrain and named geographic anchors do not imply completed settlements or campaign content. The existing `benchmark_valwood.sh` remains a Seldonne benchmark; use `./tools/benchmark_valwood_world.sh --landmark valdoa` for a continuous local check, or add `--tour --frames 5400` for all15 landmarks. See [world terrain results](docs/performance/world-terrain/RESULTS.md).

## Cinematic scenery inspection

The authored-landscape pass adds three local terrain districts, a walkable
2.28 km Valwood trail, 283 editable scenery placements, broadleaf/young-pine
tree families, finer mature crowns and depth-reprojected temporal AA. Materials
and plant placement share the same rock/meadow/moss/trail masks. See the
[authored scenery format](docs/AUTHORED_SCENERY.md),
[pass plan](docs/AUTHORED_LANDSCAPE_PLAN.md) and
[current native results](docs/performance/authored-landscape/RESULTS.md).
The preceding cinematic measurements below remain a historical baseline.

Open `../Launch_Valwood_World.sh --scenic forest` or select **Scenic trails** in the world panel. Other presets are `--scenic lake`, `ridge` and `coast`. **H** hides/shows tools; Tab opens the map. This stays in the native C/C++ engine.

The preceding cinematic pass added source-derived mature pine/fir trees with three model LODs, photographed 3D grass clumps and mossy rocks, coastal scan geometry, shaped alpine ridges, 2m near terrain sampling, slope-aware normal/roughness materials, local and macro terrain shadows, restrained contact occlusion, atmospheric depth and half-resolution HDR planar water reflections. Forest fields use bounded, staged generation and protected nearby roots. Distant cards face the camera and fade by projected size into continuous terrain canopy shading.

[Asset sources and reproduction](docs/CINEMATIC_ASSET_SOURCES.md), [terrain shaping](docs/CINEMATIC_TERRAIN_SHAPING.md), and [native scenery results and captures](docs/performance/cinematic-scenery/RESULTS.md) document the implementation and its limits. Run `python3 tools/benchmark_cinematic_scenery.py` after closing the live engine for the sequential native checks; `--scene forest` selects one run. Existing terrain-foundation profiles remain historical baselines.

## Lighting, shorelines and vegetation continuity

The next visual pass shares sky/ground illumination across terrain, trees and
water, adds terrain horizon visibility and coherent cloud shadows, and corrects
root-based tree LOD coverage. Lake/coast review areas receive finer banks and
locally varied shore edges. A separate opaque HDR/depth snapshot supports
shallow-water transmission, guarded refraction and shoreline contact.

See [the pass plan](docs/LIGHTING_SHORELINE_PLAN.md),
[lighting contract](docs/WORLD_LIGHTING.md), [water implementation](docs/WORLD_WATER.md)
and [native results](docs/performance/lighting-shorelines/RESULTS.md).

## Rendering and responsiveness pass

- Static objects use a shared PBR shader with glTF color factors, normal, roughness, metallic and emissive maps, leaf transmission and weather-consistent fog. Terrain adds matched photographic material channels, slope/height/paint blending, triplanar cliffs and macro variation.
- Near/wide shadow matrices now match their render targets. Alpha-masked foliage casts cutout shadows; nearby small dressing casts contact shadows. Terrain shadow draws use light-frustum culling.
- Distant terrain uses the intended LOD levels. Shared edge samples close LOD seams, front-to-back drawing reduces hidden hillside shading, and rebuilds release vertex/index buffers as well as VAOs.
- Repeated static glTF objects share immutable GPU geometry/textures with reference-counted ownership and warm-cache eviction. Transforms, collision and material overrides remain per instance. The 384 MiB cache target applies to releasable warm assets; active assets can exceed it. Reported static memory estimates exclude characters, terrain and framebuffer allocations.
- Embedded GLB images also share textures across different model files when their complete image content, color space and sampler policy match. Ownership is tracked per material field, including across renderer translation units. Static GPU estimates count shared texture IDs once.
- Grass uses shorter curved leaves, fan-shaped clumps, folded/bent normals, thin-leaf transmission, coherent wind and player/trail bending. Stable world seeds, graded distance density and an explicit whole-field budget replace the previous crop-like field. Terrain cover shares its ecology/color pattern and authoring masks.
- Grass builds retain the visible field and process twelve grid rows per frame during movement, then commit through orphaned streaming buffers. Empty distant cells skip terrain/water/prop queries. The initial field is populated at startup. Authoring invalidation restarts staged work; painted terrain coverage updates reuse texture storage.
- Grass rejects covered roots before computing individual blade curves. Terrain reuses material blend calculations and skips fully faded normal samples; opaque back-facing surfaces skip unused shadow visibility taps. The SSAO blur uses nine bilinear taps in place of twenty-five point taps. Disabled effects skip their rendering work.
- Editor thumbnail IO, parsing, decoding and reduction run on a bounded worker queue. The main thread uploads one small completed thumbnail per frame. Generation IDs cancel stale results after rescans, and shutdown joins the worker before context teardown.
- GPU profiling uses timestamp pairs; nested elapsed-time queries previously made the stage measurements invalid. JSON/CSV profiles retain frame intervals and percentile statistics.
- Normal play skips detailed GPU queries unless the debugger or profiler is active. The final framebuffer no longer requests redundant MSAA; the world retains its HDR target and FXAA pass. Window titles update only when changed.

New CC0 firs, three fern variants and three mossy boulders accompany denser meadow cover. Material/source records are in [the asset license record](docs/licenses/POLY_HAVEN.md). `tools/build_forest_assets.py` cooks the retained sources with Blender and bakes supported shared UV texture transforms into runtime coordinates; `tools/dress_cinematic_start.py` applies the additional dressing idempotently. Original project files are backed up under `projects/seldonne/backups/cinematic-pass`.

## Validation and performance

```sh
make test
./tools/benchmark_valwood.sh play
./tools/benchmark_valwood.sh editor --frames 1800
./tools/benchmark_valwood.sh play --view sweep
```

Use the native desktop GPU for performance checks. `--headless` may select software llvmpipe and is suitable for correctness checks, not native GPU comparisons. Benchmarks now open a visible window at 1600×900, exclude 120 warm-up frames and run for 3,600 frames by default. The editor command above uses 1,800 frames. `--view sweep` adds a full camera orbit to the travel route. The script suppresses the gameplay HUD; omit `--no-hud` when invoking `rpg` directly to measure it. Shader and filesystem caches influence startup. Earlier reference runs used shorter durations, identified in their results.

See [the measured results](docs/performance/RESULTS.md), JSON/CSV files and gameplay captures in `docs/performance`. Profiles include p95/p99, 1% low FPS, worst frames and the fraction within the 45 FPS frame budget. GPU stages are delayed, smoothed estimates; raw CPU sections can include driver waits, and `present_ms` records the preceding swap. These tests do not certify all camera positions, resolutions, combat loads or minimum hardware.

The grass checks cover deterministic roots, retained LOD blades, density masks, negative coordinates, finite inputs and the full field budget. Integration also verifies that grass painting invalidates and updates the shared terrain coverage texture. Launcher checks reject duplicate sessions and benchmarks contending with an open game.

The integration test verifies actual GLB import/placement and Save World/reload, terrain/light/readable persistence, shader linkage, shared GPU-resource lifetimes, repeated NPC model reloads, terrain-buffer cleanup and thumbnail cancellation. Texture tests cover content-hash collisions, color/sampler policy, capacity fallback and cross-translation-unit GL ownership. Image comparisons exercise 72 terrain/static material cases and 168 SSAO blur cases, including tiny and odd-sized targets. Profiler tests verify budget/tail statistics and a 10,000-frame recording. Audio teardown is checked under AddressSanitizer.

## Next production milestone

See [the production roadmap](docs/PRODUCTION_ROADMAP.md). The authored landscape pass now includes depth-reprojected temporal AA, three local terrain districts and an initial scenery authoring format. Priorities include motion-vector history, better vegetation LOD transitions, continued hero scenery, reflection probes/indirect lighting, asynchronous asset cooking and broader editor/streaming stress tests. Living-world micro-model simulation remains a specification requirement; this rendering pass does not implement that scheduler or model integration.
