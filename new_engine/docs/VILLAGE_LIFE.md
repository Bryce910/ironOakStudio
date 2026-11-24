# Western streets, tavern and sheltered residents

The village population adds twelve people after the existing128 definitions:
four neighbors in the sheltered nook, six tavern patrons/bar staff and two
walkers on the western street. Existing128 identities, routes, gait phases,
source-pose indices and blend indices are preserved. The total is140 people.

Four people rest seated: two tavern patrons and two sheltered neighbors. They
use the genuine `Sitting1` clip from the retained CDmir/TinyWorlds bartender
source, not a rotated standing mesh, hidden legs, or a claimed sleeping clip.
Other residents use the existing walking and idle appearances. Names and roles
describe neighbors and their present activity; no illness, intoxication,
criminality or other behavior is inferred from housing circumstances.

## Source and offline conversion

- Source: `assets/characters/incoming/cdmir_kelgar/bartender-IK.blend`.
- Creator: CDmir (Čestmír Dammer), with TinyWorlds; retained CC0-1.0 source and
  provenance in the same directory, including the original source-page record.
- Four keys at source frames0,48.5,97,145.5 sample the genuine194-frame loop
  at24FPS. Runtime interpolates adjacent keys over8.083333seconds.
- `tools/build_village_seated_characters.py` evaluates the original skeleton
  offline with two CPU threads. No runtime rig, source-file modification or new
  source animation is claimed.
- The fixed root follows the visible floor atY+.015m. Posterior contact sits
  .480m above that root, matching the actual.495m bench seats. Feet remain
  visible and grounded; hip/knee articulation comes from the source action.
  Seated roots sit.20m forward of the physical bench centres, allowing the
  genuine source apron to hang beyond the seat front. The two occupied dining
  tables and their empty rear benches move.40m forward to leave the full
  articulated body clear. Triangle clipping verifies the seat support rather
  than testing only isolated source vertices.
- Each of the four static GLBs has3,884triangles and one PBR primitive. Three
  shared1024×1024 atlases retain source diffuse, normal and AO; roughness.76 and
  metalness0 are explicit constants because no corresponding source PBR maps
  were supplied. Total new RGBA8 plus complete mip chains is16MiB.
- Original source quads are triangulated once before posing. An unused offline
  UV1 corner identifier permits lossless vertex/triangle permutations after
  glTF export, retaining the exact posed surface and stable adjacent-pose
  topology. Runtime samples the actual remapped UV0. Old76assets are untouched.

The machine-readable source, asset hashes, bounds and conversion details are in
`projects/valwood_world/models/village_seated_manifest.json`. This is a small
reused source character, with its original silhouette and detail; its quality
should be assessed in the actual tavern lighting rather than described as a
finished AAA character pipeline.

## Authoring and bounded runtime

`projects/valwood_world/scenery/village_life_layout.json` holds the twelve local
definitions. `python3 tools/build_village_life.py` generates
`engine/world_village_life_data.inc`; `--check` verifies it without mutation.
The tavern is centered at village-local(-145,-82), and the open-front shelter
at(-166,15). Their furniture anchors are owned by the environment layout.

The runtime uses80source poses and95fixed blend batches. Original source0..75
and blend0..90 indices remain stable; four source poses and four cyclic batches
append. Updates contain no file IO, allocations or uniform lookups, and repeated
updates at the same absolute time upload no unchanged instances. Pose data and
source assets retain the established shared texture-cache ownership and cleanup.

Existing role totals16knights/16archers/20traders/76travelers describe the preserved
population. Separate counters report4sheltered,6tavern,2western-street and4seated
people; these district counters overlap the seated activity count.

## Correctness checks

`tests/world_village_population_test.cpp` exercises all80partial-source failures,
blend/target failure cleanup, the original128motion fingerprint at15absolute
timestamps, fixed-time/cadence equivalence, no hot allocations, repeated-time
uploads and600seconds at60Hz. Its full140-person run measures350,280,000pair
separations and5,040,000ground/collision probes. Original minimum1.4m crowd
clearance remains intact. Seated bodies intentionally share their support
furniture's collider volume; their actual articulated geometry is checked
separately instead of treating them as standing capsules.

`python3 tests/village_seated_asset_test.py` reads the actual GLBs without a GPU:
stable indices/UV/material images, unit normals, articulated65–115degree knees,
grounded feet, matched posterior contact, bounded image memory and affine
between-key floor bounds. The actual GL fixture additionally loads every pose,
checks materials/cache ownership and draws both the established guard view and
a close seated tavern view in color, shadow and reflected passes. Native visual
and performance evidence is recorded separately by the integration run.
