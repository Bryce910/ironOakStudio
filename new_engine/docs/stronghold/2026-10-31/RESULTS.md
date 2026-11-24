# Grandstone stronghold and disk cleanup

This is an explorable first pass of Brandalhall in the Great Mashito Mountains,
using the user's local dwarf and dwarven architecture sources, existing scanned
rocks and timber, and local Poly Haven snow/ground materials. The initial district required no purchase or remote
asset download. The entrance art revision below adds three free CC0 cliff scans. It is a working project, not a new standalone
presentation package or a completed AAA art pass.

## Open

- Game: `/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold.sh`
- Editor: `/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold_Editor.sh`
- Project: `projects/valwood_stronghold`
- Binary: `rpg_grandstone`

The original `rpg`, castle/jungle Studio document, graphics preferences, jungle
expansion data, and player saves are protected separately. See
`protected-verification.json`. Shared project resources are read through local
links; copy/export the required resources before moving this project to another
computer.

## Area and snow

The latest mountain revision contains 897 editable placements, a hollow mountain shell, an open
stone gate, a 64 by 135 metre hall, columns, banners, braziers, four forges,
workbenches, anvils, and a throne area. The pointed gate has an 18 metre ground opening, a 25.8 metre inner peak,
and a carved crest reaching roughly 47 metres. Its stone throat is 20 metres deep. Terrain and capsule collision retain a continuous
walking floor through the portal. The scripted rehearsal uses ordinary walking
and collision checks along a 413.9 metre return route.

Twenty residents use four supplied dwarf appearances and actual idle/walk clips,
with four walking patrols. The imported Broddi armor has incorrect transforms;
that family is deferred and its unused generated poses were removed. The source
archive remains available for repair. The king's detached source weapon was
excluded from the live crowd mesh. Dense body/equipment geometry was reduced
offline where needed; face/hair geometry was retained. Pose indexing preserves
every triangle attribute across the complete bank. Blender UV coordinates are
converted to the embedded glTF image origin during import.

The snow pass uses scanned ground color and normal maps, slow drifting flakes,
alternating boot impressions, tread detail, displaced powder puffs, and generated
granular footstep audio. Prints come from player travel rather than pre-placed
decals. Their fixed 384-slot buffer fades over 180 seconds; powder uses a fixed
128-slot buffer and a 0.95 second lifetime. Roof cover keeps precipitation out of
the hall. This is a bounded surface/effects system, without volumetric snow
excavation or a persistent deformable snow simulation.

## Winter terrain revision (31 October)

The forecourt now uses fifteen shared-edge Studio terrain tiles instead of the
original flat nine-tile pad. Wind-shaped banks and rolling deposits are real
terrain geometry: walking, capsule grounding, terrain shadows and boot prints
sample the same triangulated surface. The main approach stays gentle; the hall
remains level. The exterior contains about 13,148 square metres above 0.55m of
snow relief, with deposits reaching 5.75m. The largest measured exploration
slope is 0.889, below the walking limit of 1.2. These are authored snow banks,
not runtime snow excavation.

Powder, compacted approach snow and broken frozen gravel use separate material
finishes. Distance-filtered grain and wind ridges preserve the terrain's actual
normal. Fifty-four square ground overlays were removed. Scanned-rock snow caps
now follow interpolated accumulation contours; the mountain face uses less
inflated profiles. The terrain material's winter region also extends into the
surrounding hills.

Thirty-five snow-covered pines and thirty-four saplings form the exterior
canopy and understory. Winter copies retain the existing photographed needle
silhouettes, UVs, normal maps and alpha cutouts. Near source branch geometry
avoids the large triangular artifacts found in the previous mid-distance pine
mesh. Separate narrow trunk collision lets the player pass under branches.

A roughly 35m-high frozen cascade stands to the right of the gate, at local
(63,20). Its irregular ice curtain, tapered hanging icicles and frozen lobes
join a terrain-level plunge basin centered at (63,32), with radii 13m by 10m.
Ice uses opaque PBR materials and fixed fractures. It has no volume refraction,
flow, thaw or breakage simulation. Snow prints and powder are suppressed on the
central frozen basin. Both the cascade and trees are editable Studio props.

