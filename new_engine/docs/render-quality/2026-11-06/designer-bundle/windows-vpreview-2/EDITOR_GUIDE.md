# Environment editor guide

Open **Valwood World Designer.exe**, choose an area and use **Open Editor**. Work in the selected independent workspace. Use [FIRST_ASSIGNMENT.md](FIRST_ASSIGNMENT.md) before production changes.

## Navigation and placement

| Action | Control |
| --- | --- |
| Editor / world preview | F11 |
| Fly camera | Hold RMB + WASD; Q/E down/up; Shift faster; wheel changes speed |
| Select / move / yaw / uniform scale | Q / W / E / R outside camera flight |
| Full position, rotation and axis scales | Inspector |
| Extend selection | Ctrl+click |
| Frame selection | F |
| Copy / paste / duplicate / delete | Ctrl+C / Ctrl+V / Ctrl+D / Delete |
| Snap selected models to terrain | End |
| Undo / redo / cancel gesture | Ctrl+Z / Ctrl+Y / Escape |
| Save applied scene / Apply and Save | Ctrl+S / Ctrl+Shift+S |
| Find a tool or command | Ctrl+P |
| Graphics settings | View → Graphics / performance; F10 in world preview |

Shortcuts are guarded while typing. Select objects in the viewport or outliner. Search the Content Browser and drag supported assets into the viewport. Searchable labels, type icons, favorites and real mesh/material inspection are available; asset cards do not promise rendered mesh thumbnails.

Editor and player cameras keep separate positions. F11 returns to the saved preview position; it does not start a walk from the current flying camera. Put the first practice props near the initial walkable area so you can reach them for the ground-level check.

Copy a selection, then paste repeatedly to form a row. **Edit → Paste settings** controls direction relative to the copied object's yaw, gap and snapping. Spacing uses transformed mesh bounds, including rotation and scale. Each paste is an undo step. **Reveal pasted copy** frames the model and neighbor; F frames the selected copy again. Check clearance against surrounding buildings and roads: row spacing is not a general collision-avoidance solver.

**End / Snap to Terrain** seats transformed bounds against the terrain support under the footprint. It preserves X/Z, orientation and scale. Separate selected props ground independently; groups retain their arrangement. It does not align objects to the slope, flatten plots, rebuild paving or regenerate architectural collision.

Object-only edits normally auto-preview. The actual mesh follows transform gestures; Escape cancels. Changes to scale, tilt or structure can require a longer Apply boundary. Use **Apply** before reviewing final cached lighting and shelter masks after moving architecture or trees. Saving persists the applied world.

### Find the apartment

**Apartment & Valois Arrival** opens at the meadow reference. In the **Scene Outliner** search field, enter `nm_opening_reference_home_story`, select its result, then right-click and choose **Frame selection**. Alternatively, leave the search field, click the viewport and press F with that object selected. Allow the model to stream, then fly through the doorway with RMB + WASD. `nm_opening_reference_home_enclosure` identifies the room shell. Use the outliner to locate these existing objects; dragging from the Content Browser would add another instance.

The apartment is an elevated reference interior. Keep its shell, floor and story anchors in place during orientation; terrain snapping the apartment shell changes its authored height. The editor does not automatically start the opening story. Its View menu controls the workspace layout rather than providing an apartment camera button.

## Landscape: shape, plant and rivers

Click **Landscape** in the editor's top bar, then **Find open ground**. The camera frames terrain away from buildings and imported floors. A green outline shows the brush; a blocked spot is orange. Imported city/countryside ground is separate from sculptable terrain. If a change is blocked, use the message and move to open terrain.

| Page | Simple workflow |
| --- | --- |
| Shape | Choose Raise, Lower, Flatten, Smooth or Rolling hills. Set Brush width and Strength. Drag on the ground, then release to build. |
| Plant | Choose Forest, Meadow, Jungle, Trees, Grass, Plants or Rocks. Set Brush width and Density, then paint. |
| River | Click a short downhill route, set River width, Banks and Depth, then Finish river. The channel, banks and water build together. |

**Smaller / Larger** changes brush size quickly. Flatten holds the height of the stroke's first ground pick. Rolling hills uses repeatable world-space variation. Each completed drag is one Undo; Escape cancels the entire current stroke. **Undo landscape** restores the previous state. **Save landscape** applies and saves; saving remains necessary after the automatic build.

