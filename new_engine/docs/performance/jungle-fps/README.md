# Jungle FPS — latest measured results

The [latest two complete native walks](final-unmonitored/results.json) pass
source, graphics-preference, route, scene, and GL/resource-cleanup checks while
retaining the approved cloudy sky, dark canopy, mist, lighting, and scenery.
They retain **15,120 measured intervals** with seven frames over the 45 FPS
budget. Both pass the average/slowest-one-percent gate, but **the stricter
all-frame 45 FPS goal is not met**.

| Latest clean walk | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| [Walk 1](final-unmonitored/walk-1/results.json) | 57.47 | 47.20 | 28.947 ms | 6 |
| [Walk 2](final-unmonitored/walk-2/results.json) | 57.64 | 48.84 | 23.720 ms | 1 |

Both 7,680-frame runs keep all 7,560 post-warmup intervals, complete the unchanged
432.632946-metre return route, and preserve saved quality settings. Their
endpoint BMPs match exactly: zero changed pixels and zero maximum channel
difference. The expensive external GPU/DRM resource monitor is disabled. GPU
stage queries are explicitly unavailable in these frame-only measurements;
zero stage fields mean unavailable, not zero GPU cost. UI tracing remains
enabled with fixed-memory collection and shutdown-only output. Vsync remains
unchanged at swap interval one.

The second walk's sole slow interval, frame 2303, includes approximately
8.274 ms of update wall time, 0.253 ms of render CPU wall time, and a preceding
15.174 ms swap. It therefore cannot be explained solely as GPU work or an
empty-UI backend stall. These wall intervals do not establish an exclusive
driver, scheduling, or resource cause. Earlier failed candidates and diagnostics
are preserved below rather than replaced by the better repeat.

## Image-preserving changes

- **Earlier cached species filtering.** `Dressing::select` rejects other tree
  species before calculating their distances. Tree selection no longer computes
  an unused root hash. Accepted roots retain the same distance arithmetic,
  traversal order, visibility tests, caps, and uploaded instance data.
- **Exact horizon membership reuse.** Each horizon bucket compares selected
  source-index membership within the same field publication. A distance-ranking
  permutation of the same set reuses the committed GPU buffer and its exact
  root order. Changed membership or publication requires an upload; membership
  becomes authoritative only after a successful upload. Temporary probe buffers
  are restored from the committed instance data. Each bucket also retains the
  actual anchor of its GPU payload. A failed upload retains the previous buffer
  and its anchor, and retries selection rather than moving old instances to a
  newly published origin. A failed probe restore invalidates selection so it
  can recover.
- **Conservative horizon spatial grid.** A 512-metre grid rejects whole cells
  only when bounds prove every root fails the existing distance/frustum
  predicate, including the pool's mirrored view. Retained roots still enter
  that exact predicate in their original source order. Nearest-root selection,
  tie behavior, bucket caps, and uploaded instance bytes remain unchanged.
  Invalid or unavailable grid data uses the original full scan. Index storage
  is allocated once; publication and selection do not allocate.
- **Conservative pool reflection scissor.** The full reflection sky remains
  rendered. Geometry is restricted only to a conservative rectangle containing
  every pool reflection sample, including the shader's ±0.009 UV distortion
  and maximum five-texel blur. Each axis expands by
  `ceil(0.009 * texture_extent) + 8` texels; the eight texels include that blur
  plus filtering/raster guards. Near-plane intersections and
  uncertain inputs retain the full viewport. Reflection texture resolution and
  the shading of sampled texels remain unchanged; scissor is disabled before
  returning to the main view.
- **Exact shadow-tile rejection.** Terrain tiles are tested against the actual
  directional shadow projection with a four-metre guard on their bounds. Only
  tiles proved outside that volume are omitted.
- **PCF work beyond its existing fade endpoint.** Terrain and foliage skip
  shadow-map samples at horizontal distances of at least 160 metres, where the
  prior result was already exactly one. The active shadow response and existing
  125–160 metre fade remain unchanged, including both nine- and four-tap modes.