The local source archives are `snow_01_1k.gltf.zip`, `snow_02_1k.zip`, and
`brown_mud_rocks_01_1k.zip`; their Poly Haven asset pages and CC0 provenance are
recorded in `snow-upgrade/materials.json` and the generated
`assets/purchased/dwarven_stronghold/WINTER_MATERIALS.json`. No remote asset
download was needed for this revision.

## Cleanup

`cleanup_duplicate_models.py` fully SHA256-verified equal-content candidates and
replaced 7,435 duplicate model copies with hard links, preserving every path.
That initially reclaimed 41,426,345,984 physical bytes. A second audit rehashed
1,167 canonical files and checked all 7,435 linked paths, with no mismatches.
Afterward, 132 editable working models were separated again so future edits
cannot change older packages. All 212 editable model paths in the audit are
independent. The final 7,303 redundant copies share storage inside saved builds
and documentation; net duplicate recovery is 40,825,237,504 bytes.

Failed/oversized temporary cook outputs and extracted architecture image copies
accounted for another 1,346,056,492 bytes removed. Twelve unused generated
Broddi poses accounted for 156,523,128 bytes. Together these operations reclaimed
39.42 GiB, excluding files the user removed independently. Original download
archives, saves, and nonduplicate source models were retained. The cleanup tool
now excludes editable project/source models by default. To edit a shared saved
package model independently, write a replacement file atomically rather than
modifying the shared inode in place.

The exact operations are recorded in `duplicate-model-cleanup.json`,
`cleanup-verification.json`, `temporary-cleanup.json`, and
`unused-generated-cleanup.json`, `editable-model-independence.json`, and
`cleanup-final-verification.json`.

## Rebuild

From `new_engine`:

```sh
python3 tools/build_dwarf_stronghold.py
make -j2 TARGET=rpg_grandstone
python3 tools/check_dwarf_stronghold.py --output /tmp/grandstone-views
python3 tools/check_dwarf_stronghold.py --walk --output /tmp/grandstone-walk
```

Rebuilding replaces the generated stronghold Studio document; preserve any
manual editor work first. Source cooking uses Blender with
`tools/cook_stronghold_sources.py`, followed by
`tools/compact_stronghold_poses.py`. It processes one FBX at a time in a temporary
directory. `fix_stronghold_uvs.py` is an idempotent migration for older generated
files and is unnecessary for newly cooked models.

## Validation

The five final viewpoint captures in `native-final-v2` passed on the native AMD
Radeon RX 5300 at 1600 by 900 with render scale 1.0, automatic quality disabled,
and snowfall enabled. Each view checked all 20 residents, 48 pose assets,
zero actor/snow/weather failures, zero final GL errors, and zero retained texture
references after teardown. Source documents and player saves remained unchanged.
These short still-view runs held the 60 Hz display cap; sustained walking is
reported separately in the route evidence.

The 9,000-frame run in `walk-final-v2` traversed all 413.898 metres with zero
collision/slope blocks, zero sliding, and zero return error. It produced 342
prints (171 per foot), 2,052 powder particles, and 342 audio step triggers.
Across 8,880 measured frames at 1600 by 900 and a fixed 0.7 render scale, the
average was 59.95 FPS, the 1% low was 51.12 FPS, and the worst frame took 30.749 ms.
Twenty-one frames exceeded the 45 FPS budget, so a strict 45 FPS floor is still
not achieved. This run measured the complete route plus a stationary tail.

`snow-display-final` repeated the complete route after the final footprint shade
adjustment and captured 18 powder particles still active just after returning.
It also passed with 342 prints, 2,052 puffs, 342 audio events, and no blocks.
Its average was 59.95 FPS, 1% low 52.11 FPS, and worst frame 31.404 ms. Peak
aggregate device VRAM usage was about 2,774 MiB, including the desktop and other
processes; this is not a per-process game allocation figure. Graphics settings
remain available from pause and the default launcher retains automatic quality.

`snow-regression.log` records a native GL test of stationary, airborne, dry-hall,
and teleport rejection, alternating feet, fixed-capacity reuse, expiry, and clean
teardown. Normal exploration and the editor share the existing single-instance
launcher lock. Both launchers passed syntax and duplicate-session checks.
The final content audit rehashed 1,299 distinct files across 8,602 model paths,
with no changed content or missing paths. `frozen-inputs.json` records the exact
binary, runtime models/materials/shaders, Studio data, launchers, and relevant
source inputs; all three final native runs used that binary.