Planting creates saved **painted areas**, with generated trees and ground cover streamed around the camera. It does not spend the 1,024 individual added-prop slots. Painted trees and rocks are visual scenery and do not add solid collision. Use individually placed collision-enabled models or explicit Physics obstacles when a trunk or rock must block movement. Later areas replace the entire growth mix in their overlap. **Clear growth** clears generated growth; imported plants and individual model props remain separate objects. Select a **Painted area** and use **Remove this area** to reclaim its slot or reveal the older underlying mix. There are 32 saved areas; one long drag can use several. Start with a short stroke and inspect it before increasing density.

Rivers support 2–32 points, 0.25–256m apart and at most 1,024m in total. **Undo last point** fixes an unfinished route; **Cancel river** discards it. The simple tool uses a downhill, linear water profile. The editor can reject steep cuts, crossings through protected buildings, paths, model roots or existing water, and routes that exceed a budget. Rejection preserves the existing landscape. There are 16 road/river paths. UI river width is 4–24m, banks 2–12m and depth 0.5–4m. Keep the first river short and gentle.

Landscape sculpting uses up to **64 tiles**, each 128m with 2m sample spacing. Brush width is 16–512m. Combined sculpt and road/river displacement is limited to **±32m**; increase the base world/source terrain for larger mountain changes. The editor protects fixed foundations, paths, camps, water and imported ground. Review the result from ground level and several camera distances.

Open **Advanced landscape tools** for heightmap import/export, erosion and constrained biome/corridor tools. Those stage a draft and require **Apply**. The older **Terrain** tool remains available for small stamps. **Foliage** masks only suppress existing procedural growth; they are separate from the new Plant areas. **Objects** scatter creates individual placements and uses their normal capacity. Leave path shoulders readable, and check roots, repetition, wind and LOD transitions before adding density.

## Surfaces

**Surfaces → Texture library** imports source images using a path field. Choose the correct map role. Albedo is sRGB; normal and ARM maps are linear. ARM channels are **red AO, green roughness, blue metallic**. Choose DirectX normal inversion only for maps using that convention. Create a PBR layer, bind it, paint or project it, then Apply and Save.

A model binding affects every instance and LOD of that alias. Terrain painting changes material weights; sculpting and foliage masks are separate. Use local projection boxes for restrained dirt, moss or wear. Nearest projections have bounded draw limits. Consult the layer and resource counters before importing additional images.

## Lighting, atmosphere and VFX

Use **Lights** and **Atmosphere** for practical lights, sun direction, environment, cloud/fog and scene mood. Check buildings, faces and vegetation under the same intended lighting. There are eight active practical-light slots shared with supported effects; local lights have no independent shadow maps.

For an HDR environment, use **Browse HDR** and import a 2:1 Radiance `.hdr` panorama, at most 4096×2048. The in-editor browser has a **Folder** field, **Open**, **Parent folder**, and double-click selection. Align the separate sun controls with the panorama. Environment loading/filtering happens during Apply.

Open **VFX Studio** for continuous fire, smoke, mist, dust, fireflies and other presets. Add in front of the camera or place on the ground; frame, move, tune, duplicate and preview with pause/restart/scrub/solo. The scene supports 16 authored emitters within shared budgets. Save stores their settings. Preview playback and solo state are temporary. Combat events and character effects use separate game systems.

## Import static environment models

Prepare a static glTF 2 **GLB** with embedded buffers/images, UV0, metre scale and baked transforms. Preserve original source files. FBX and Unity packages need offline conversion; the native static importer does not perform character rigging or general animation authoring.

In **Asset inspector → Project import / reimport**, choose a unique alias or replace an existing one, browse for LOD0 and optional LOD1/LOD2, set **Metres per source unit**, type and collision, then stage the import. Trees require LOD1. Inspect dimensions, triangles, maps and supplied LODs; Apply and Save after validation. Reimport creates an immutable project revision and retains the original files.

Supported GLBs are bounded to 128 MiB, one million vertices/triangles, 128 triangle primitives and 128 embedded images. Dense assets still need reduced LODs and practical texture sizes. Transparent BLEND uses a cutout fallback; sorted imported glass is unavailable.

**Preserve source collision** retains supported existing collision. A whole-model box blocks visual doorways. New GLBs do not automatically gain detailed architectural collision. Test the actual walkable result.

