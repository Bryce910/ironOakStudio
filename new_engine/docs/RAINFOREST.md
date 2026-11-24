# Old Tanthoon rainforest

Open `../Launch_Valwood_Rainforest_Showcase.sh` for the pool, or
`../Launch_Valwood_Rainforest.sh` for the entrance. You can also choose **Old Tanthoon | Emerald
rainforest** in the world's Scenic trails panel. **Rainforest heart** opens a
second view inside the grove. **Rainforest reflecting pool** starts beside the
new raised freshwater basin. All three start on foot. Use WASD to walk, right-drag
to look, Shift to run, Space to jump and H to toggle the tools.

This is a temperate fantasy rainforest placed in Old Tanthoon's mapped woods.
Its location and ecology are a production addition compatible with the supplied
map; the original lore document does not specify this rainforest.
[Latest native images and performance evidence](performance/showcase/RESULTS.md)
document the tested build.

## Terrain and ecology API

The rainforest occupies a bounded part of **Old Tanthoon**, centered at map UV
`(0.235, 0.670)`. Its outer ellipse has UV radii `(0.065, 0.085)`, approximately
13.1 × 9.6 km across the full map. This adds an ecology overlay to the existing
region; the eight region IDs and fifteen settlement landmarks remain unchanged.

For normalized squared elliptical distance `q²`, the regional weight is
`1 − smoothstep(0.36, 1, q²)`: full strength inside 60% of the outer radius,
with a smooth ecotone to zero at its edge. Only dry Old Tanthoon contributes.
The weight also fades from zero at 120 m to full strength at 360 m from water,
protecting pond and river banks. All existing settlement foundations are outside
the footprint; the nearest settlement is more than 3 km beyond its southern
bound. The original starting area lies in a different region.

At full weight, atlas forest suitability approaches 0.94 and moisture approaches
0.98. These are bounded suitability values, not additional instance counts.
Existing renderer and streaming caps still determine actual vegetation work.
At weight zero, the previous climate values remain bit-identical. Coastlines,
settlement foundations and the original trail remain unchanged. A single new
local freshwater pool sculpts terrain inside a compact 32 × 44 m support ellipse;
its bed is excluded from generated vegetation. Connected rivers and seas retain
their original terrain and Y=0 water plane.

The additive API leaves `WorldAtlasSample`'s six-field, 24-byte layout intact:

```c
float world_atlas_rainforest_weight(double x, double z);
const WorldAtlasTrailPoint *world_atlas_rainforest_trail_points(size_t *count);
float world_atlas_rainforest_trail_mask(double x, double z);
```

Call `world_atlas_init()` before querying ecology. The weight returns a finite
value in `[0,1]`, or zero for invalid coordinates and an uninitialized atlas.
It rejects points outside the ellipse before reading the four macro shore
samples; it performs no allocation, noise evaluation or landmark traversal.
The two fine shoreline-offset patches are outside this footprint, so these
macro shore reads match the effective shore here.

The six-point local walk follows these east-X / south-Z offsets in meters from
the biome center:

```text
(-15, 95), (2, 60), (-8, 20), (5, -25), (-5, -65), (12, -110)
```

It is approximately **216.316 m** long and follows the existing terrain. Its
mask is one within 1.6 m of the polyline and fades to zero by 2.8 m. The same mask
is merged into `world_atlas_landscape_mask().trail` and the combined landscape
sample, including outside the three older material districts. Cover can reuse
that existing trail value. A fixed bounding rectangle rejects distant points
before the five segment tests. There is no new per-frame terrain bake or
unbounded search. The viewer owns the spawn and rendering of this path.

## CPU verification

From the engine root:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror -I. \
  tests/world_rainforest_test.c engine/world_atlas.c -lm \
  -o /tmp/valwood-world-rainforest-test