Original source recordings/facial acting were not added. Dwarf dialogue,
quests, smithing transactions, deeper mountain rooms, and final cinematic art
polish remain future work. This area should be reviewed as an exploration pass.

## Dwarf material correction

The initial checks above missed invalid split normals in the imported dwarves.
The textures were embedded, but Blender 4.0 triangulation had left the exported
normal vectors at zero, causing washed-out reflection shading. The source cook
now explicitly computes evaluated split normals, preserves hard edges and
smooth faces, handles mirrored source winding, and rejects missing normal
streams. All 48 poses were rebuilt and compacted again.

Bedvar's head now uses the supplied `Dwarf_01_Head` atlas. A missing body/head
atlas raises an error instead of silently using hair. The stature reference also
recognizes head materials: this source face is named `subtool1`, so the old
name-only selection could scale the body from its shoes. Detached Orme weapon
props were excluded from the resident bank; original sources are retained.

`tools/check_dwarf_materials.py` verifies every pose's unit normals, spatial
normal variation, human-scale bounds, pose budgets, and exact embedded color,
normal and ARM images. Five production-lit indoor/outdoor close-ups passed on
the RX 5300, followed by an animated gate check with 20 residents, 48 pose assets,
zero actor failures, zero GL errors, and zero retained texture references.
See `dwarf-material-fix/final/results.json` and
`dwarf-material-fix/animation-final/results.json`. The close-ups were visually
inspected, including Bedvar's corrected head, stature and outdoor appearance.

`frozen-initial-inputs.json` preserves the original build evidence. The earlier
full-route frame-pacing measurements belong to that initial build; they were
not repeated for this material correction. `frozen-inputs.json` now records the
corrected assets and executable used for the current material/animation checks.

## Winter revision verification

The winter inputs are preserved in `frozen-winter-inputs.json` (version 3). The previous
material revision remains in `frozen-material-inputs.json`. All 48 corrected
dwarf poses retain their previous hashes. The original game binary, castle and
jungle document, graphics preferences and player saves retain their protected
hashes.

Nine final native captures passed at 1600×900, render scale 1.0 and automatic
quality disabled: approach, gate, drifts, snow detail, waterfall, trees, hall,
forge and throne. Each checked 20 residents, 48 pose assets, zero actor/snow/
weather failures, zero final GL errors and zero retained texture references.
Screenshots and profiles are in `snow-upgrade/final-captures`.

Actual capsule walking, terrain slope tests and collision remained enabled:

| Rehearsal | Distance | Result | Snow effects |
| --- | ---: | --- | --- |
| Off-path banks | 182.53m | Complete, no blocked/sliding steps | 344 prints, 2,064 powder puffs |
| Frozen plunge basin | 25.93m | Complete, no blocked/sliding steps | Correctly no prints or powder |
| Gate and hall return | 413.90m | Complete, no blocked/sliding steps | 342 prints, 2,052 powder puffs |

Walking profiles use render scale 0.7, automatic quality disabled, at a 1600×900
output. They averaged 59.95 FPS. One-percent lows were 42.35 FPS on the banks,
57.60 on the ice and 48.46 on the hall route. The bank and hall runs had 18 and
21 frames respectively beyond the 45-FPS frame budget. This does not meet a
strict 45-FPS minimum. Full results and desktop-inclusive VRAM samples are in
`snow-upgrade/verification-summary.json`. Still captures are short validation
runs, not sustained full-resolution gameplay benchmarks.

The standalone snow regression also covers stationary/air/dry/teleport
rejection, frozen-basin rejection, alternating feet, capacity and expiry. The
terrain check validates matching tile borders, hall/basin flatness, approach
slope and bank relief (`snow-upgrade/terrain-check.json`).


## Entrance art rebuild (31 October)

