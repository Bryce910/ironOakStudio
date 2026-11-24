# Village ground material history

The village core uses one cached material mask to connect the source cobbles to
foundations, working cargo, traffic and drainage. It changes material shading;
terrain height, collision data and walking routes are unchanged.

`scenery/village_ground_history.json` defines the editable traffic, well and
gutter paths. `tools/build_village_ground.py` reads the final scene manifest and
intersects the actual source triangles at 10 cm above the village ground.
Those real sections preserve gate openings and separate table legs; overhanging
eaves cannot stamp a convenient whole-model box onto the ground. Elevated cargo
does not create an unsupported ground contact.

The linear RGBA8 mask covers local `[-90,-70]..[90,110]` at 1,025 square texels
(17.58 cm spacing). It uses 4.008 MiB at the base level and 5.341 MiB with its
complete mip chain. Row zero is minimum local Z. Three metres of boundary fade
keep the cached core continuous with the surrounding village.

| Channel | Material history | Effect |
|---|---|---|
| R | Soil beside actual foundations and working cargo | Soil albedo, normal and ARM blend into cobble recesses |
| G | Authored footways and delivery-wheel traces | Cleaner, slightly burnished stones with reduced normal amplitude |
| B | Well overflow, gutters and narrow damp foundation margins | Local darkening and lower roughness, strengthened during rain |
| A | Sediment at the authored paving boundary | Earth blends across the cobble/grass shoulder |

The mask does not contain lighting, and no random field can create an unrelated
global dirt patch. Small broad variation is restricted to actual contact
deposits. Existing soil/cobble textures supply the material detail. Albedo stays
linear after source sRGB decoding; ARM remains AO in R, roughness in G and a
dielectric metallic channel. Damp areas change the actual BRDF roughness rather
than relying on a darker painted color.

The terrain remains within GL 3.3's 16 fragment sampler units. The unchanged
soil normal and ARM pixels share `uLitterPBR` at unit 13, as array layers 0 and
1. This frees unit 14 for `uVillageGroundMask`. The history field adds one
filtered sample and uses no extra fullscreen pass or geometry displacement.

Regenerate after the scene or relevant source meshes change:

```sh
python3 tools/build_castle_village.py --force --require-models
python3 tools/build_village_ground.py
make test-world-village-ground test-world-lighting test-world-shadow-filter
```

`--validate` rebuilds the mask in memory and compares every published byte and
input hash without writing files. The generated
`scenery/village_ground_history_manifest.json` records scene/model hashes, real
ground sections, channel coverage and pixel identity. CPU controls reject eave
and elevated-cargo contact, preserve a gate gap and check independent authored
traffic/well/edge locations. The actual GLSL fixture isolates soil normals,
roughness and dielectric ARM packing, including a black-albedo specular control,
and checks disabled/outside/elevated and world/time invariants.