- **Existing zero-coverage horizon cards.** The color vertex shader clips a
  horizon card only below a conservative 2.99 projected pixels, where the
  existing fragment shader already has zero coverage below three pixels.
  Positive coverage is retained. Invalid inputs fail open, and the shadow
  program explicitly keeps this shortcut disabled.
- **Wildlife material preparation during loading.** Resident animal poses are
  exercised through the actual two-attachment main target, one-attachment
  reflection target, and shadow target before measured walking. Four headings
  cover each target. The rainforest contract is 12 preparation calls and 144
  pose draws, with zero failures and no walking fences. Temporary submissions
  are cleared and normal actor state, residency, animation, and draw counters
  remain separate. This addresses a first-use candidate; the new complete walk
  must establish any frame-time benefit.
- **Exact procedural grass classification cache.** Cache the original seeded
  procedural/tuft classification by root structure revision. Fade progress does
  not change root structure; camera visibility and per-root fades still update.
- **Single-pass Dressing dispatch.** Route each root to its existing species
  and LOD groups rather than repeating the same source-field scan per group.
  Point predicates, source order, distance/cap behavior, and the old scan as the
  fallback for an unsupported group layout remain intact.
- **Zero-contribution fragment work.** Skip ALU-only detail calculations and
  other work only at existing exactly-zero contribution endpoints, retaining
  every positive-weight expression. Production shader comparisons below cover
  the implemented endpoints and detect intentionally incorrect shortcuts.
- **Exact cached/radix grass replacement.** Maintain sorted root-identity
  lookup and use stable radix passes for incoming identities and outgoing
  opacity ranks. Root order, world state, fades, cap behavior, and structural
  revisions retain the legacy results; detailed differential CPU evidence is
  preserved below.
- **Guarded empty UI submission.** Skip only valid empty passive draw data
  without callbacks, geometry, or pending texture work. Input/frame/text-input
  handling remains active and all real backend work still runs. This removes
  unused GL calls; it does not claim to fix the remaining frame-time tails.
- **Manual-quality timing-query cleanup.** Adaptive GPU queries begin only
  for automatic quality. Both latest saved-manual runs report zero adaptive
  query frames. Frame intervals remain measured, and GPU stage profiling is
  separately opt-in for diagnostic runs.
- **Conservative pool visibility and retained leaf/cloud shortcuts.** The
  pool reflection clip test and proved zero-contribution shader work skips are
  retained. Both latest walks test the pool 3,379 times but prove zero outside
  cases and skip zero reflection passes, so this route demonstrates no FPS gain
  from the pool clip. Full-resolution sampled reflections and the opaque/loading
  snapshot contracts remain unchanged.

This pass changes no source assets, vegetation density, LOD distances, image
resolution, reflection resolution, quality preset, or saved graphics preference.
The latest two walks independently verify preference preservation. These
image-preserving changes do not establish an uninterrupted 45 FPS floor.

## Current visual and correctness evidence

[visual-parity.json](visual-parity.json) records these fixed-view comparisons:

| View | Reference | Changed pixels | Maximum channel difference |
| --- | --- | ---: | ---: |
| Rainforest pool | Prior atmosphere build | 0 | 0 |
| Purchased understory | Prior atmosphere build | 0 | 0 |
| Rainforest pool | Same build with reflection scissor disabled | 0 | 0 |

These comparisons establish pixel equality for the captured views. They do not
replace the final walking run or prove equality for every camera position. The
prior [acceptance report](acceptance.json) also records zero changed pool pixels
after the upload-anchor correction. The completed intermediate grid/preparation
candidate's [acceptance report](final-grid-retry/acceptance.json) records zero
changed pixels and zero maximum channel difference for **both** pool and
understory against the prior approved atmosphere captures. The subsequent
[lossless-candidate report](final-lossless/acceptance.json) also records zero
changed pixels and zero maximum channel difference for both approved views.
Future candidate captures remain required.