The previous entrance revision added overlapping physical cliff faces in front of the
mountain shell. Three free CC0 Poly Haven scans retain their original UVs and
normal maps: [Rock Face 01](https://polyhaven.com/a/rock_face_01),
[Rock Face 02](https://polyhaven.com/a/rock_face_02), and
[Namaqualand Cliff 01](https://polyhaven.com/a/namaqualand_cliff_01).
Downloaded API metadata, original geometry, image files and source checksums
remain in `assets/polyhaven/stronghold_cliffs`. The largest scan is reduced
from 94,369 to 30,198 triangles using an offline edge-collapse modifier; its
source files remain intact. Rebuilds use Blender for that cached preparation.
There was no asset purchase in this pass.

The gate now has physical stone chamfers, recessed front panels and segmented
cornices, with the existing carved dwarf guardian above its opening. Near
cliff buttresses and ledges break up the previous flat rock wall. The hollow
hall and its walking clearance remain. This is still an art pass requiring
review; scan repetitions, source architecture detail and distant terrain do
not yet meet a finished AAA environment standard.

Thirty-five firs and thirty-four saplings have fuller crowns assembled from
the existing scanned trunk and photographed needle shoots. Irregular boughs
carry shaded snow pillows; separate near/mid LODs and narrow trunk collision
remain. The frozen cascade is rebuilt as joined three-dimensional flows with
staggered narrow channels, surface fractures and tapered hanging icicles.
Its opaque ice finish uses roughness, frost and bounded thickness-dependent
lighting. It does not provide refraction, flowing water, thaw or breakage.
The light keeps warm direct sun and cooler sky fill; it is saved in Studio.

Fresh boot prints use a 32 by 32 metre height field around the player, sampled
at 1024 by 1024 texels (about 3.1cm per texel). Depressions reach about 3.8cm,
with raised rims, tread, lighting normals, a bounded parallax offset and a
matching depth-buffer adjustment. This adds shallow visual depth to the
existing travel-driven prints and powder puffs. It does not excavate the
collision terrain. The 384 impressions still expire after 180 seconds;
nearby impressions are reconstructed when the field shifts, and far prints
fall outside the 32m window. Ice and dry interiors reject powder impressions.
The GPU height texture uses 2MiB; its CPU staging uses 4MiB. Empty stationary
fields are reused instead of repeatedly cleared and uploaded.

`VALWOOD_materials_winter` is an optional versioned material extension for
snow accumulation, ice response and optical thickness. CPU, direct and staged
loaders retain the same parameters, and draw return resets the values so the
finish does not leak into actors or procedural assets. Only the private
Grandstone shaders implement the finish. Parser and native GL regressions,
including upload agreement, ordinary material defaults, draw-state reset,
footprint depth/capacity/expiry and clean teardown, are recorded in
`entrance-art/material-parser.log` and `entrance-art/gl-regressions.log`.
`entrance-art/geometry-check.json` validates finite geometry, unit normals,
UVs/indices and source winding, and confirms that all 48 dwarf pose hashes
remain identical to the previous frozen winter build.

The native route and final image evidence is summarized in
`entrance-art/verification-summary.json`. Short still captures use a fixed
1.0 render scale at 1600 by 900. Walking rehearsals use a fixed 0.7 scale at
the same output size, with ordinary capsule collision and slope checks.
These are different workloads; still-view results are not a sustained
full-resolution walking benchmark. The saved launcher retains automatic
quality and the existing graphics settings menu.

`frozen-entrance-inputs.json` version 4 identifies that revision’s binary, runtime art and
source files; version 3 is preserved as `frozen-winter-inputs.json`.
`entrance-art/protected-verification.json` confirms the original executable,
castle/jungle document, original graphics preferences, jungle data and player
saves retain their protected hashes. The unchanged source dwarf rig/content
has not been replaced with a new hero or NPC pack.

### Previous entrance rehearsal measurements

All three corrected-scene rehearsals completed with zero collision/slope blocks,
zero sliding and zero return error. The ice-shore cliff was moved 3m inward
after its conservative collision box blocked the previous return point.
The same route was retained and rerun successfully.

| Route | Distance | Mean FPS | 1% low FPS | Worst ms | Frames over 45 FPS budget |
| --- | ---: | ---: | ---: | ---: | ---: |
| walk | 413.90m | 59.95 | 59.05 | 17.457 | 0 |
| drift-walk | 182.53m | 59.73 | 50.56 | 22.229 | 1 |
| ice-walk | 25.93m | 59.95 | 59.26 | 16.910 | 0 |

Walking was measured at fixed 0.7 render scale, 1600 by 900 output and
automatic quality disabled. The nine short fixed-1.0 still runs averaged
50.07 to 59.95 FPS. Full-resolution sustained walking was not benchmarked.


## Mountain architecture and setting revision (31 October)

A new original pointed gate replaces the earlier rectangular frame and imported
guardian arrangement. Individually beveled voussoirs, battered stone wings,
deep recessed hammer/anvil panels, bronze inlays, an octagonal crossed-hammer
crest, interlocking chains and two suspended amber lanterns form one consistent
facade. The portal is open geometry. Separate stone piers and wings provide
solid collision without a box across the opening. Precipitation shelter now
follows the pointed throat as well as the hall roof. The gate photo camera has
been moved back to frame the taller architecture.

The mountain shell has continuous receding, fractured rock faces behind the
scanned buttresses. Upper ledges are buried into that ridge rather than placed
against empty wall space. The right-hand face includes a recessed channel for
the existing frozen cascade. A separate pair of near/mid meshes forms distant
branching alpine ridgelines with distance haze. Those distant mountains are
visual scenery, without added traversable terrain or collision.

Large flagstones and physical dressed masonry replace the flat hall wall/floor
modules. Continuous stone backing closes the dressed-block mortar joints.
A dark rear tympanum closes the tall vault’s transition to the hall ceiling;
sheltered vault stone has its own rough, dry material. Clan banners use shaped woven cloth and embroidered marks; they are
static meshes, without a cloth simulation. Seventy dry sedge patches, forty-four
small talus clusters, six fallen logs and five stumps add foreground scale.
Solid logs and stumps are placed off the walking routes. Winter sun bearing is
+28 degrees, providing side light on the entrance and vegetation.

All thirteen new models are original geometry or derivatives of existing local
owned/CC0 assets, with bounded embedded texture sizes. This revision required
no purchase or additional download. The 48 corrected dwarf poses are retained.
The original executable, castle/jungle scene and player files are protected.

This remains an environment art iteration. Source character fidelity, distant
landscape detail and repetition in reused rock/foliage remain below a finished
AAA art standard. Dwarf dialogue, quests and smithing interactions have not
been added by this visual pass.

### Mountain revision verification

All nine final views and all three routes passed on the same executable.
Every view loaded 20 residents, four families and 48 poses, with no actor,
snow or weather failures, no GL errors and no retained texture references.
All three routes completed with zero collision/slope blocks, zero sliding
and zero return error.

| Route | Distance | Mean FPS | 1% low FPS | Worst ms | Frames over 45 FPS budget |
| --- | ---: | ---: | ---: | ---: | ---: |
| walk | 413.90m | 59.95 | 58.93 | 19.212 | 0 |
| drift-walk | 182.53m | 59.59 | 49.52 | 21.748 | 0 |
| ice-walk | 25.93m | 59.95 | 57.86 | 19.774 | 0 |

Walking used fixed 0.7 render scale, 1600×900 output and automatic quality
disabled. The nine short fixed-1.0 still runs averaged 50.07–59.95 FPS. Full-resolution sustained walking was not benchmarked.

`walk` produced 342 impressions, 2052 powder particles and 342 audio step triggers.
`drift-walk` produced 344 impressions, 2064 powder particles and 344 audio step triggers.
`ice-walk` produced 0 impressions, 0 powder particles and 0 audio step triggers.

`mountain-finish/verification-summary.json` records the exact measurements,
including desktop-inclusive VRAM peaks. `geometry-check.json` validates all
13 new models and the revised shell; every previous dwarf pose hash is
preserved. `terrain-check.json` validates all 15 terrain tiles, matching
borders, flat hall/basin and walkable approach/exploration slopes.
`protected-verification.json` records the original files remaining unchanged.

`frozen-inputs.json` version 5 records the current executable, runtime files
and sources. Version 4 is preserved in `frozen-entrance-inputs.json`.
The interrupted `verified-main-walk`/`release-captures` and pre-joint `final-*` runs and visual experiments in this
folder predate the final geometry and are not the release evidence.
