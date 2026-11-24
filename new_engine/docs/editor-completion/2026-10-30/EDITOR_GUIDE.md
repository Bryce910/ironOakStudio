# Valwood editor

Open `Launch_Valwood_Editor.sh` from the source workspace folder. In an artist delivery, first read **ARTIST_START_HERE.md**, create a separate working copy, and run `./launch.sh --editor --scenic castle-finished-arcade` there. F11 switches between the editor and the running world. This is Valwood's custom SDL/OpenGL engine. Guide updated for the world-artist handoff on 31 October 2026.

## First session and controls

Choose a small assignment area and establish a before screenshot/benchmark. The initial assignment is a 50–100 m castle street and tavern frontage; see **FIRST_ASSIGNMENT.md**. Scenic presets select starting views in the same world, not separate editable scene files. Asset aliases, material overrides and atmosphere may be shared outside your immediate view.

| Action | Control |
| --- | --- |
| Toggle Studio / return to world | F11 |
| Editor camera | Hold RMB + WASD; Q/E down/up; wheel changes speed |
| Frame selection | F |
| Select / move / yaw / uniform scale | Q / W / E / R outside camera flight |
| Full position/rotation/axis scale | Inspector |
| Extend selection | Ctrl+click |
| Copy / paste / duplicate / delete | Ctrl+C / Ctrl+V / Ctrl+D / Delete |
| Ground selected models | End |
| Undo / redo | Ctrl+Z / Ctrl+Y |
| Cancel current transform gesture | Escape |
| Save applied scene / Apply and Save | Ctrl+S / Ctrl+Shift+S |
| Find command or tool | Ctrl+P |
| Graphics settings | F10 |

Shortcuts are guarded while typing in fields. A model added through the Assets panel is scenery; use NPC / Navigation for an animated actor. Close other engine sessions before profiling. Save the scene and related tool documents before closing; reopening is part of every delivery check.

## Build and compose

The outliner, inspector, content browser and viewport share persistent scene identities. Drag supported assets into the viewport; Ctrl-click adds to a selection. Use F to frame a selection. Right mouse plus WASD flies the separate editor camera. Q/W/E/R select, move, yaw and uniformly scale. The viewport tools preserve their existing shortcuts.

Select a house and press **Ctrl+C**, then **Ctrl+V** repeatedly to build a row. Paste snapping is on by default: each copy sits beside the previous one, using the actual transformed mesh bounds, including overhangs, rotation and axis scales. **Edit → Paste settings** or the Inspector's **Copy / paste** section changes the row direction (relative to the copied object's yaw), the gap in metres, or snapping. Each paste previews geometry/collision automatically and is one Undo step. Undo then paste fills the undone slot. The clipboard keeps a snapshot of the copied objects and supported attachments; it lasts for this editor session. Text fields retain ordinary text copy/paste. Copy and Paste buttons are also on the top toolbar. **Reveal pasted copy** is enabled by default and frames the actual model with its neighbor from above; turn it off in Paste settings to keep the camera fixed. The viewport notification distinguishes loading, a rendered copy, and a rejected scene update. Single-model copies select the model directly, with a full bounds outline and normal move handles. Recopying the original or reversing the row direction skips exact occupied model roots. Press F to frame the new selection, and save to keep the scene. Snapping spaces the copied selection; check clearance against other buildings and roads when composing a street.

Select one or more assets and press **End** to **Snap to Terrain**. The command also appears on the top toolbar (**Terrain End**), in the Inspector, in Edit, in the context menu, and in Find tools. It seats the transformed model bounds on the terrain under the footprint, including rotated/scaled assets and floating or buried objects. X/Z, rotation and scale are preserved. Separate selected assets snap independently; models in a selected group move together with supported light/particle attachments. Each snap is one Undo step; an already grounded selection adds no undo entry. Models grouped with NPC, navigation or water attachments must be snapped separately.

The Inspector now exposes position XYZ, yaw Y, pitch X, roll Z, uniform scale and separate Scale X/Y/Z factors. Rotations use degrees in the UI and saved authoring document. The order is yaw × pitch × roll; axis factors multiply uniform scale. Factors must be positive and between 0.1 and 8; mirrored/zero scales are rejected. Normal and tangent transforms match the rendered shape in color, reflection and shadow passes. Bounds picking uses the full inverse transform.