Supporting logs currently include:

- [CPU checks](cpu-tests.log) and [terrain/shadow culling checks](terrain-culling-tests.log):
  50,000 directional shadow cases and 150,000 terrain/camera/skirt cases, with
  independently proved rejected bounds and invalid-input fallback.
- [Shadow endpoint GL checks](../jungle-atmosphere/shadow-endpoint-tests.log):
  896 terrain/foliage frames bit-identical to the prior shadow expression across
  the fade and its endpoints, with active-shadow and premature-cutoff controls.
- [Zero-coverage GL checks](../jungle-atmosphere/zero-coverage-tests.log):
  162 complete production vertex/fragment color, depth, and MRT frames match
  exactly, including positive-coverage, invalid-input, and unchanged-shadow
  controls. An intentionally premature 3.1-pixel cutoff is detected.
- [Pool scissor checks](../jungle-atmosphere/pool-reflection-scissor-tests.log).
- [Horizon grid checks](horizon-grid-cpu.log) and [method](horizon-grid.md):
  100,000 synthetic roots across 1,008 views, plus 95,498 real atlas roots
  across 64 views, preserve selected pair order, capped results, uploaded bytes,
  and draw counts. The isolated selector measured 1.100674 ms for the full scan
  and 0.586684 ms for the grid, a 1.876x CPU speedup with inert GL stubs. These
  timings are not native renderer FPS evidence. The log retains the initial
  fixture setup/compile failures and subsequent passing runs.
- [Wildlife preparation CPU checks](wildlife-preparation-cpu.log): all three
  resident banks and 672 creation/target/pose failure probes, including loading
  guards, cleanup/retry, unchanged actor state, and no hot allocations or IO.
- [Dressing selection checks](dressing-selection-cpu.log) and
  [method](dressing-selection.md): 284 differential cases retain every pre-cap
  pair, capped order, transformed instance/fade byte, overflow/shadow/reflection
  payload, and upload count, with no guarded allocations. Classification refreshes
  fall from 284 to 11. The isolated CPU selector measures 0.681717 ms for the
  original path and 0.346932 ms for the combined changes; inert GL stubs mean
  this is **not** native FPS evidence.
- [Native zero-contribution shader checks](zero-contribution-native-tests.log):
  the actual AMD Radeon RX 5300 renders 104 terrain plus 248 foliage cases
  (352 total) with bit-identical HDR, depth, and MRT output. Positive-contribution
  and deliberately incorrect noise/probe/sun/leaf controls retain measurable
  differences. The [software checks](../jungle-atmosphere/zero-contribution-tests.log)
  are recorded separately; neither shader fixture is a timed full route.
- [Atmosphere CPU checks](../jungle-atmosphere/atmosphere-cpu-tests.log),
  [sky GL checks](../jungle-atmosphere/sky-tests.log), and
  [water GL checks](../jungle-atmosphere/water-tests.log).
- [Empty actor draw checks](../jungle-atmosphere/empty-actor-draw-tests.log):
  zero GL work for empty submissions while absolute-time actor state and
  nonempty drawing remain intact.
- [ImGui submission checks](imgui-submission-tests.log): the actual public ImGui
  draw/texture types exercise empty, nonempty, callback-only, and pending
  create/update/destroy cases. This validates an optional experiment, not a
  production performance gain.
- [Earlier build log](build.log). Historical build and test logs alone do not
  establish the final candidate's native acceptance.

## Historical native results — targets not met

The previous full rainforest walk is retained in
[walk-empty-draws/results.json](../jungle-atmosphere/walk-empty-draws/results.json).
It completed correctly, but missed the prior slowest-one-percent FPS gate:

| Full return walk | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| Previous empty-draw baseline | 56.16 | 44.65 | 25.735 ms | 37 |
| First optimization run, [walk-1](walk-1/results.json) | 57.21 | 44.61 | 51.030 ms | 11 |
| Repeat, [walk-2](walk-2/results.json) | 56.56 | 40.72 | 114.949 ms | 28 |