/tmp/valwood-world-rainforest-test
```

The regression uses hashes captured before this overlay: 8,385 global terrain
samples retain their height, region, shore and land-transition bits; all fields
at 8,254 excluded samples and fifteen settlement centers also remain identical.
Dense probes check water and bank exclusion, core and edge continuity, finite
deterministic weights and the climate blend. A separate 440-station path check
verifies the clear width, merged trail mask, dry ground and unchanged terrain;
its maximum sampled half-meter grade is approximately 0.0392. Initialization and
rebuilding are also checked. These CPU checks do not establish visual quality,
complete collision coverage or a guaranteed frame rate; native walk-throughs
and frame profiles remain separate validation.

## Authored canopy walk

The regional overlay spans approximately 13.1 × 9.6 km. A **210 × 235 m**
inspection pocket receives detailed authored dressing around the 216.316 m
path. Walking to its northern end and back covers **432.633 m**; this is an
out-and-back route. The wider biome uses bounded procedural ecology.

The pocket contains **972 authored records**. Its original 350-record canopy,
ferns, rocks, logs and flowers are joined by **622 root, stump, shrub and sorrel
placements**, grouped around the pool and the arrival path. The preserved two
older scenery files contain 1,639 records, bringing the merged world to **2,611**
within a bounded **4,096-record cap**. Five new model aliases use ten near/mid
GLBs cooked from four newly sourced Poly Haven asset families. The ten files
occupy 37.51 MiB encoded, with 54.00 MiB of unique decoded RGBA8/mip image payload.
Near/mid and the two shrub variants share identical embedded image bytes.

Original source geometry, UVs, photographic PBR textures and shrub alpha masks
are retained. Model LODs, image tiers, source hashes and CC0 provenance are in the
[showcase asset manifest](../projects/valwood_world/models/showcase_manifest.json).
The dedicated [asset cooker](../tools/build_rainforest_showcase_assets.py) does
not fetch or cook anything during play. New prop LODs overlap over an 8 m band,
with complementary temporal coverage and a stable shadow partition. The raised
water mirror has its own visibility union, so reflected-only props are retained.

Within the regional core, the ordinary procedural tree field selects 50%
broadleaf, 40% fir and 10% young pine from stable root seeds. Its transition
uses each root's biome weight. The existing distant horizon field continues to
use pine/fir cards. Forest-floor ecology reduces meadow grass and spends its
unchanged 360-fern budget closer to the viewer. Existing root, LOD and staging
caps remain in place.

Humid lighting uses a locally cloudier sky, a restrained distant haze and a
separate terrain-following mist layer. Sunlight scattering uses the existing
shadow map. Rain falls in world space, tilts with the same wind used by foliage,
and produces short surface splashes. Leaves and ground use bounded wet-material
responses; water gains wind waves and filtered rain rings. **Light and atmosphere**
contains hour, cloud, rain, wind and mist controls. Zero rain retains mist; set
both rain and mist to zero to bypass their render pass. Foliage retains the
ordinary ambient breeze when the extra weather wind is zero.

The [weather module](WEATHER_RENDERING.md) uses eight half-resolution fog steps,
4,096 rain and 768 splash instance caps, and a 65² surface cache staged at at most
520 surface requests per frame. Its HDR/fog/cache image payload at 1600×900 is
13.75 MiB. It runs after contact occlusion and before temporal AA. No per-frame
particle allocations or CPU/GPU readbacks are used in production.

Sun shadows use a 2048² depth map and nine compare-before-filter depth taps
with continuous B-spline weights. This avoids interpolating raw depths before
comparison and removes square leaf-shadow edges. Dedicated mid meshes supply
shadows for four procedural near-tree groups and six authored near tree/shrub
groups. Their images are shared through the existing cache, their instance
batches match the detailed color models, and their world-space wind is identical.
The main view retains its original high-detail meshes. This is a shadow LOD
approximation; it does not reproduce every near leaf triangle exactly.

The pool is centered at global XZ `(-26734, 9615.496705929329)`, at **Y=134 m**.
Its conservative water cap is 20 × 32 m; an irregular bed and dry raised shelf
hide the cap edge. A six-meter bank feather joins the original height field.
The maximum center depth is 0.95 m. Collision, terrain tiles, ecology exclusion
and rain support use the same atlas height. The pool uses a half-resolution
planar reflection at its own elevation, depth transmission and shoreline foam;
reflected terrain and model geometry clip below that water plane. The existing
216.316 m walk lies outside the sculpted support.

```c
const WorldAtlasPool *world_atlas_rainforest_pool(void);
bool world_atlas_local_water(double x, double z, float *surface_y, float *depth);
```

`local_water` returns true only over actual submerged terrain, zeros nullable
outputs on false, and rejects non-finite/uninitialized inputs. The additive pool
API preserves the 24-byte `WorldAtlasSample` ABI. The focused pool regression
checks 72 radial shore sections, continuity and dry banks, preserves 10,232
surrounding half-meter height probes bit-for-bit and verifies that the existing
path stays outside its support.

This remains a prototype rather than finished AAA production art. The detailed
pocket covers only part of the much larger biome; distant horizon species,
repeated foliage silhouettes and terrain microgeometry still need further work.
The latest report distinguishes visual evidence from measured frame budgets.

Edit [rainforest_layout.json](../projects/valwood_world/scenery/rainforest_layout.json)
or individual [placement records](../projects/valwood_world/scenery/rainforest_instances.txt).
The [placement manifest](../projects/valwood_world/scenery/rainforest_manifest.json)
retains poses, real bounds and clearances; the
[overhead plan](../projects/valwood_world/scenery/rainforest_layout.svg) shows the
actual placements. From the engine root:

```sh
python3 tools/build_rainforest_area.py --validate-only
python3 tools/build_rainforest_area.py --force
```

The force command regenerates this pocket. Default operation preserves an
existing output; neither mode rewrites the original starting-area source.
