> Historical pass: current counts, renderer policy and final native results are in [castle-demo-ready](../castle-demo-ready/README.md). The221-placement /7,463-instance counts and castle blocker below describe the earlier build.

# Castle, tavern and Old Tanthoon demo polish

This pass uses assets already in the project. Launch the normal walking demo
with `Launch_Valwood_Demo.sh`, or start beside the jungle fire with
`Launch_Valwood_Jungle_Camp.sh` in the parent folder.

WASD walks, hold the right mouse button to look, Shift sprints, F9 advances
through the nine showcase destinations, and F10 opens graphics settings.
The tour visits the arcade, west street, market, tavern, creek garden, purchased
gardens, jungle path, travelers' camp, and jungle pool before returning.

## Authored additions

- 221 additional placements in an additive ninth source: delivery barrels,
  sacks, buckets and wall herbs; table-supported tankards and food; two tavern
  wall lamps; four framing trees; 147 clustered low plants and eight root/stone
  anchors; a fire ring, split logs, two benches, shelter and supplies.
- Four camp residents: two resting travelers, a standing wayfarer and a smoothly
  walking craftsman. Two retained WildMesh goats walk separate grazing loops.
  The existing village population remains 140; the tavern has a moving bartender
  and a different host appearance. End-of-route pauses smooth the bartender's
  turns.
- A compact elongated terrain terrace supports the camp furnishings and leaves
  the older western plant beds intact. A 117.153 m return walk links the main
  jungle trail and the clearing, routing around existing rock outcrops. A layout
  check covers 714 solid/segment pairs with at least 2.216 m centerline clearance;
  the native collision audit also checks three parallel walking lanes. Terrain,
  collision, rain and actor grounding use the same production atlas surface.
- The camp has purchased flame art, smoke, embers, warm point lighting and
  positional fire ambience. The tavern gains two wall practicals and a bounded
  diffuse return from its four actual lights. Jungle clouds, subdued sunlight,
  rain, mist and fog remain enabled, with slightly more readable ground bounce.

The original eight scenery instance files are preserved byte for byte. The
renderer now loads nine sources / 7,463 authored instances. The polish layer
adds no model filenames or texture images. Existing shared materials, LODs,
frustum/distance culling and texture blocks remain active.

## Seated character texture repair

Visual review exposed a pre-existing importer error: the legacy character's
Body material needs the case-sensitive `UVmap` layer; Head and Hair need
`UVMap`. The old importer used the latter everywhere, placing 96.62% of body
corners on blue texture padding. Reimporting all four genuine sitting keys
reduces that to 3.68% seam corners and restores the original fabric and skin.

An independent comparison verifies unchanged positions, normals, source-corner
IDs, indices, Head/Hair UVs and all three embedded texture images in each key.
Only the Body texture coordinates change. This adds no geometry, images or
runtime shading passes. Original purchase/source files remain unchanged.
See `seated-uv-parity.json`; `seated-uv-before/` retains the earlier cooked poses.

## Bounded runtime work

Native diagnostics exposed heavy system paging with the castle open. Exact
texture-sharing comparison keys accounted for approximately 406 MiB of private
engine RAM. They now live in one unlinked, read-only mapped temporary file,
bounded by the same 768 MiB logical budget plus at most one padding page per
entry. Released intervals are reused. Loading boundaries finish writes and
discard the comparison pages from the working set; ordinary rendering never
reads them. New asset loads still compare full bytes, including forced hash
collisions. File-storage failures retain the exact original heap fallback.

This changes neither GPU textures nor their resolution, formats, mip chains,
samplers or sharing. Tests cover source-page trimming, five backing failure
paths, 1,100 reused intervals, actual GL pixels and cross-loader ownership.
The instrumented engine resident set fell from approximately 724 MiB to
316 MiB. This reduces private RAM pressure; it does not resolve the separate
castle GPU-residency problem or establish a castle FPS improvement.
The original stalled run is retained under `native-final/`, with its interruption
record; it does not establish the performance target. `castle-pressure/` retains
the separate instrumented paging diagnosis.

Camp pose geometry loads only behind startup/destination staging, with 260/300 m
residency hysteresis. Twenty retained poses use 60 independent main, reflection
and shadow batches; the CPU update and draw paths allocate no heap storage.
The existing camper shader fade reaches zero at 122 m; CPU submission stops at
123 m, retaining a one-metre guard. Fully hidden passes skip uploads and uniform
work. Returning to view reconciles every pose batch before drawing, including
clearing old batches. Wildlife uses its existing bank residency.