A rotated child under a nonuniformly scaled parent can create shear. Such a hierarchy edit is rejected rather than approximated. Use a uniform parent or unparent the object while preserving its world pose. Typed route, water, particle and live physics/character attachments retain their narrower transform rules; the editor reports the limitation before Apply. Tilted static collision boxes use conservative enclosing boxes, rather than arbitrary triangle collision. Keep architectural collision and passage clearance in mind when tilting large structures.

Models dropped from the content browser now render automatically. Completed Inspector/gizmo object edits, duplication/deletion and Undo/Redo also preview automatically, through the same validated **Apply** transaction. **Edit → Auto-preview object edits** disables this behavior for manual staging. A preview applies the current scene draft, including other pending scene changes; it does not save it. Terrain, materials and asset imports retain their explicit Apply workflow. Save keeps the applied world. Existing static GLB import preserves purchased sources and creates immutable project revisions. FBX conversion, rig retargeting and general character animation authoring remain separate workflows.

During a gizmo drag or transform-field gesture, the actual model updates every frame through its existing instance buffers, including full rotation and axis scaling. Its shadow and reflection draws use that same pose. No scene/terrain rebuild or new undo entry occurs while dragging. Escape restores a gizmo or Inspector gesture. Ordinary level moves/yaw commit in place on release, including static collision, without refreshing the scene. Cached indirect lighting and canopy/roof masks stay in preview: use **Apply** to rebuild them before a final lighting review. Saved/exported worlds rebuild these caches on load. Scale/tilt, structural changes and moves requiring new grounding retain the full validated Apply boundary and may pause on release. Transient drag poses never modify saved files or live collision; the release commits collision once.

For a draft containing overlapping copies from an earlier session, **Edit → Separate overlapping copies** moves exact duplicates to free adjacent positions in one undoable edit. The startup **Recover unsaved scene** action preserves the recovered models and separates exact duplicate roots before previewing them. Both actions keep saving explicit.

## Save related work together

**File → Save project checkpoint**, also under **Production / Prefabs → Whole-project saving**, captures the applied scene and current physics, gameplay and living-world authoring definitions, the loaded component manifest, and NPC mind/appearance assets. Apply the scene draft first. When a component manifest is loaded, its references choose the physics/gameplay/living destinations; independent tool filenames are used otherwise. The operation runs file I/O in a worker while authoring is held. Personal player saves, simulation balances, original model archives and browser-only memory drafts are not included.

`world.vwproject` is a bounded portable checkpoint of authoring text assets; `.previous` retains the prior saved generation. The files continue to exist at their normal project-relative paths so existing tools and the runtime can read them. A write-ahead `.journal` allows the complete set to finish or roll back if the process stops after only some file renames. Files changed externally after staging are rejected instead of overwritten silently. Checkpoints reject traversal, symlinks, duplicates, oversized files and corrupt payloads.

After an interrupted save, open Production and choose **Finish interrupted project save** or **Roll back interrupted project save**. **Restore previous project checkpoint…** restores the previous set after an explicit confirmation. Restore/recovery requires restarting the engine, so every subsystem loads the same generation. Editing and Play are held until that restart. The current generation remains available as the new previous checkpoint after an ordinary restore.

Closing the workspace checks unsaved typed subsystem edits as well as scene edits; the save-and-quit action saves a whole-project checkpoint. Individual tool Undo/Redo histories remain separate. Ctrl+S retains its scene-only meaning; Ctrl+Shift+S applies and saves the scene. The existing debounced scene-draft recovery is still available; whole-project checkpoints are explicit saves, not a claim that every unsaved subsystem gesture is autosaved.

## Terrain, surfaces and lighting

The 25 tools cover Terrain, Foliage, Objects, Lights, Materials, Splines, Decals, Atmosphere, Shaders, Asset inspector, NPC / Navigation, Particles, Water, Material graph, Production / Prefabs, Landscape, Living world, Cinematics, Regions / Layers, Gameplay, Physics, Animation, Surfaces, Wind / Roads and Live components.

Landscape supports bounded height maps, erosion and biome stamps. Surfaces supports imported PBR layers, terrain paint, projection boxes and model blends. Wind / Roads supports local wind volumes, graded roads and river channels/meshes. Material graphs can target supported model, terrain, water and NPC surfaces. These are bounded tools within the current renderer, not arbitrary new render passes. See the surface guide for image packing and sampler limits.

