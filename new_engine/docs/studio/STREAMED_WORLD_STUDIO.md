> Historical core-tool guide. For the current editor, full transforms, coordinated saves, surface/road tools and packaging, use [Valwood editor guide](../editor-completion/2026-10-30/EDITOR_GUIDE.md). Some future-work statements below were superseded by later implementations.

# Valwood streamed-world Studio

`Launch_Studio.sh` opens the castle arcade at walking height in the custom streamed world. **F11** toggles Studio in a world session; **F10** opens graphics settings. **RMB** looks, **WASD** moves, **F** toggles flight, **Shift** accelerates flight and **V** switches first/third person. Studio flight speed is editable from 1–100 m/s. `Launch_Legacy_Studio.sh` preserves the earlier editor.

Changes are drafts. **Apply / rebuild** validates and commits to the real streamed scene; **Save / Ctrl+S** atomically saves the applied document to `new_engine/projects/valwood_world/world.studio`. **Undo / Ctrl+Z**, **Redo / Ctrl+Y** and **Load** restore a draft; Apply previews it. Failed imports, scene preparation or shader compilation retain the committed scene. Applying structural changes can pause editing. Walking FPS is measured separately from these explicit loading operations.

The **Tool** dropdown reaches every tab, including **NPC / Nav**, **Particles**, **Water** and **Graph**. New saves use document version 3; versions 1 and 2 still load. These tools use the same draft, Apply, Save and undo workflow.

## Select and transform

Open **Objects** and use the floating **Select / Move / Yaw / Scale** toolbar. Click an authored model to select it; **Ctrl-click** toggles additional objects. Picking uses background triangle intersection on supported static GLBs, with bounds fallback for unsupported sources. It does not test transparent texture pixels. Procedural grass and forest roots use the Foliage tools instead of individual object selection.

Move handles support XYZ translation, the yaw ring rotates around the vertical axis, and Scale changes each object’s uniform scale and the group’s XZ spacing. Snap controls or **Ctrl** constrain changes to steps. One drag creates one undo transaction; **Escape** restores its beginning. Gold wire bounds preview the draft; rendered geometry and collision change on Apply. Numerical controls include east/south position, yaw, scale and vertical offset. Pitch/roll and nonuniform scale are not implemented.

Duplicate selected source or added objects, hide source objects, delete added objects, and save named groups. Group selection can be disabled for editing an individual member. The nearby hierarchy includes added and hidden objects and filters before limiting to 256 nearby roots. Source groups and overrides carry a source-scene fingerprint; changed source placements stage the document for repair instead of applying old indices to different objects.

## Assets and materials

**Inspect selected asset** opens **Assets**. Inspect each real LOD to see vertices/triangles, bounds, embedded map resolutions, map roles/color spaces, source normal scale and roughness/metallic factors. Diagnostics identify missing maps, unsupported UV features, separate AO packing, cutout fallback for BLEND, and expensive geometry or textures. The RGBA mip estimate is a reference estimate, not measured VRAM; it excludes compression and sharing.

Use **Browse** or enter full source paths for static embedded **GLB** files. Supply metres per source unit and optionally separate reduced LOD meshes; trees require at least two LODs. Import copies a content-addressed immutable revision into the project, validates CPU geometry and then actual GPU model loading, and stages a definition. Apply replaces the scene only after preparation succeeds. Purchased sources remain intact. Reimport uses a new revision, allowing undo to restore the old definition. FBX conversion, rigging and animation retargeting remain separate character/cooking workflows.

LOD draw distance and split are editable per alias. Collision choices are none, box, circle, trunk or preserve source. **Preserve source** retains castle/tavern doorway openings; a whole-model box can close them. The real-mesh LOD preview temporarily forces one selected committed object within 128 m to a chosen LOD. Return to Automatic or close Studio to restore normal selection. There is no collision-shape sculptor or general mesh preview viewport in this pass.

**Materials** changes tint, roughness multiplier, normal strength and UV tiling per model alias across its LODs. It preserves embedded source maps and source normal scale. UV changes also affect masked shadows. These controls do not replace individual texture images or edit procedural cover materials. Facade detail samplers use up to 8× anisotropy while retaining shared image ownership.

