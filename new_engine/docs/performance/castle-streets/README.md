# Valdoa western streets and Copper Lantern tavern

The expansion adds 274 placements to the existing world: 26 house frontages,
connected roads and residential lanes, a tavern square, the Copper Lantern
tavern, and a sheltered residents' courtyard. The five earlier scenery files
are unchanged. All 508 earlier model files match the preceding bandit-camp
source freeze byte for byte. Eight compact middle-block frontages and continuous
paved sidewalks bring the streets closer to the houses. A separate full-source
SAT check covers 3,939 house/structure/prop pairs, with no intersections.

The tavern is an enterable 20×16 m building with actual front and rear openings,
windows, exposed timber framing, a counter and staff aisle, shelves, casks,
three tables, six benches, stools, mugs, a hearth and a hanging lamp. New
furniture and shelter models reuse the existing licensed village PBR images;
they add no new image-cache policies. The character bank adds a bounded 16 MiB
diffuse/normal/AO atlas.

Twelve people are appended to the original 128 residents: six tavern staff and
patrons, four sheltered neighbors, and two western-street walkers. Four use a
genuine seated source animation. They retain deterministic GPU pose blending;
this expansion does not introduce a dialogue system or simulated economy.

Ground markings merge into the existing cached village paving/grass mask.
Terrain height is unchanged. Four new practical lights fill the existing
32-emitter flame budget; at most eight nearby point lights are evaluated.

## Open and explore

Run `/home/computer/Desktop/final_engine/Launch_Valwood_Castle_Streets.sh`.
The saved graphics profile is read normally. WASD walks, right mouse looks,
Shift runs, H opens tools, Tab opens the map, and F10 opens graphics settings.

Five direct views are available through `Launch_Valwood_World.sh --scenic`:
`castle-west-street`, `castle-tavern`, `castle-tavern-interior`,
`castle-shelter`, and `castle-alleys`.

## Validation

- `build-final-density.log` and `project-cpu.log`: final executable build and scenic argument checks.
- `scene-cpu.log`: actual six-source production colliders, 25,740 capsule
  sweeps, and 13,770 rotated model-footprint mask probes.
- `population-cpu.log` and `population-asan.log`: ten minutes at 60 updates per
  second, all-pair crowd checks, unchanged original 128 identities/routes/poses,
  no hot allocations, and exact ownership cleanup. ASan/UBSan ran with leak
  sanitizer disabled because its shutdown is unsupported under container ptrace.
- `seated-asset-fit.log`: actual articulated source bodies, cyclic pose
  compatibility, bench support and complete hand/clothing clearance at tables.
- `population-gl.log`: all six scenery files and 80 actual poses; source image
  sharing, color/shadow pose alignment and zero retained owners in software GL.
- `receiving-domain-cpu.json`: 1,684 actual-geometry probes confirm full light
  receiving weight on the tavern floor, inner walls and roof underside, while
  excluding its exterior wall faces.
- `pets-crowd-cpu.log`: the actual 140 residents and 12 pets remain separated.
- `original-models-preserved.json` and `baseline/`: prior model/source/settings proofs.

The native benchmark helper is `tools/check_castle_streets.py`. It profiles
daylight streets, the tavern at dusk, the complete 585.072 m street/tavern walk,
and the existing market. It preserves the saved Custom profile: 1600×900
display, fixed 85% internal scale, 2K shadows, half-resolution reflections,
contact shadows, volumetric fog and clouds. No builds, asset cooks or full
hashes run during the measured GPU jobs. Every interval after the declared
120-frame warmup is retained. Separate untimed captures carry no FPS claim.

## Interior rendering optimization

The first native batch is preserved in `native/`. Its dusk case averaged
58.76 FPS but missed the 45 FPS low-frame-rate gate (43.03 FPS 1% low).
The renderer now draws the opaque tavern once before the terrain while the
camera is inside its conservative interior volume, then excludes it from the
later main scenery draw. Hidden outside fragments can fail the depth test
earlier. Outdoor cameras, shadow casters and reflections keep their previous
paths. `forest_gpu_ms` includes the early draw, so its cost is still counted.

`early-pixels/results.json` compares fixed-time native interior/exterior
captures with this optimization enabled and disabled: zero changed RGBA pixels
in both pairs, with no tolerance. `draw-filter-cpu.log` checks exact group/LOD
partition, state isolation and allocation-free repeated draws. Final timing
reports are under `native-optimized/`; the dusk case passed at 59.95 FPS mean,
58.63 FPS 1% low, and 20.345 ms maximum measured frame time.

The subsequent market regression still showed a short presentation stall, so
the final build also caches conservative source-pose bounds for each committed
NPC batch. Entirely invisible batches are rejected against the actual main or
reflection matrix. Partially visible batches draw in full; shadow casters,
animation updates, resident counts and source meshes are preserved. Invalid
matrices fall back to the original draw path.

`population-frustum-cpu.log` covers 1,728 rotated pose/clip-edge/reflection
witnesses, failed-upload bounds, the original resident fingerprint, the full
600-second crowd regression, and zero hot allocations or bounds queries.
`population-frustum-gl.log` compares twelve actual-model main/reflection views
bit for bit in color and depth. `population-frustum-pixels-final/` confirms
zero changed native screenshot pixels in the tavern and market. The initial
`population-frustum-pixels/` reference stopped because the validator incorrectly
required the optimization to be enabled in the deliberately disabled reference;
that harness condition was corrected before the complete final comparison.