Each full run retained 7,560 measured intervals after 120 warmup frames. Both
optimization walks completed correctly and preserved graphics preferences, but
both failed the all-frame target. The prior [aggregate report](acceptance.json)
therefore explicitly reports `all_frames_above_45fps: false`. Its pool,
understory, and market sweeps passed with worst frames of 20.735, 20.716, and
16.778 ms respectively; those stationary sweeps did not resolve walking hitches.

## First-use diagnosis and rejected UI experiment

[first-leopard-entry.json](first-leopard-entry.json) independently reconstructs
the unchanged walking route and animal draw-radius test. `leopard_00` crosses
90 metres between frames 1221 and 1222, matching the first frame at which its
resident shadow pose can be submitted. Historical frame 1222 took 23.765 ms
in walk-1 and 114.949 ms in walk-2. This establishes a first-use candidate and
frame alignment; it does not by itself prove a driver stall or establish a fix.
Stage GPU timings are delayed estimates, and CPU wall time can include waiting
and scheduling rather than active CPU work.

The subsequent [UI/wildlife experiment](ui-wildlife-experiment/results.json)
used two **partial** 2,400-frame runs, each retaining 2,280 measured intervals.
Both enabled wildlife loading preparation and preserved preferences; their
fixed captures matched exactly. The experiment compared skipping the hidden,
empty ImGui backend with the established full-backend path:

| Partial diagnostic | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| Optional empty UI shortcut | 53.50 | 43.98 | 25.071 ms | 20 |
| Full UI backend | 53.47 | 44.37 | 23.773 ms | 14 |

Backend wall time fell from approximately 1.860 ms to 0.001 ms, but mean
presentation time rose from 15.217 ms to 17.019 ms, with almost unchanged mean
GPU time (17.267 versus 17.279 ms). Waiting moved into presentation without
a demonstrated FPS benefit. **The empty UI shortcut was rejected as the
production default at this stage.** The subsequent full-route experiment and
later guarded-cleanup decision are recorded below; neither establishes the
FPS floor. Optional fixed-memory UI tracing is disabled unless an output path
is requested.

Neither partial run passes the FPS target or completes the full 432.633-metre
trail. Because both runs prepared wildlife, this comparison does not isolate
the preparation's causal effect. It also does not establish performance on a
cold driver cache or for other routes. Final complete-route acceptance remains
required.

## Grid/preparation intermediate — all-frame gate failed

The [complete native suite](final-grid-retry/acceptance.json) retains every
post-warmup interval from two 7,680-frame return walks and three 1,800-frame
sweeps: 20,160 measured intervals. All five runs pass the previous average FPS
and slowest-one-percent FPS thresholds. Correctness and saved-preference checks
also pass. The stronger all-frame gate fails on 19 intervals:

| Intermediate native run | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| [Walk 1](final-grid-retry/walk-1/results.json) | 56.64 | 46.50 | 23.268 ms | 4 |
| [Pool sweep](final-grid-retry/sweeps/results.json) | 56.07 | 46.59 | 22.034 ms | 0 |
| [Understory sweep](final-grid-retry/sweeps/results.json) | 58.29 | 46.78 | 22.535 ms | 1 |
| [Market sweep](final-grid-retry/sweeps/results.json) | 59.95 | 59.10 | 18.176 ms | 0 |
| [Walk 2](final-grid-retry/walk-2/results.json) | 56.65 | 45.69 | 29.448 ms | 14 |

Both walks complete the unchanged 432.632946-metre route with ten waypoints,
zero blocked/sliding steps and destination-loading holds. The reports verify
renderer/resource contracts, loading-only wildlife preparation, color-only
zero-coverage flags, and active conservative grid counters without fallback.
Both fixed-view captures match exactly as described above. These passing checks
do not override the failed all-frame result.

