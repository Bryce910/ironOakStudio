# Valwood editor

Open `Launch_Valwood_Editor.sh` from the workspace folder. F11 switches between the editor and the running world. This is Valwood's custom SDL/OpenGL engine.

## Build and compose

The outliner, inspector, content browser and viewport share persistent scene identities. Drag supported assets into the viewport; Ctrl-click adds to a selection. Use F to frame a selection. Right mouse plus WASD flies the separate editor camera. Q/W/E/R select, move, yaw and uniformly scale. The viewport tools preserve their existing shortcuts.

The Inspector now exposes position XYZ, yaw Y, pitch X, roll Z, uniform scale and separate Scale X/Y/Z factors. Rotations use degrees in the UI and saved authoring document. The order is yaw × pitch × roll; axis factors multiply uniform scale. Factors must be positive and between 0.1 and 8; mirrored/zero scales are rejected. Normal and tangent transforms match the rendered shape in color, reflection and shadow passes. Bounds picking uses the full inverse transform.

A rotated child under a nonuniformly scaled parent can create shear. Such a hierarchy edit is rejected rather than approximated. Use a uniform parent or unparent the object while preserving its world pose. Typed route, water, particle and live physics/character attachments retain their narrower transform rules; the editor reports the limitation before Apply. Tilted static collision boxes use conservative enclosing boxes, rather than arbitrary triangle collision. Keep architectural collision and passage clearance in mind when tilting large structures.

Scene edits remain drafts until **Apply**. Undo restores a draft; Apply previews it. Existing static GLB import preserves purchased sources and creates immutable project revisions. FBX conversion, rig retargeting and general character animation authoring remain separate workflows.

## Save related work together

**File → Save project checkpoint**, also under **Production / Prefabs → Whole-project saving**, captures the applied scene and current physics, gameplay and living-world authoring definitions, the loaded component manifest, and NPC mind/appearance assets. Apply the scene draft first. When a component manifest is loaded, its references choose the physics/gameplay/living destinations; independent tool filenames are used otherwise. The operation runs file I/O in a worker while authoring is held. Personal player saves, simulation balances, original model archives and browser-only memory drafts are not included.

`world.vwproject` is a bounded portable checkpoint of authoring text assets; `.previous` retains the prior saved generation. The files continue to exist at their normal project-relative paths so existing tools and the runtime can read them. A write-ahead `.journal` allows the complete set to finish or roll back if the process stops after only some file renames. Files changed externally after staging are rejected instead of overwritten silently. Checkpoints reject traversal, symlinks, duplicates, oversized files and corrupt payloads.

After an interrupted save, open Production and choose **Finish interrupted project save** or **Roll back interrupted project save**. **Restore previous project checkpoint…** restores the previous set after an explicit confirmation. Restore/recovery requires restarting the engine, so every subsystem loads the same generation. Editing and Play are held until that restart. The current generation remains available as the new previous checkpoint after an ordinary restore.

Closing the workspace checks unsaved typed subsystem edits as well as scene edits; the save-and-quit action saves a whole-project checkpoint. Individual tool Undo/Redo histories remain separate. Ctrl+S retains its scene-only meaning; Ctrl+Shift+S applies and saves the scene. The existing debounced scene-draft recovery is still available; whole-project checkpoints are explicit saves, not a claim that every unsaved subsystem gesture is autosaved.

## Terrain, surfaces and lighting

The 25 tools cover Terrain, Foliage, Objects, Lights, Materials, Splines, Decals, Atmosphere, Shaders, Asset inspector, NPC / Navigation, Particles, Water, Material graph, Production / Prefabs, Landscape, Living world, Cinematics, Regions / Layers, Gameplay, Physics, Animation, Surfaces, Wind / Roads and Live components.

Landscape supports bounded height maps, erosion and biome stamps. Surfaces supports imported PBR layers, terrain paint, projection boxes and model blends. Wind / Roads supports local wind volumes, graded roads and river channels/meshes. Material graphs can target supported model, terrain, water and NPC surfaces. These are bounded tools within the current renderer, not arbitrary new render passes. See the surface guide for image packing and sampler limits.

Local practical lights share eight active slots and have no independent local shadow maps. The sun/shadow, environment, cloud/fog and biome atmosphere systems remain available. Water has authored surface shading and channels; no interactive fluid or ripple solver is claimed. Navigation remains local; general crowd avoidance and global route authoring are unfinished.

## Responsiveness and budgets

Object-only Apply retains terrain height data and its streaming worker. Unchanged shelter masks, PBR surface data and material-graph programs are reused; changing trees, architecture or their bounds invalidates the shelter bake. It no longer regenerates the global 1025 × 577 terrain lookup texture merely because a prop moved. Terrain, water or vegetation-mask changes still use the appropriate worker fence. The Production panel shows the last Apply duration, whether terrain rebuilt, skipped/rebuilt counts, residency and tracked geometry/texture pressure.

Cold asset imports, GPU shader compilation and actual terrain rebuilds can still pause at an explicit boundary. Budget counters are not total measured VRAM. Current defaults track 192 MiB of streamed geometry and 1536 MiB of shared textures; other render targets, fallback assets and desktop apps also use GPU memory. The asset inspector shows real LOD geometry and map diagnostics. Benchmark edited scenes before treating frame rate as established.

## Export and compatibility

Production offers validation and Linux packaging. Relocated Studio includes its static importer, schema validator, packaging and frame-audit helpers; these commands require Python 3 and the documented host utilities. New exports include `check-system.sh` (Python 3), which checks OS, CPU architecture, required glibc and library availability from the package's ELF dependency inventory. It does not certify a GPU driver, audio service or frame rate. OpenGL 3.3 and a working display session remain required. The package records the highest required glibc version among its executable and bundled application libraries.

Run the relocated package on each intended target machine. Windows and macOS native exports have not been implemented or certified by this pass. A compatibility preflight is not evidence that those platforms work.
