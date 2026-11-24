# Cinematic terrain shaping

The terrain retains the reference map's 100.8 × 56.7 km footprint, traced land,
lakes and waterways, sea-level water, fifteen dry settlement foundations, and
3.5 m/s walking speed. Walking the reference width still takes exactly eight
hours by distance/speed calibration; a continuous eight-hour navigable land route has not been validated. The authored Seldonne scene and its source assets remain separate.

## Authored landscapes

The global 1025 × 577 atlas supplies the broad Mashito backbone, Ulasse crown,
forested Valwood spurs, Old Tanthoon ridges, Wallows escarpment and Crosswind
uplands. A short height filter controls curvature before foundations are applied.
Three additional 513 × 513 local residual fields provide finer landscape shaping:

| District | Reference-image UV bounds | Local grid spacing |
|---|---|---|
| Ulasse crown/headwall | (0.290, 0.035)–(0.470, 0.235) | 35.4 × 22.2 m |
| Valwood glade/trail | (0.485, 0.220)–(0.530, 0.285) | 8.9 × 7.2 m |
| Crosswind headlands | (0.490, 0.875)–(0.580, 0.947) | 17.7 × 8.0 m |

Ulasse has independently authored uneven crown and brow spines, five rounded
drainage incisions, projecting buttresses, and broad ledges that terminate
laterally. Domain-warped rock detail uses several scales; no periodic altitude
bands are added. The highest crest is about 2,770 m. A middle summit reaches
about 2,442 m, separated by saddles at about 1,704 m and 1,753 m. Opposing local
relief checks verify a real incised channel and a separate projecting buttress.

Valwood has a slightly sloping oval clearing at the on-foot forest entrance,
with a winding northwest trail valley and raised wooded shoulders. The authored
2,282 m trail is 3.2–3.7 m wide, with a 0.65 m soft material edge. Its maximum
measured one-meter grade is 0.152. The clearing reduces forest suitability,
while its meadow, moss and rock masks support material and scenery placement.

Crosswind has four irregular rocky headland spurs, three incised coves and an
uneven raised shore platform behind the existing coastline. The broad terrain retains the reference-map land/water layout. Coastal gullies retain
a real land floor rather than collapsing into a false sea-level basin.

Two additional paired 513 × 513 fields shape the shore at walking scale.
The Ulasse review shore uses UV bounds (0.4485, 0.168)–(0.4635, 0.192),
with approximately 3.0 × 2.7 m sampling. Crosswind uses
(0.510, 0.914)–(0.540, 0.938), with approximately 5.9 × 2.7 m sampling.
Unequal gravel shelves, low bank rises, shallow runnels and terminating terraces
replace the uniform bank ramp. A second cached channel adds nonperiodic coves
and spits at roughly 30–100 m scales, bounded to an 8 m signed-shore offset.
This deliberately permits local dry/water classification changes only within
8 m of the original line inside those two rectangles. The original macro atlas,
waterway connectivity and every core point beyond that band retain their signs.
The offset fades by 48 m from the original shore, at patch edges and around
settlement foundations. It leaves the lake entrance's shoreline and height exact.

Initialization bakes the complete offset first, then material masks and bank
heights against that effective line. Runtime height, region, forest suitability,
land weight, collision queries and the underwater bed all use the same effective
signed distance. The wet bed is `-min(85, -shore_distance * .07)`; both wet and
dry branches meet continuously at Y=0. No runtime feature scans or new noise
queries are added. `world_atlas_reference_shore_distance()` exposes the unchanged
macro line for regression and authoring diagnostics; it must not replace the
effective runtime sample. No new terrain vertices or draw calls are required;
the water pass derives shallow contact from the actual opaque terrain depth.

Every residual field blends to zero around its rectangle and near settlements
and water. The twelve fixed foundations retain their authored elevations.
Brandalhall, Callieut and Rustpatch retain natural elevations of about 1,131 m,
1,030 m and 411 m, with their previous surrounding grades. Terrain shaping is
deterministic artist-authored geography with erosion-like forms; it does not
run a hydraulic erosion or river-flow simulation.

## Material and trail integration

`world_atlas_districts()` exposes the three rectangles in global meters.
`world_atlas_landscape_sample()` supplies actual height plus independent
`trail, rock, meadow, moss` weights, each in 0–1. District masks can be baked into
three RGBA texture-array layers in that order. Outside districts the weights
are zero. Height and the broad material fields require no runtime feature scans.
`world_atlas_landscape_mask()` supplies the same weights without a second height
query when the caller already has an atlas sample. Existing rock/meadow/moss
weights also grade the dry gravel-bank transition.

`world_atlas_trail()` exposes nine world-space points and their half-widths.
The exact CPU path weight evaluates eight fixed line segments, allowing grass
and tree exclusion from the actual clear walking surface. A 512² texture over
the forest district has roughly 9 m texels, so the narrow path must also use
the analytic polyline in the fragment shader or a separate high-resolution
trail representation. Sampling that texture alone cannot reliably retain the
3–4 m surface. The same polyline is used to shape its broader terrain valley.