The earlier `final-grid/` attempt was interrupted with status 143 after a
partial CSV through frame 5001. Its [interruption record](final-grid/INTERRUPTED.txt)
and partial files are retained. That attempt has no complete results and is not
part of the accepted sample count; `final-grid-retry/` contains the complete
intermediate suite.

The repeat walk's worst interval, frame 6011, is recorded as 29.448 ms:
`raw_editor_ms` is 28.006 ms, `raw_update_ms` 0.223 ms, and
`raw_render_cpu_ms` 0.729 ms; the recorded preceding swap duration is 0.479 ms.
The editor field measures wall time across the UI block, including passive
frame setup, rendering, and state handling. It is **not thread CPU time**.
This run has no per-stage UI thread trace, and its GPU snapshots are delayed
and smoothed. The interval therefore does not establish an ImGui CPU hotspot,
an OS interruption, or a driver-stall cause.

## Lossless intermediate — all-frame gate failed

The [next complete suite](final-lossless/acceptance.json) includes the structural
grass cache, single-pass Dressing dispatch, and zero-contribution fragment
shortcuts. Its build and focused checks passed. It retains the same 20,160
measured intervals across two complete walks and three sweeps. All five
average/slowest-one-percent gates pass, but **21 intervals fail the all-frame
gate**:

| Lossless intermediate run | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| [Walk 1](final-lossless/walk-1/results.json) | 56.66 | 46.11 | 38.304 ms | 6 |
| [Pool sweep](final-lossless/sweeps/results.json) | 56.12 | 46.57 | 28.219 ms | 1 |
| [Understory sweep](final-lossless/sweeps/results.json) | 58.20 | 47.36 | 21.981 ms | 0 |
| [Market sweep](final-lossless/sweeps/results.json) | 59.95 | 58.16 | 20.309 ms | 0 |
| [Walk 2](final-lossless/walk-2/results.json) | 56.47 | 45.39 | 41.553 ms | 14 |

Correctness, full-route completion, quality/settings preservation, and both
exact fixed-view comparisons pass. The changes reduce isolated CPU selection
work as shown by the dedicated fixture, but this suite does not demonstrate an
all-frame improvement or meet the final target. The failed timing evidence is
retained in full.

The [first-walk slow-frame trace](final-lossless/walk-1/slow-frame-ui-diagnostics.json)
aligns every over-budget interval with UI wall and calling-thread CPU samples.
For frames 6051 and 6054, the empty passive backend takes 26.160 and 36.346 ms
of wall time, while consuming only 0.040 and 0.050 ms of calling-thread CPU.
Both have zero draw lists, vertices, indices, and pending texture work. This
shows that those wall intervals are not substantial active UI thread CPU work.
It does not distinguish driver synchronization, scheduling, or another waiting
cause. Delayed/smoothed GPU snapshots do not provide exact per-frame causality.

The earlier UI A/B ended at frame 2400 and did not cover these late-route tails.
This motivated the completed **full-route** comparison below. Its results do
not establish a stable FPS fix. The later decision to remove correctness-proven
empty GL submission work is separate from the unresolved performance target.

## Full-route UI comparison — FPS tails unresolved

The [complete same-binary diagnostic](ui-full-route-ab/results.json) runs
fast/reference/fast on the built radix/scalar-shadow candidate. All three
7,680-frame runs complete the full 432.632946-metre return route, retaining
7,560 measured intervals each: **22,680 total**. Source/binary/preferences,
route, and correctness checks pass. Both fast runs actually skip all 7,560
measured empty/passive/no-pending submissions, while the reference skips none.
All three endpoint BMPs match exactly, with zero changed pixels and zero
maximum channel difference.

| Full-route UI case | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| Fast 1 | 57.61 | 49.09 | 22.344 ms | 1 |
| Full-backend reference | 57.55 | 43.63 | 54.195 ms | 24 |
| Fast 2 | 57.46 | 44.72 | 83.303 ms | 8 |