## Lighting and the world

| Tool | Current behavior |
| --- | --- |
| Terrain | Raise/lower/flatten/smooth, radius and strength; up to sixteen local 128 m tiles at 2 m sample spacing. Terrain, walking, scenery grounding and weather share the committed heights and triangle interpolation. Foundation, water and fixed navigation regions are protected. |
| Foliage | Separate procedural grass/tree/rock/fern density masks; restore density, undo or clear. Authored plants/trees support object placement, scatter and transforms. |
| Lights | Point/spot color, intensity, radius, direction, cone and flicker. They share eight active practical-light slots with existing torches and do not add shadow passes. |
| Splines | Linear or Catmull-Rom points, arc-length spacing and baking into grounded/colliding instances. Baked copies are independent; repeated Bake adds more copies. |
| Surface patches | Rotated box projections for procedural soil, moss, wear and wet stains on terrain/rigid geometry, before lighting. Thirty-two saved patches, at most eight nearby active. Fine grain fades with pixel footprint to reduce distance shimmer. Arbitrary texture decals are still separate work. |
| Atmosphere | Time/bearing/direct sun, sky irradiance, exposure, bounded sharpening, cloud, rain, wind and ground mist. The rainforest retains its overcast canopy shelter response. |
| Shaders | Transactional compile/reload of terrain, model and water GLSL. A failed compile keeps the old programs. The separate Graph tab generates model material/vertex code. |
| Performance | Saved graphics presets, render scale, auto-quality, shadows, reflections, contact shading, volumetric fog/clouds, FPS and terrain memory. |

**Atmosphere → HDR environment** imports a 2:1 Radiance `.hdr` panorama, up to 4096×2048 / 64 MiB. Enable it, rotate it and adjust intensity/blend, then Apply. Rotation affects sky, SH diffuse irradiance, GGX-filtered reflections and water together. A 256-side raw/filtered cube pair plus BRDF LUT uses approximately 6.06 MiB; source floats are released after loading. The sky retains clouds/fog and rainforest shelter, terrain receives diffuse irradiance, and models/water receive reflections. Align the independent sun controls to the panorama manually. Intensity and rotation do not rebake the environment. No HDR is enabled in the supplied demo look: the generated QA panorama is only a test fixture.

Structural Apply joins terrain workers and rebuilds grounding/collision, procedural roots, roof/canopy/weather masks and relevant local-light caches. Unchanged scenery geometry transfers ownership instead of uploading again. Material/look-only edits avoid geometry rebuilds. No automatic asset-folder scanning or asset import starts during normal walking. Explicitly requested background jobs can finish after switching tabs.

## NPCs and navigation

In **NPC / Nav**, click dry ground to set a target, then **Add resident at target**. Choose from the ten loaded appearances, name the actor, set speed/pause/facing, and append target waypoints. A single point is stationary; multiple points return along the same route unless **Closed patrol** is enabled. Idle-only appearances cannot patrol. **Edit nearest existing resident** overrides a village resident within 12 m of the target; removing that edit restores the source resident. Added residents reuse existing pose/material batches rather than loading more characters.

For an obstacle detour, create a navigation area at the target and assign it to the actor. Set span, cell size, radius and slope. **Bake / preview committed ground** shows a sampled green walkable overlay; Apply rebakes against the draft terrain and scenery, checks exact endpoints and computes A* routes. Water, solid obstacles, steep terrain and disconnected routes cause rejection. Direct routes still receive swept capsule and water checks. Authored waypoint lines appear in the viewport.

Budgets: four local areas, at most 96×96 cells per area / 32,768 cells combined; 32 resident additions/overrides; 32 authored waypoints and 128 baked corners per actor. Paths bake on Apply and animate at runtime with distance-driven gait and eased turns/stops. This is static local navigation: dynamic crowd avoidance, automatic world-scale navigation and editable behavior trees are separate work. The original 140 residents retain their authored behavior unless explicitly overridden. The bandit, animal and training systems are separate from this resident editor.

## Particles