The global fire source limit increases from 32 to 40 for 35 actual fire sockets;
nearest lighting remains eight sources and audio remains twelve voices. The
existing particle budget is unchanged: fourteen emitters (eight smoke, six dust)
share the same fixed buffers and render targets. No additional shadow pass,
texture-resolution cut or automatic quality reduction is introduced.

## Verification

The final regression log covers camp failure cleanup and 600 simulated seconds,
rainforest terrain, pool support, wildlife support, torch/model/light paths,
and exact resource ownership. Actual 80-pose population GL tests pass after the
UV repair. The native camp audit samples two simulated minutes before measured
play, checking actual terrain/collision support, continuous walking and
person/animal separation.

Native captures are in `visuals-final/`. Timing evidence and the final measured
results are recorded below. The current stationary camp capture is
`visual-final-camp-current/camp.png`. Timing captures use sweep/route cameras and
can face scenery rather than the camp. Captures themselves make no FPS claim.

## Measured readiness — 27 October 2026

The authored polish and movement/resource integration checks pass. **The full
demo is not yet signed off for 45–60 FPS.** Final native tests on the RX 5300 at
the unchanged saved quality produced:

| Test | Mean FPS | Slowest 1% FPS | Frames slower than 22.222 ms | Worst frame |
| --- | ---: | ---: | ---: | ---: |
| Camp clearing | 59.52 | 52.46 | 1 / 1,680 | 25.547 ms |
| Full 117.153 m camp return walk | 58.30 | 47.32 | 1 / 2,160 | 25.017 ms |
| Full 432.633 m jungle return, first run | 55.63 | 45.23 | 20 / 7,560 | 24.695 ms |
| Full 432.633 m jungle return, repeat | 55.63 | 45.03 | 23 / 7,560 | 26.594 ms |

These tests meet the mean-and-slowest-1% gate. They do **not** meet an every-frame
45 FPS floor. All measured intervals are retained; none fell below 30 FPS.
Both routes complete without blocked, sliding or slope-rejected steps, with zero
return-position error and no loading holds during walking. The camp additionally
passes 3,600 samples of camper/animal support and separation over two simulated
minutes before measured play. See `native-cull-final/` and `final-readiness.json`.

The nine-stop F9 integration run completes, returns to the castle, retains both
lighting-probe regions, and releases all texture references cleanly. Ten material
preparations / forty headings occur at startup or destinations, with no walking
fences, GL errors, failed tiles or actor support failures. This tour is **untimed**;
its loading intervals and successful scene transitions do not establish FPS.
Automated runs use a dummy audio device, so they verify mixing/readiness and
distance-based footsteps rather than audible playback. See `tour-final/`.

Castle performance remains a blocker. The latest 600-frame instrumented castle
diagnostic measured 14.71 mean FPS / 1.40 slowest-1% FPS under the current desktop
load. It is a short diagnostic, not a completed street/tavern acceptance route.
Approximately 558 MiB of the engine's requested graphics residency was displaced
to system memory, while the desktop was swapping and reading hundreds of MB/s.
The earlier full castle walk was interrupted after severe presentation stalls;
its partial raw samples and interruption record are retained under `native-final/`.

Steam, Discord and the user's other applications have been left running.
Permission to close Steam and Discord temporarily was requested because it would
interrupt their sessions. A clean castle/arcade/tavern acceptance run remains
pending that response; closing them is a diagnostic step and does not guarantee
the target. If paging persists, the renderer still needs a smaller regional GPU
working set. No normal-map or ARM-map compression was enabled to hide the issue:
the experimental offline encoder failed its quality guard and the production
data textures remain unchanged.

## Reproduce native timing

Close the interactive demo first. Run one rendering job at a time from
`new_engine`, with no concurrent build or asset cook:

```bash
python3 tools/check_demo_finish.py --native --frame-times-only \
  --runs route arcade_route tavern --frames 1800 \
  --output docs/performance/demo-polish/recheck-castle
python3 tools/check_demo_polish_camp.py --native \
  --output docs/performance/demo-polish/recheck-camp
python3 tools/check_rainforest_performance.py --native --frame-times-only \
  --output docs/performance/demo-polish/recheck-jungle
```

These use saved Custom/manual 60 FPS settings at 1600×900 output / 1360×765
scene resolution, 2K shadows, half-resolution reflections, contact shadows,
volumetric clouds and fog. Every interval after exactly 120 warmup frames is
retained and independently recomputed, including hitches. Detailed GPU-stage
query rings are omitted to match ordinary manual-quality play. Mean and
slowest-1% FPS gates are reported separately from the stricter every-frame
22.222 ms budget. No settings are saved by the acceptance tools.