All three runs fail the all-frame gate; the aggregate explicitly reports
`all_fast_frames_above_45fps: false`. The first fast run passes the older
average/slowest-one-percent gate, but its repeat does not. Fewer slow intervals
in both fast runs are useful diagnostic evidence, not proof of stable tail
behavior or a completed fix.

Mean GPU estimates are nearly unchanged: 15.501/15.503/15.518 ms. Backend wall
means are approximately 0.001/8.761/0.001 ms, while presentation means are
16.231/7.440/16.203 ms. The location of waiting changes without a demonstrated
mean frame-rate benefit. In the second fast run, frame 6448 takes 83.303 ms:
the recorded preceding swap lasts 80.526 ms, the empty backend approximately
0.0011 ms, and the whole UI block 0.0497 ms. The late cluster persists despite
skipping the backend. These wall measurements do not establish an exclusive
driver, scheduling, resource, or OS cause; GPU stage values remain delayed
estimates.

**The guarded empty-submission cleanup is now enabled by default.** This is a
correctness-proven removal of unused GL calls, not a claimed fix for frame-time
tails. The predicate remains unchanged: only valid, genuinely empty passive
draw data with no pending texture work skips the backend. Visible/nonempty UI,
callbacks, invalid data, and texture create/update/destroy requests use the
complete backend; frame/input/text-input handling still runs.

The [policy/lifecycle CPU checks](ui-default-policy-cpu.log) pass five actual
wrapper configurations: default fast, explicit full-backend override, override
precedence over the positive QA flag, zero-valued flags, and an empty disable
value. Each exercises 401 queued events and 420 balanced frames, callback and
pending-texture work, keyboard/mouse/text-input transitions, and policy latching
after initialization. Fast configurations submit 11 backend calls and skip
409 empty frames; forced-full configurations submit all 420. The public ImGui
submission fixture also passes. Together with the three exact endpoint images,
this supports cleanup safety while leaving the failed all-frame result intact.

`VALWOOD_QA_DISABLE_EMPTY_UI_FAST_PATH=1` retains the full-backend reference.
The former positive QA flag is redundant; the disable flag takes precedence.
The policy is included in the latest built clean-walk candidate. Its native
correctness checks pass, while the all-frame timing goal remains unmet as
reported at the start of this document.

## Shadow-fetch diagnostic — no demonstrated gather gain

The [same-binary native A/B](shadow-fetch-ab/results.json) runs
scalar/gather/gather/scalar through four 1,200-frame pool sweeps. Each retains
all 1,080 post-warmup intervals: **4,320 measured intervals, zero over budget**.
All four endpoint BMPs match exactly, with zero changed pixels and zero maximum
channel difference. Runtime fingerprints, graphics preferences, and correctness
contracts remain identical. The scalar QA reference compiles out gather code
so it does not inherit the gather path's register cost.

| Diagnostic case | Mean GPU time | Average FPS | Slowest 1% FPS | Worst frame | Over budget |
| --- | ---: | ---: | ---: | ---: | ---: |
| Scalar 1 | 16.348 ms | 57.82 | 50.99 | 20.402 ms | 0 |
| Gather 1 | 16.392 ms | 57.63 | 50.88 | 19.826 ms | 0 |
| Gather 2 | 16.395 ms | 57.72 | 49.99 | 20.386 ms | 0 |
| Scalar 2 | 16.364 ms | 57.85 | 50.39 | 20.539 ms | 0 |

The gathers preserve the same nine logical shadow samples, but these runs show
**no demonstrated performance gain** on this machine. Production now compiles
gather out by default. `VALWOOD_QA_ENABLE_SHADOW_GATHER` requests the separately
compiled experimental path, and `VALWOOD_QA_DISABLE_SHADOW_GATHER` takes
precedence. The scalar policy is present in the built full-route UI candidate.
This stationary diagnostic does not replace a complete-route test or establish
that the remaining walking dips are resolved.