In **Particles**, create an emitter at the viewport target or copy/override an existing smoke, dust or fire source. Edit position, bounds, intensity and ceiling. Smoke/dust expose birth-rate, lifetime, size, warmth and wind-response multipliers. Fire uses the purchased flame/ember renderer and can contribute a flickering practical light. Apply previews the result; deleting a source override restores the original effect.

Sixteen editor records share the existing fixed 512-instance smoke/dust budget and 56-emitter flame capacity. Fire bounds include embers up to 3.2× the selected flame height; raise the ceiling when increasing height. Practical fire lights share eight active light slots and add no shadow passes. These are three supported effect families, not a general particle simulation graph or arbitrary sprite importer.

## Water

In **Water**, select **Ocean**, **Forest pool** or **Castle moat** to edit transmission tint, absorption, roughness, wave amplitude, flow and foam. Existing shoreline geometry and levels stay fixed. **Carve 6m pond at target** blends a 10 m bank into suitable open terrain and sculpts a basin in one undo step. It needs dry ground clear of protected foundations, fixed routes and nearby architecture. Alternatively, **Add pond to existing basin**, then adjust the plane and ellipse radii. Apply requires at least 8 cm center depth and a dry rim, rejecting floating water edges.

Custom ponds connect to water-surface queries, vegetation exclusion, weather and navigation. They use the opaque depth/refraction snapshot and environment reflections, keeping the existing single planar-reflection budget. Up to eight water records, with custom radii from 1–32 m; nearby ponds render within 220 m. Removing a water record leaves separately sculpted terrain; Undo restores the combined carve operation. This is bounded surface shading and basin authoring, not a fluid solver, river spline or shoreline sculptor.

## Shader graph

Select an authored model in **Objects**, then open **Graph** and create a starter for that model alias. **Expand graph workspace** enlarges the resizable Studio window. Starters include source tint, animated emission and gentle vertex motion. Add typed nodes, drag their headers, middle-drag to pan, and click an output pin followed by an input pin to connect. Selected-node controls and **Output connections** offer numerical/dropdown editing. **Compile graph / Apply** validates the graph, generates GLSL and compiles replacement programs; invalid connections, cycles or compile failures preserve the live world.

Sixteen node types cover scalar/color values, source PBR channels, UV/world/time, add/multiply/mix, sine/noise/clamp and Fresnel. Outputs are albedo, roughness, metallic, normal strength, emission and vertex offset. Eight model-alias graphs with 32 nodes each are supported across their LODs. Vertex displacement is shared by color/shadow programs, capped at 15 cm per axis and leaves collision unchanged. Unassigned model aliases keep source behavior. This graph does not yet target terrain, water or NPC materials, import arbitrary texture nodes, or create new render passes. Raw GLSL reload remains in **Shaders**.

## Demo layer and limits

The supplied `world.studio` look layer calibrates existing building/vegetation/rock materials and applies seven restrained ground-integration patches around measured planted pockets. It adds no mesh or image resources and leaves terrain, navigation, NPCs, animals and purchased placements intact. `tools/build_studio_demo.py --output /tmp/my-demo/world.studio` reproduces a reviewable candidate without overwriting an existing output. Edit the live result in Studio and Apply/Save normally.

Flatten suitable open ground before adding architecture. Adding a building does not create an interior, navigation route or doorway. Fixed NPC/pet routes, torch/FX sockets and paving/biome masks are separate authored systems: moving a building does not move those attachments. Fixed castle cover occlusion is disabled after architecture moves so it cannot hide foliage behind a moved wall incorrectly.

This is a working custom-engine authoring pass, not a finished commercial editor. Dynamic crowd navigation, general particle graphs, fluid/river tools, wind volumes, arbitrary texture decals, broader import formats and project packaging remain future systems. High-poly scattering and large textures can exceed the 3 GB GPU budget; benchmark the edited scene before treating its frame rate as proven.

[Verification, captures and performance](authoring-tools-2026-10-28/RESULTS.md) records the measured routes and practical limits. [Earlier core-tool verification](streamed-world-2026-10-28/RESULTS.md) remains historical evidence.

[Runtime-editor verification](runtime-editors-2026-10-29/RESULTS.md) records the NPC/navigation, particles, water and graph tests and edited-world measurements.
