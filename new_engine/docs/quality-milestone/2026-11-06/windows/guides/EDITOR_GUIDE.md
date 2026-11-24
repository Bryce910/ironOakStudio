# Editor reference

## Camera and selection

Hold right mouse with WASD to fly and Q/E to move vertically. Shift boosts speed; Ctrl gives precise movement. Wheel over the viewport zooms; right mouse plus wheel changes fly speed. **View → Fly speed** sets it directly. **F** frames selection or picked ground.

Release right mouse to select. Q/W/E/R choose Select/Move/Yaw/Scale. Ctrl+click adds or removes a selection. Ctrl+C/Ctrl+V copy and paste; Ctrl+D duplicates; End snaps to terrain. Inspect the object before saving a large repeated layout.

## Landscape

Use **Landscape → Find open ground**. **Shape** raises/lowers/smooths terrain, **Plant** paints independent vegetation layers, and **Water** creates a pond, lake or river. Short strokes are easier to review. The live shape preview is a draft; release to build. Undo restores the whole operation.

Imported floors, buildings, fixed routes and existing water have protections. Orange means the current tool overlaps a protected area. Move the entire brush/shoreline to clear ground. Fences and walls placed as ordinary scenery may need individual collision settings.

For a pond/lake, choose size and depth, position the whole outline on clear ground, then click once. Basin and water commit together. Rivers need at least two points and **Finish river**. New ponds retain an editable basin recipe. In Water, select the pond, adjust **Editable basin diameter** and depth, then **Apply basin size**. Use its move action or **Move pond to picked target** to move basin and water together. Moving, deleting or disabling a derived pond restores the former ground; Undo restores the whole edit. Existing zero-depth legacy ponds keep their baked sculpt and do not gain this restoration behavior automatically.

Layered plant painting selects grass, trees, rocks or low plants independently. Painting one layer preserves other selected-world layers. Committed generated tree trunks and rocks block walking; grass and low plants remain visual. Collision follows the published generation, not the uncommitted paint preview. Individual objects and Physics remain available for other solid obstacles.

## Apply, save and recover

Ctrl+Shift+S applies the draft and saves the world. Ctrl+S saves an already applied world. Ctrl+Z/Ctrl+Y undo/redo. Close the editor normally after saving. Recovery is separate from a clean saved scene: read any recovery prompt before discarding work.

Studio version 9 supports independent vegetation layers and basin provenance. Versions 1–8 remain readable. New format 9 saves need this updated runtime and its matching validation/export helpers; keep old deliveries separate.

## Areas and collision boundaries

City structural collision is a separate resource. Grandstone architecture is assembled in combined chunks. Source structures backed by a separate cooked collision resource are protected against edits that would desynchronize the scene; the editor does not automatically recook that collision. A new structural layout requires matching collision authoring. Test stairs, doors and walkable ground in game view after structural work.

The apartment/arrival seed contains the current milestone environment. F11 switches to game view for a short walkthrough; the separate demo is the story presentation. Use the Outliner and F to inspect distant objects and apartment assemblies.

## Returning work

Save, close the editor, then use **Return Saved Changes** in the frontend. Send the new return folder it opens. Do not send private player saves or replace the pristine seed. Keep asset credits and licenses with any permitted handoff.