Local practical lights share eight active slots and have no independent local shadow maps. The sun/shadow, environment, cloud/fog and biome atmosphere systems remain available. Water has authored surface shading and channels; no interactive fluid or ripple solver is claimed. Navigation remains local; general crowd avoidance and global route authoring are unfinished.

### Practical landscape workflow

Start with the **Terrain** tool for bounded height changes, and **Landscape** for heightmap import, erosion, constrained biome masks and corridor sculpting. Set a world target, use small changes, Apply, and inspect the walkable result. Foundation, route and water protections can reject edits; do not repeatedly increase brush strength against a protected area. Sculpt geometry and surface paint are separate operations.

Use **Surfaces → Texture library** to import maps, then create PBR layers and bind terrain or a model alias. Albedo is sRGB; normals and ARM are linear, with ARM channels AO/roughness/metallic. Enable DirectX normal inversion only when the source uses that convention. Paint/erase weights at the world target, Apply, and save. Use projection boxes for local dirt/moss/stains and restrained wear. An alias binding changes all instances using that alias. The standalone **SURFACE_GUIDE.md** in the artist package contains the exact surface/road controls, source image filtering and graph restrictions.

For vegetation, use **Foliage** masks and **Objects** scatter for supported aliases. Constrained biome masks can sample height and slope. Work with a stable seed, modest counts and clear path shoulders. Preserve the scene's canopy variety instead of stacking dense instances in the foreground. Check the result from ground level and at distance for repetition, transitions and temporal shimmer. Wind must stay consistent with the environment; exaggerating it can obscure foliage stability problems.

For **Wind / Roads**, begin from existing ground heights, keep reasonable grades, and Apply each short section. River channels and visible meshes are authored features; respect their bank/channel dimensions. Removing a ribbon restores underlying terrain. Keep the moat outside the outer town wall. Roads are not automatically global NPC navigation routes.

### New model preparation

1. Prepare a static mesh in metres with a sensible pivot and baked transforms. Confirm the object is upright and its physical dimensions are plausible. The supported runtime import is a self-contained glTF 2 `.glb` with embedded buffers and images; keep original purchased/Blender/FBX sources in the artist's source workspace.
2. Check base-color, normal, roughness, metallic and AO mapping. Use the correct normal convention. Avoid expecting unsupported external textures, procedural Blender materials, rigs or general animation authoring to transfer through static import. Preview the actual imported result.
3. In **Asset inspector → Project import / reimport**, enter a unique alias, select LOD0 and optional LOD1/LOD2, set **Metres per source unit**, type and collision. Trees require LOD1. The static cooker limits each GLB to 128 MiB and bounded nodes/images; supported models are validated before replacement.
4. Import, inspect the reported dimensions/maps/triangles, then **Apply**. Reimport creates an immutable project revision; it does not modify purchased sources. Save the scene to retain the alias definition. Do not manually replace an existing revision's files.
5. Place an instance, compare its size with a doorway/person, and walk around it. Use **Preserve source collision** on existing buildings where authored openings matter. A whole-model box collider closes visual doorways. New static GLBs do not automatically receive detailed architectural collision.
6. Check all supplied LODs with **Show LOD0/1/2**, then restore **Automatic LOD**. Excessive draw distance and oversized textures consume the owner's limited GPU budget. Record alias, units, LOD and collision choices in the delivery notes. Animated characters use the separate character pipeline.

### Current authoring limits

These are validation limits, not recommended scene budgets. Dense geometry, texture pressure and draw distance can hit performance limits much earlier.

| Area | Current limit / restriction |
| --- | --- |
| Added model placements | 1,024 per scene document; existing base scenery is separate |
| Sculpt tiles | 16 × 128 m tiles, each 65 × 65 samples; not unrestricted editing of the 100 km map |
| Imported/overridden asset definitions | 64 per scene document |
| Scene light records / active practical slots | 24 records / 8 active slots shared with supported effects |
| Authored particle emitters | 16, with fixed shared particle budgets |
| Vegetation masks | 32; constrained biome painting shares this budget |
| Surface images / PBR layers | 16 / 8 |
| Surface paint | 256 stamps across 64 paint tiles; separate from sculpt tiles |
| Projection records | 64; nearest eight drawn within 180 m |
| Local wind / road-river curves | 16 / 16 |
| Road-river curve | 32 points, up to 1,024 m; neighboring points 0.25–256 m apart |