The [actual AMD gather fixture](shadow-gather-amd.log) separately passes 7,776
RGBA32F visibility cases covering formats, sizes, borders, subtexels, fades, both
PCF modes, and fallback controls. The [AMD leaf-shadow fixture](leaf-shadow-amd.log)
also passes 480 night/tiny-sun boundary cases and the 352-frame full fragment
parity comparison. These correctness checks do not turn the unsuccessful
gather optimization into a performance claim.

## Grass publication diagnosis and linear candidate

The [isolated component profile](dressing-update-components.json) identifies
grass replacement as the largest Dressing component at frame 257: 1.579545 ms
of 2.415499 ms, alongside 0.304674 ms pumping and 0.531010 ms selecting. Frame
172 has no publication or pumping and spends approximately 0.309 ms selecting.
This fixture follows the ideal route with the actual atlas/cover code but
excludes native driver work and scenery collision queries. It identifies real
component work; it does not explain every native over-budget interval or the
late UI wall waits.

The [linear-match candidate](grass-transition-linear.md) keeps identity lookup
order across replacement and compaction, then matches sorted identity streams
without repeated binary searches. Desired source order, retained outgoing-root
ranking, fades, world state, caps, and structural revisions remain unchanged.
Its [differential log](grass-transition-linear-cpu.log) records 1,845 complete
byte/state/revision comparisons with zero guarded allocations and independent
lookup invariants, including reentry, reorder, compaction, invalid input,
teleport/reset, empty and saturated generations.

| Isolated actual replacement | Previous mean | Linear mean |
| --- | ---: | ---: |
| Rainforest generation 2, 8,454 → 8,602 roots | 1.537398 ms | 0.870128 ms |
| Rainforest generation 3, 8,602 → 7,794 roots | 1.489445 ms | 0.836281 ms |
| Castle, 46,000 → 46,000 roots | 10.780217 ms | 5.992008 ms |

Maintaining the lookup adds approximately 0.047 ms to rainforest compaction and
0.336 ms to dense castle compaction. The measured replacement reductions are
CPU-only results from alternating controls with complete state comparisons,
not a native frame-rate gain.

The [installed radix/cache implementation](grass-transition-radix.md) also
passes the frozen legacy differential and focused integer/float edge cases,
including exact opacity/identity ordering, stable ties, backing storage, and
zero guarded allocations. Its isolated actual-cover means are 1.557646 →
0.478119 ms and 1.495502 → 0.454318 ms for the two rainforest replacements,
and 10.820349 → 2.941627 ms for the castle replacement. The
[radix log](grass-transition-radix-cpu.log) records those CPU proofs. This
implementation is in the built full-route UI candidate above; that candidate's
native outliers show that reducing replacement CPU work alone is insufficient
to meet the all-frame goal.

## Monitored query-mode attempt — checker failure retained

The [tracked coordinator](monitored-profile-sequence/coordinator.json) planned
frame-only/stages/frame-only full walks but stopped after the first engine run.
The generic timing loader incorrectly required a positive GPU-stage estimate
even when stage queries were deliberately disabled. The
[checker log](monitored-profile-sequence/01-frame-only/checker-runner.log) retains
that failure. No complete checker `results.json` or aggregate acceptance exists;
the coordinator records zero completed validated runs. Its running status is
false, and the partial attempt remains preserved.

The [offline diagnostic](monitored-profile-sequence/01-frame-only/resource-analysis.json)
independently summarizes the raw 7,680-row CSV: 7,560 post-warmup intervals,
56.815 average FPS, 42.940 slowest-one-percent FPS, 43.369 ms maximum, and 30
over-budget intervals. It is explicitly **not valid checker acceptance** and
does not substitute for the renderer/route/settings checks that the failed
checker never completed. No synthetic successful checker result was created.