The first acceptance run for both optimizations is `native-final/`. Earlier
timing and failed/diagnostic runs are retained separately, with their original
samples. The additional water-readiness results below supersede its route
stutter result.

Initial visual captures are retained under `initial-day/` and `initial-dusk/`.
The first day-capture validator incorrectly expected the wilderness hour 17.05;
the town's existing `WorldMarketHeroLook` correctly used 16.05. The helper now
checks 16.05 for these village scenes. The early day images also motivated the
closer frontage layout; `final-street/` shows the revised placement. No early
capture provides a performance claim.

## First creek visibility

Three independent route runs reached the first visible creek at frame 392.
The last pre-fix run measured 93.718 ms there, with 91.567 ms of render CPU
time. That frame also first allocated the reflection target, reflected sky
target and opaque-water color/depth snapshot. The delayed GPU query alone
does not identify which driver operation accounted for every millisecond.

Nearby castle starts now prepare those same resources during ordinary loading,
independent of camera heading. Startup performs the ordinary reflected sky draw
and water blit and completes their first use before the walking clock starts.
The real reflection and water snapshot still overwrite those images before
sampling. Remote inland starts retain lazy resources, and the existing water
visibility test, settings and quality remain unchanged. There is no added
walking-loop synchronization or increased benchmark warmup.

`water-prewarm-review.md` documents state/lifecycle review, geographic margins,
and the corrected matching float radius. `creek_water_preparation` in each
world report records startup readiness, first reflection frame, and allocations
during play. In `native-water-ready/`, the route still first reflects at frame
392, but takes 16.72926 ms there; all three play-allocation counters remain zero.

## Native performance and limits

The complete `native-water-ready/` batch passed correctness and the mean/1%
low 45 FPS gates on the native Radeon RX 5300. These are saved-quality runs,
not software-rendered or adaptive-resolution measurements.

| Case | Mean FPS | 1% low FPS | Maximum frame ms |
| --- | ---: | ---: | ---: |
| Daylight western streets | 59.95 | 57.38 | 19.277 |
| Dusk tavern interior | 59.95 | 57.83 | 20.049 |
| Complete street/tavern route | 59.95 | 54.41 | 32.232 |
| Original castle market | 59.95 | 53.02 | 31.683 |

All four runs retain every sample after the declared 120-frame warmup.
The street and tavern runs kept every measured frame within the 45 FPS budget.
The route and market still contain isolated slower frames: passing mean/1% low
does not guarantee every frame exceeds 45 FPS. The complete route reaches all
29 destinations, walks 585.072 m at 3.5 m/s, returns exactly, and records zero
blocked, sliding, or slope-blocked steps and no travel-loading holds.

The final source-only radius correction changes the startup gate by at most
0.000000191 m. All four measured startup positions are more than 35 m from
that boundary, so their branch choices are unchanged. The rebuilt executable's
isolated route is retained in `native-water-ready-final/`. It revealed another
95.39251 ms frame 392 despite zero play-time allocations. The earlier 16.72926
ms result therefore did not establish a reliable first-use fix; allocation and
sky preparation alone were insufficient.

## Complete draw preparation

Startup now calls the same reflection and ocean/creek drawing helpers as play,
in four cardinal directions at the original camera position. This exercises
the reflected terrain, actual model/material variants, cloud, flame and water
pipelines. Shadow depth is initialized, foliage history inputs are explicit,
and scenery selection is restored afterward. It does not advance simulation,
camera position, animation time, temporal history or jitter. The four draws
are reported as `startup_render_passes`; play counters and GPU timing queries
exclude preparation. Normal module draw totals include startup draws.

`full-water-warm-review.md` records the independent source review. The final
native batch is `native-full-water-warm/`, deliberately ordered with the full
walking route first, followed by the street, tavern and market regressions.
Its source freeze identifies the exact executable. All earlier passes,
outliers and diagnostic failures remain available under their original names.

The completed route-first batch passed correctness, preserved preferences and
all four mean/1% low gates on that same final executable:

| Final case | Mean FPS | 1% low FPS | Maximum frame ms |
| --- | ---: | ---: | ---: |
| Complete street/tavern route, run first | 59.95 | 56.19 | 28.607 |
| Daylight western streets | 59.95 | 56.72 | 25.812 |
| Dusk tavern interior | 59.95 | 57.49 | 21.332 |
| Original castle market | 59.95 | 55.38 | 23.769 |

The creek still first appears at frame 392: 16.70153 ms total and 2.14496 ms
render CPU, versus 95.39251/93.14365 ms in the isolated allocation-only test.
Nearby preparation takes 228–265 ms during loading and performs four draws;
all three water-resource allocation counters remain zero during play.
The complete walking route again reaches every waypoint, returns exactly,
and records zero blocked/sliding/slope steps or loading holds.

These are measured mean/1% low results, not an every-frame guarantee: the final
route, street and market runs contain isolated frames above 22.222 ms. Raw
CSVs preserve all of them. Peak aggregate device VRAM usage during the batch
was 2,983,821,312 bytes; this public device counter includes other applications.

Final untimed images and their exact-input/pixel checks are under
`full-water-final-captures/`. `final-verification.json` gathers the final binary,
settings, feature, preservation, CPU/GL and native evidence. The screenshots
make no FPS claim; the performance table comes from the separate timed batch.