Use **Regions / Layers** to name/organize selected scene entities and set visibility/locks. Apply and Save scene identities before **Save region document**. Save region metadata separately; pushing flags stages changes on the scene draft. These organizational regions are not independently mergeable scene files, network locks, or a completed automatic renderer-residency adapter. Keep one active owner of each authoring file.

## Responsiveness and budgets

Object-only Apply retains terrain height data and its streaming worker. Unchanged shelter masks, PBR surface data and material-graph programs are reused; changing trees, architecture or their bounds invalidates the shelter bake. It no longer regenerates the global 1025 × 577 terrain lookup texture merely because a prop moved. Terrain, water or vegetation-mask changes still use the appropriate worker fence. The Production panel shows the last Apply duration, whether terrain rebuilt, skipped/rebuilt counts, residency and tracked geometry/texture pressure.

Cold asset imports, GPU shader compilation and actual terrain rebuilds can still pause at an explicit boundary. Budget counters are not total measured VRAM. Current defaults track 192 MiB of streamed geometry and 1536 MiB of shared textures; other render targets, fallback assets and desktop apps also use GPU memory. The asset inspector shows real LOD geometry and map diagnostics. Benchmark edited scenes before treating frame rate as established.

## Export and compatibility

For artist collaboration, keep the verified seed unchanged and create a separate work folder with `artist-workspace.sh`. Save and close before `artist-return.sh`. The owner uses `world_artist_handoff.py review` to check file conflicts and optionally stage a new complete review copy. `ARTIST_START_HERE.md` contains the exact commands. The original package manifest is historical after editing; export a new verified package for a new distribution. Scene files are not automatically merged at entity level.

Production offers validation and Linux packaging. Relocated Studio includes its static importer, schema validator, packaging and frame-audit helpers; these commands require Python 3 and the documented host utilities. New exports include `check-system.sh` (Python 3), which checks OS, CPU architecture, required glibc and library availability from the package's ELF dependency inventory. It does not certify a GPU driver, audio service or frame rate. OpenGL 3.3 and a working display session remain required. The package records the highest required glibc version among its executable and bundled application libraries.

Run the relocated package on each intended target machine. Windows and macOS native exports have not been implemented or certified by this pass. A compatibility preflight is not evidence that those platforms work.

## Benchmark before and after dressing

Use F10 to inspect presets/settings. Keep the reference settings the same between the baseline and candidate. For a reproducible castle test in the artist working copy, close the interactive session and run the following from that folder. Use a new report name for every run. This command sets Balanced quality at 1600 × 900, render scale 1.0, with adaptive quality disabled so added detail cannot silently change resolution.

```sh
mkdir -p artist-reports
VALWOOD_CAPTURE_VIEW=third SDL_AUDIODRIVER=dummy ./launch.sh --play \
  --scenic castle-west-street --benchmark --view river \
  --frames 10500 --warmup 120 --width 1600 --height 900 \
  --graphics-preset balanced --auto-quality off --render-scale 1 \
  --profile "$PWD/artist-reports/street-after.json"
python3 tools/check_world_performance.py artist-reports/street-after.json \
  --output artist-reports/street-after-audit.json
```

The complete existing street route is approximately 585 m and can take several minutes. Check the `.world.json` sidecar for `benchmark_route.complete`, zero collision/slope-blocked steps and a near-zero return error. A partial route or a static camera is not a completed walking test. Repeat with `VALWOOD_CAPTURE_VIEW=first` and a new report name. Benchmark files include profile JSON, CSV and `.world.json`; retain all three plus the audit and actual settings.

The audit reports native renderer, average FPS, 1% low, worst frame, every retained frame over 22.222 ms (45 FPS) and rendering errors. By default any retained budget miss fails the strict gate. Do not delete bad intervals or relabel an average as a minimum. Warmup is exactly the declared initial 120 frames. A 45–60 FPS target remains a target until that particular edited route passes on the owner's machine. If the baseline already misses, stop increasing detail and send the before/after profiles for engineering review. A documented render-scale change can be tested as a separate configuration; it is not a matching-quality comparison.