The [external resource trace](monitored-profile-sequence/01-frame-only/resource-monitor.json)
also shows that this monitor is too intrusive for clean acceptance timing:
10.161 seconds of its own CPU over 176.701 seconds, about 5.75% of one CPU core,
with a 5.75 ms average sample CPU cost and a 74.402 ms maximum sample read wall
time. Zero dropped samples or missed sampling ticks do not make that observer
cost negligible. The artifact lacks individual read-duration markers, so the
worst monitor read cannot be assigned to one game frame.

Across the 135.3-second sampled engine span, cumulative DRM accounting records
approximately 116.867 seconds of game graphics-engine work. Known background
clients account for another 8.824 seconds: Cinnamon 4.547, ChatGPT 2.490, Firefox
1.785, and approximately 0.001 seconds from Discord. These are sampled engine
counters, not exact per-frame waiting or proof that another application caused
a particular hitch. The worst frame's approximately 200 ms sample window
contains 177.80 ms of game graphics work and 10.32 ms from known other clients;
it also spans eight main-thread involuntary context switches.

Game residency ranges from 1.559–2.366 GB of VRAM and 0.112–0.919 GB of GTT;
aggregate VRAM ranges from 2.365–2.852 GB of 3.204 GB. Those are real accounting
changes, but do not prove an eviction caused a particular frame delay. Samples
are 100 ms apart and slow-frame windows include an extra 100 ms on each side;
neighboring frames contribute and overlapping windows must not be summed.
Client catalog/permission limits and observer perturbation prevent exclusive
GPU, scheduling, resource, or OS attribution.

The generic unavailable-GPU-timing validation was corrected and passed its
focused mode checks. The completed **unmonitored** walks under
`final-unmonitored/` are reported at the start of this document. The failed
monitored run is retained independently; its resource monitor is not used in
those clean acceptance walks.

## Current verification and remaining limit

The built source and correctness checks pass, including the guarded empty-UI
policy, radix grass transitions, retained leaf/cloud shader behavior, scalar
shadow policy, and conservative pool clip proof. Pool clipping does not engage
on the measured walking route, so its correctness proof must not be presented
as measured headroom there. The remaining seven over-budget intervals prevent
an all-frame success claim.

The [native SDL display audit](display-mode-audit.json) reports a current
2560 × 1440 desktop at 60 Hz and 26 enumerated modes, with **no exact
1600 × 900 mode**. The fullscreen QA patch is unapplied; exclusive fullscreen
is not a validated solution and no display-mode or quality preference has been
changed. Post-submission pumping and worker variants also remain unapplied
ideas, not measured fixes.

| Current acceptance item | Result |
| --- | --- |
| Focused CPU/shader checks and build/source freeze | Pass |
| Latest complete native evidence | [Two clean walks](final-unmonitored/results.json) |
| Measured intervals | 15,120, none dropped or filtered |
| Average/slowest-one-percent FPS gate | Both walks pass |
| Over-budget intervals | Seven: six plus one |
| Complete routes and preserved preferences | Pass |
| Scene/GL/resource-cleanup correctness | Pass |
| Endpoint image parity | Zero changed pixels in both walks |
| All-frame 45 FPS gate | **Fail** |
| Fullscreen QA patch | Unapplied |

The **all-frame gate** requires every measured walking frame to take at most
`1000 / 45` milliseconds (approximately 22.222 ms), hence zero over-budget
frames. This is stricter than the previous average-and-slowest-one-percent
45 FPS gate, which could pass despite isolated slower frames. Final acceptance
must retain the approved image settings and completed collision-tested route.

The historical five-case suites remain retained separately; the latest repeat
is two full walks, not a newly completed five-case sweep suite. Every latest
walk completes all ten waypoints with zero blocked/sliding steps or destination
loading holds, intact renderer/resource contracts, unchanged preferences,
wildlife preparation, color-only zero-coverage flags, and conservative grid
counters without fallback. These passing checks and improved slowest-one-percent
results do not waive any of the seven intervals above the budget.