## Save and organize

Use **Ctrl+Shift+S** to Apply and Save the scene. **Ctrl+S** saves the applied scene. Simple Landscape strokes build on release; **Save landscape** still saves explicitly. Advanced terrain, material and import previews require Apply. An unsaved recovery draft is a recovery aid, not a delivery.

After editing separate typed subsystems, use **File → Save project checkpoint** and wait for completion. It coordinates supported authoring documents and retains a previous generation. Interrupted saves expose finish/rollback actions in Production. Restoring a checkpoint requires a restart so all systems load the same generation. Keep personal player saves separate from authoring.

Use **Regions / Layers** for names, visibility and locks. Apply and Save identities before **Save region document**, then save that metadata explicitly. Regions are organizational; they are not network locks or independently mergeable scene files. Coordinate one editor of each authoring file at a time.

Before returning work, save, close, reopen and inspect. Close again and choose **Return Saved Changes** in World Designer. Returns contain saved supported authoring changes and new runtime imports, with conservative file conflicts against the matching seed. Keep original purchased/Blender source archives outside the runtime return.

## Current environment boundaries

- **City:** building, boundary, stair and paving visuals use separate authored collision/ground resources. Moving, copying or hiding their scenery does not rebuild `city.physics`. Structural changes need source/collision regeneration and a walking recheck.
- **Grandstone:** source architecture is spatially combined into chunks. Selecting a pillar may select a whole chunk. Source editing and recooking are needed for individual module changes. Complete navigation acceptance remains a separate task.
- **Apartment & Valois Arrival:** landing, conversation anchors, camera framing and approach routes are linked gameplay data. Discuss movement of these anchors before changing them. The environment editor opens at the meadow reference.
- **All areas:** scenic presets are views of the same project. Alias-wide materials and atmosphere can affect areas beyond the current camera. Keep changes small and review their extent.

| Authoring resource | Current bound |
| --- | --- |
| Custom asset families | 128: city uses 99; jungle 20; Grandstone 121; opening 128 |
| Added model placements | 1,024 per scene document: city 0 used; jungle 960; Grandstone 0; opening 10 |
| Selection / undo history | 128 selected entities / 32 history checkpoints |
| Sculpt tiles | 64 tiles, each 128 m with 65×65 samples; combined displacement ±32m |
| Generated growth | 32 painted areas, each up to 256m radius; streamed around the camera |
| Roads / rivers | 16 paths; 32 points each; 1,024m total per path |
| Scene lights / active practicals | 24 records / 8 active slots |
| Surface images / PBR layers | 16 / 8 |
| Terrain paint | 256 strokes across 64 paint tiles |
| Authored emitters | 16, with shared particle budgets |

Base world scenery and streamed Plant areas are separate from the added-model limit. The jungle currently has **64 added-placement slots available** for individual props; the first small-prop exercise fits. The opening has no unused custom-family slots; reuse or deliberately replace existing definitions. These are validation limits, not recommended performance budgets. Review capacity before new imports or large placement batches.

This edition saves generated-growth data using Studio version 8. Use this edition's editor, validator and return tools for those files; older editor/game executables cannot open the new saves. Keep older deliveries and work folders separate. The owner uses the matching new Windows or native Linux runtime/validator when reviewing and deliberately migrating work between baselines. Replacing an EXE inside an old project is not an update procedure.

## Check quality and performance

Use **View → Graphics / performance** while editing, or F10 in world preview, to choose settings appropriate to the receiving GPU. Record settings with before/after screenshots and use identical settings for comparisons. The owner's RX 5300 has 3 GB VRAM; extra high-resolution textures, long draw distances and many lights can exhaust its budget even if a stronger PC looks smooth.

Production shows Apply duration and tracked geometry/texture pressure. These counters are not total measured VRAM. Watch missing/late models, LOD jumps, shimmer, floating props, blocked doorways and path readability. Keep profiling JSON, CSV and world sidecars when available. Loading and explicit Apply are functional checks; a still image or startup pass does not establish sustained gameplay FPS.

For support, include the area, selected workspace, exact steps, alias, screenshot, graphics settings, GPU name and `logs/editor-latest.log`. Frontend operation logs are beside the extracted bundle in `<bundle folder>_Work/Logs/`.
