# Silverpine Waystation starting area

Build a compact, deliberately composed forest waystation in the existing native
Valwood engine. The area adds a weathered stone entrance, a ruined courtyard,
a working camp, a small grove and an overlook. Its purpose is a walkable visual
starting area; it does not imply finished quests, NPC schedules, crafting or the
planned living-world simulation.

[Editable layout](../projects/valwood_world/scenery/starting_area_layout.json)
defines the terrain and walking routes.
[Placement text](../projects/valwood_world/scenery/starting_area_instances.txt)
contains the scene objects.
[Actual placement plan](../projects/valwood_world/scenery/starting_area_layout.svg)
is an overhead SVG generated from those records, rather than a concept image.
[Placement manifest](../projects/valwood_world/scenery/starting_area_manifest.json)
records local/world poses, ground heights, actual GLB bounds and clearances.

## Composition and walk

The 250 × 220 m authoring field is centered on the forest entrance at map UV
`(0.511, 0.252)`. Local **R** means camera-right and **F** means forward for the
existing camera yaw of −0.55 radians. World origin is approximately
`(1108.8, −14063.783392)` meters. The untouched spawn height is about 335.796 m.

| Place | Center R/F, m | Purpose |
| --- | --- | --- |
| Entrance arch | −0.25 / 14 | A clear architectural destination immediately ahead; lower broken walls frame the passage. |
| Courtyard | −0.8 / 43 | Arrival hub with a small plank floor, dressed table, supplies and broken stone perimeter. |
| Ruin tower | −32 / 27 | A large silhouette on the left; its closed exterior is kept away from the route. |
| Workshop | 32 / 61 | Two dressed work tables, stacked crates, barrels, chopping stumps and an unlit firepit. |
| Overlook | 46 / 98 | A low plank viewing floor with a short ruin edge and an open view. |
| Grove | −15 / 117 | A quieter table/firepit clearing, flowers and a return path under the trees. |

The arrival stem is 43.007 m. The closed loop is 264.242 m; walking from the
start around the loop and back totals approximately **350.257 m**. The new path
has a 3 m clear walking strip; tree crowns can overhang above head height. The existing 2.28 km trail remains available through
the middle of the area. Five native view definitions in the JSON identify the
arrival, courtyard, workshop, overlook and grove at normal 1.75 m eye height.

The original terrain gently descends forward. Six local pads flatten only the
architecture and activity pockets, with soft 8–14 m margins; connections keep
the existing grade. A four-meter spawn core, the broader map, fifteen settlement
foundations and the original trail are retained. The runtime terrain overlay
and its independent grade/seam tests belong to the atlas integration.

The courtyard's paving footprint is 22 × 20 m within its unchanged 38 × 34 m
height pad. The entrance and tower also use smaller surface footprints. This
keeps the level foundations while allowing forest litter, moss and grass to
return around their edges. The same authored material mask controls terrain
appearance and vegetation exclusions.

## Models and placement

The current layout has **1,356 new records**, added to the original 283 records for
1,639 total. It includes one arch, one closed tower, twenty-five wall sections,
21 low plank pieces, five tables, twelve crates, eight barrels, seven buckets,
nine lanterns, three axes, eight stumps and two cold firepits. A total of 113 trees,
127 rocks, nineteen fallen logs, 319 ferns and 676 low flowering plants frame
the route. Thirty-six named planting beds, ten canopy belts and five sapling
cohorts vary size, species and density; their ground-level objects are clipped
away from the walking strip. This is a set of bounded
compositions rather than a global random scatter.

The canopy now favors fir and broadleaf groups, with younger pines around their
edges. Overlapping crowns frame the activity pockets while small logs, boulders
and fern beds connect the planted margins. Short connected wall returns behind
the workshop, grove and overlook give those pockets an enclosing edge. Mature
pine geometry remains naturally open below its crown; adding more identical
isolated pines would not produce a dense lower forest.

The new architecture and props use the sourced CC0 model families recorded in
[the asset manifest](../projects/valwood_world/models/starting_manifest.json).
Their original metric geometry and material structure remain visible. The gate
is approximately 7.41 m wide and 8.62 m tall, with a measured 3.50133 m opening.
It is buried 0.30 m to remove the raised source sill. The tower is approximately
15.84 m across and 13.50 m tall; it is a closed landmark, not an enterable room.
The reduced wall sections create an incomplete enclosure with actual gaps;
the courtyard pieces join into short broken wall runs rather than isolated blocks.
The runtime uses separate gate piers/header so its doorway remains walkable.

Tabletop lanterns and buckets and the four two-crate stacks use the optional
seventh placement column. The tool derives this elevation from the actual
supporting model's top and the integrated ground support calculation. Objects
are not all left on the ground. Lanterns are visual props and the firepits are
unlit; no dynamic light, fire or interaction system is claimed.

## Editable source and checks

Each record uses the existing restricted alias format:

```text
alias map_u map_v model_yaw_degrees scale burial_meters [elevation_meters]
```

Model yaw uses the engine's local-X rotation convention. A model whose long
axis points along local R uses **+31.512679 degrees**, while the camera's
forward yaw is −0.55 radians. The JSON carries both conventions explicitly.

`tools/build_starting_area.py` reads real transformed GLB vertex positions to
measure footprints; it does not use preview-image dimensions. It samples the
current CPU atlas, verifies dry roots, protects the original trail with the
loader's conservative full-crown radius margin, and preserves the new path
against rotated model bounds. For trees on the new loop, the tool clips actual
transformed triangles at 2.2 m above ground to measure the walking volume;
branches and crowns above that height may overhang the path. The gate's two
piers are tested separately, and the low plank
floor is allowed on a walking surface. The initial new-path footprint margin
is at least 0.205 m beyond its 3 m strip; the original-trail margin is at least
0.187 m. These are authoring checks, not a substitute for the independent
continuous player-capsule query or native walk-through.

From the engine root:

```sh
python3 tools/build_starting_area.py --validate-only
```

Directly edit the text for individual placements or edit the layout JSON for
terrain, routes and views. Existing placement output is preserved by default.
An explicit regeneration uses the named compositions in the tool and replaces
those editable outputs:

```sh
python3 tools/build_starting_area.py --force
```

The terrain include and viewer's baked route/view definitions must be refreshed
when their JSON coordinates change. Regeneration does not modify
`authored_instances.txt`, model files, the map image or the Seldonne project.

The new-record cap is 1,450, within the combined loader's 1,800-record bound.
The largest family contains 528 instances, below the 768-instance batch cap.
The new records total approximately 5.23 million triangle-instances if every
object used its near mesh simultaneously. That is a conservative content count,
not a measured frame workload: distance, view culling, size fades and existing
LOD paths reduce the rendered work. All objects remain instanced by model.
Native captures must establish whether the pockets read as a coherent place;
native frame profiles must establish the added GPU cost. Counts, an SVG and a
successful loader cannot establish finished cinematic quality or a guaranteed
45–60 FPS everywhere.