Use Production's Apply duration, residency and texture/geometry diagnostics and Asset inspector's actual LOD/map data to find excessive assets. Reuse models/materials, supply real LODs, limit oversized textures and overlapping practicals, and keep visibility distances appropriate. The owner's 3 GB GPU also serves render targets, characters and other desktop applications. Heavy imports, shader compilation, explicit terrain Apply and Save/restore checks belong in functional verification, not steady walking measurements.

## Troubleshooting and support notes

| Symptom | Check / action |
| --- | --- |
| Drop shows no model | Check placement notification, valid ground, alias support and placement limit. With auto-preview off, Apply. Inspect import maps and source dimensions; frame with F. A character scenery alias is not an animated NPC. |
| Only selection box appears to move | Verify this package's binary revision. Normal move drags should update the mesh immediately. Save a reproduction and report the model alias, operation and whether auto-preview is enabled. Scale/structural release may still pause at Apply. |
| Shadow/roof lighting looks stale after a move | Apply to rebuild cached lighting before reviewing. Drag poses preview directly; indirect/canopy masks can remain cached until Apply. |
| Doorway visible but blocked | Preserve the existing source collision. A newly assigned whole-model box blocks openings. Inspect tilted/scaled collision and walk the threshold again. |
| Terrain edit rejected | Read the message for tile, slope, foundation, water or protected-route limits. Narrow the edit; the previous live scene is retained. |
| Missing work after restart | Confirm which working copy launched, Apply/Save scene, save separate tool documents, and wait for project checkpoint completion. Recovery drafts are not the artist return. Do not overwrite a stale-base warning. |
| Startup offers recovery | Choose recovery only for the intended working copy. Inspect the recovered result, then Apply/Save explicitly. Exact duplicate roots can be separated through Edit; check their placement afterward. |
| Shimmer, flicker or sudden distant changes | Capture the alias/view/settings and a short before/after sample. Compare with wind paused only if that tool supports it. Check LODs, close overlapping geometry and very thin/repeated surfaces; avoid stacking coincident copies. Do not hide the issue by raising global fog. |
| Frame-rate dips | Retain raw profiles, route completeness, camera mode, settings, GPU/driver and texture pressure. Compare one change at a time; use the same reference configuration. |
| Return refuses a conflict | Owner and artist changed the same saved file or used different revisions. Keep both copies. Reconcile in a new review workspace; do not overwrite the owner's scene. |

When reporting a problem, include package content ID (`build-recipe.json` / `package-manifest.json`), binary revision, alias, exact steps, scene preset, screenshot, settings and the relevant log/profile. If working unsaved, preserve the entire working copy before troubleshooting recovery.


## VFX Studio

Click **VFX** in the top menu bar (or Create → VFX Studio). Choose Arcane wisps, Ember fountain, Campfire, Chimney smoke, Steam vent, Low mist, Sunlit dust, or Fireflies.

- **Add in front of camera** creates and frames an effect immediately. **Place on ground…** arms placement; click the landscape to place the effect, or press Escape to cancel.
- Select an effect in the VFX list or click its marker in the viewport. **Frame effect** brings it into view; double-clicking its list entry also frames it. The normal scene Inspector and move gizmo can move/rename it.
- Shape controls adjust spread, height, intensity, tint, glow and size. Motion controls adjust density, lifetime and (for smoke) wind/warmth. Fire has animated flames, embers and a flickering practical light. Glow on other particle types changes their HDR appearance.
- Slider changes preview immediately without a scene rebuild during the drag. A completed gesture uses normal Apply validation and is one undo step. Auto-preview uses the existing editor preference; when disabled, Apply commits the draft. **Ctrl+Z / Ctrl+Y** undo/redo changes.
- **Pause**, **Restart**, **Speed** and the timeline control the particle/flame preview clock while this tool is open. Scrubbing pauses playback. **Solo selected effect** temporarily isolates that emitter. Leaving VFX restores normal simulation; preview time and solo state are not saved.
- **Duplicate** makes an independent effect with a new seed. **New variation** changes the smoke/dust/spark/wisp pattern. **Position → Snap effect to terrain** grounds the birth point without changing its effect height. Existing world emitters can be edited from **Edit existing world effect**.
- **Save** stores effects, color/glow and names in the world; scenes with VFX also work in Play. Existing version 1–6 scenes remain readable; scenes with colored/emissive VFX use version 7. Unchanged legacy scenes retain version 6 canonical bytes so existing recovery drafts still match. The scene supports 16 authored emitters within the renderer's shared fixed budgets.