## Review locations

All positions use normal on-foot eye height, 1.75 m above the actual terrain.

| View | UV | Yaw / pitch, radians | Ground |
|---|---|---|---:|
| Ulasse lake | (0.45400, 0.18000) | −0.90 / 0.025 | 4.05 m |
| Ulasse close shore | (0.4534948785, 0.1792875066) | −0.90 / −0.13 | 0.042 m |
| Valwood trail entrance | (0.51100, 0.25200) | −0.55 / 0.025 | 335.80 m |
| Mashito valley shoulder | (0.29760, 0.08800) | 0.58905 / 0.040 | 569.00 m |
| Crosswind headland | (0.52200, 0.92525) | 1.96350 / −0.080 | 142.40 m |

The ridge viewpoint has an open downhill ray for its first 600 m before the
opposing mountain rises. Crosswind faces down its headland toward open sea,
with water visible along the forward ray from 600 m onward. The close shore is 65 m ahead of the lake entrance, 23.04 m from the effective
water edge (16.82 m from the reference line), with a 0.003 local grade. Camera positions
must still wait for nearby detailed tiles before ending a landmark jump.

## CPU and memory budget

Measurements use an optimized C build and deterministic one-million-query
workload on this machine. These are CPU sampling costs, not game FPS claims.

| Operation | Previous shaped atlas | Authored districts | With fine shore fields |
|---|---:|---:|---:|
| Warm full random-world sample | approximately 68 ns | approximately 74–77 ns | previous height-only fields approximately 79–83 ns; paired-field measurement pending |
| Atlas initialization | approximately 1.25 s | approximately 1.58 s | previous height-only fields approximately 1.67 s; paired-field measurement pending |
| Resident atlas storage | 8.46 MiB | 14.48 MiB | 18.50 MiB |

The three eight-byte residual/material grids add 6.02 MiB; the two eight-byte paired
shore grids add 4.016 MiB (2.008 MiB for heights and 2.008 MiB for offsets). Authored feature evaluation, polygon tracing and noise
baking run only during initialization. Runtime height queries check three broad
district rectangles and two shore rectangles, with a bilinear lookup in each
applicable field. The exact path query evaluates at most eight segments.
Sampling allocates no memory and performs no file access or GL calls.
The shaping change adds no tile records, terrain vertices or draw calls.
Shader/cover integration and native rendering benchmarks establish the separate
GPU costs and visual result.

## Validation

Atlas tests cover finite bounds, unchanged map water samples, all fifteen dry
landmarks, twelve fixed foundations, natural northwest settlement grades,
coordinate round trips, continuous heights and district edges, corner ULPs,
deterministic restart, authored crag/gully relief, exact trail width and grade,
the clear forest entrance, and the eight-hour width/speed calibration.

The shore regression retains the original `0x08b1e07e55b80fe2` dry/water
classification golden over 33,153 coarse reference-map samples for both the
reference and current effective fields; it does not regenerate that golden.
All 33,089 control points beyond the original ±8 m band preserve their signs.
An additional 33,282 local patch samples verify the 8 m offset bound, dry/wet
height agreement, exact underwater-bed formula and ocean ecology. They detect
89 actual local classification changes, all within the approved band; sampled
offsets range from −7.815 to +7.611 m. Water/field-edge continuity, one-ULP
corners and deterministic full cache reinitialization are checked.

The lake entrance remains exactly 4.054718 m high with signed shore 72.878387 m.
Equal-distance bank samples 30 m from the effective water edge range from
0.423 to 2.607 m; the first 80 m toward water retain a maximum one-meter grade
of 0.104. The prior uniform-bank source fails the new bank-relief regression.
These are geometric checks; native images establish the combined visual result.

The LOD test obtains actual generated mesh/skirt data through the tile API.
For both 32-cell and 64-cell meshes it verifies every vertex, normal, index and
height bound; every possible LOD 0–3 interface through the authored districts;
and every possible LOD 4–6 interface across the full atlas plus an ocean guard.

| Interface | 32-cell maximum gap | 64-cell maximum gap | Covering skirt |
|---|---:|---:|---:|
| LOD 0 / 1, authored districts | 1.699 m | 0.717 m | 24 m |
| LOD 1 / 2, authored districts | 3.312 m | 1.511 m | 24 m |
| LOD 2 / 3, authored districts | 5.124 m | 3.312 m | 24 m |
| LOD 3 / 4, authored districts | 13.413 m | 5.124 m | 24 m |
| LOD 4 / 5, full atlas | 22.489 m | 17.897 m | 24 m |
| LOD 5 / 6, full atlas | 56.681 m | 22.489 m | 96 m |

Release tests and Address/Undefined Behavior Sanitizer runs pass. LeakSanitizer
is disabled because the execution environment uses ptrace. The tests exercise
idempotent initialization, shutdown and complete reinitialization. These checks
establish geometric coverage for the tested interfaces; native captures remain
the authority for composition, material appearance and rendering performance.
