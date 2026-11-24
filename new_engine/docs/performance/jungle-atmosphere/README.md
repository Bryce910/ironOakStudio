# Rainforest atmosphere

The jungle now blends into a regional overcast look: cloud cover rises to 0.96,
direct-light strength falls to 0.36 of the global control and sky fill to 0.60,
with neutral green-gray fog and a darker sky. A smoothstep of the existing
atlas biome weight drives the transition; it has no per-frame allocations or
camera-time adaptation. At zero rainforest influence the global controls and
castle lighting remain unchanged.

The humid ground-fog weight doubles, taking the ordinary 0.62 mist control to
1.24 in the jungle core. A bounded 0.00080 extra aerial-haze coefficient gives
distant trunks atmospheric separation. Nearby ground and plants remain legible.
The existing eight-step ground volume and twelve-step half-resolution clouds
keep their current targets and sample caps; full overcast avoids the two direct
cloud-light density queries whose solar contribution is then zero.

The overcast sky sheet suppresses blue gaps and the visible sun smoothly,
including when volumetric clouds are disabled. Reduced canopy lighting and
ordinary surface haze also remain on lower quality settings; expensive effects
stay controlled by the user's existing settings. Main geometry, sky, water,
particles and loading-only scene probes use the same local lighting. Water's
reflection fallback now matches the cloudy sky at reflection edges and with
planar reflections disabled.

## Residency and walk correctness

Startup and destination loading now use a CPU-only wildlife selector that
prefers the ordinary/current habitat, then permits one nearby habitat inside
its existing exit envelope. It prepares that bank behind the loading view.
Ordinary walking keeps the original enter/exit hysteresis and staged-loading
policy; the selector does not expand the walking thresholds or perform model
I/O, allocation or GL work itself.

The [wildlife CPU regression](../demo-finish/wildlife-loading-residency-cpu.log)
passes all three existing habitat envelopes, 1,536 annulus/edge/outside/priority
probes and the prior failure, collision, bounded-actor and hot-path checks.
The completed [preloaded rainforest return walk](walk-preloaded/results.json)
traversed **432.633 m**, reached every waypoint and returned with zero position
error. It recorded zero collision, slope or sliding blocks and zero mid-walk
destination-loading frames. The initial 120 warmup frames are separate. The
wildlife bank loaded once before walking, with no unload or load failure;
local probes baked only during loading. These correctness results do not imply
that its frame-time gate passed.

## Checks

- The [final optimized build](build-empty-draws.log), production layout
  validation and final route/scene checks passed. The final frame-time gate
  remains below target as recorded below.
- [Pure CPU tests](atmosphere-cpu-tests.log) cover exact exterior controls,
  deep-jungle targets, 10,000 monotone traversal samples, smooth endpoints and
  input bounds.
- [Software sky GL tests](sky-tests.log) cover neutral overcast with volume
  on/off, zero visible sun leakage, partial transitions, original normal-weather
  output, fixed targets, finite pixels, state and cleanup.
- [Software water GL tests](water-tests.log) retain all prior water checks and
  compare actual rendered output to legacy controls: normal cloud values
  0/.28/.65 are bit-exact; overcast, mirrored-altitude, reflection-edge fallback
  and reflected solar-halo suppression pass.
- [Native fixed-time captures](captures/results.json) pass scene, weather,
  material, probe, audio-resource, cleanup and unchanged-graphics checks. The
  castle comparison against the previous finished demo differs at only four of
  1,440,000 pixels, each by one 8-bit channel step. No original source asset or
  saved setting changed. Audio correctness used the SDL dummy backend; this
  does not establish an audible-device review.

## Native measurements

These are targeted RX 5300 runs at the saved manual 60 FPS/85%-scale profile:
1600 × 900 display, 1360 × 765 rendering, 2K shadows, reflections, contact
shadows, volumetric fog and clouds. Settings stayed unchanged. Every measured
interval after the 120-frame warmup is retained, including stalls; no build,
cooking, image processing, source edits or asset hashing ran during timing.
The strict gate requires both average and slowest-1% FPS to reach 45.

| Run and view | Average FPS | Slowest 1% FPS | Worst frame ms | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| [Earlier atmosphere run: pool](native-final/rainforest_pool-rainforest-pool.json) | 57.22 | 49.13 | 20.827 | 0 |
| [Earlier atmosphere run: understory](native-final/understory-rainforest-purchased-understory.json) | 58.36 | 49.14 | 20.594 | 0 |
| [Earlier atmosphere run: castle market](native-final/market-castle-market.json) | 59.95 | 59.73 | 16.797 | 0 |
| [Preloaded run: pool — gate failed](native-preloaded/rainforest_pool-rainforest-pool.json) | 56.94 | 36.40 | 49.947 | 6 |
| [Preloaded run: understory](native-preloaded/understory-rainforest-purchased-understory.json) | 57.78 | 46.47 | 22.096 | 0 |
| [Preloaded run: castle market](native-preloaded/market-castle-market.json) | 59.95 | 58.50 | 18.934 | 0 |
| [Isolated pool repeat](native-pool-repeat/rainforest_pool-rainforest-pool.json) | 55.75 | 46.11 | 22.387 | 1 |
| [Preloaded 432.633 m walk — gate failed](walk-preloaded/results.json) | 56.13 | 44.19 | 32.306 | 42 |
| [Final 432.633 m walk — gate failed](walk-empty-draws/results.json) | 56.16 | 44.65 | 25.735 | 37 |

The later pool run's six clustered stalls remain part of the evidence. Its
scene/resource counters and average GPU stage costs matched the earlier run,
but the stall cause has **not been established**. Scheduling and driver waits
remain hypotheses. The isolated repeat passed the average/slowest-1% gate;
that is non-reproduction of the severe stalls, not a proven fix, and its one
frame over 22.222 ms is still reported.

The complete preloaded walk passed route and scene correctness but missed the
slowest-1% gate at 44.19 FPS. Its 42 over-budget frames, including the 32.306 ms
worst frame, are not removed or waived. The latest targeted results therefore
do **not** establish that every current demo case meets the 45 FPS gate.

Full frame records are retained in the [earlier atmosphere pool CSV](native-final/rainforest_pool-rainforest-pool.json.csv),
[preloaded pool CSV](native-preloaded/rainforest_pool-rainforest-pool.json.csv),
[isolated repeat CSV](native-pool-repeat/rainforest_pool-rainforest-pool.json.csv)
and [complete preloaded walk CSV](walk-preloaded/rainforest-walk.json.csv).
The corresponding run directories retain logs, world telemetry, source freezes
and captures. Fixed-time comparison images are in [captures/](captures/).

## Final build and complete walk

Population, training and pets now skip empty draw submissions using their
committed batch counts, matching the existing watch renderer. This removes
unnecessary program/uniform setup far from the castle without skipping actor
updates or public queries. [CPU/stub regressions](empty-actor-draw-tests.log)
verify zero GL/query/upload work and unchanged incoming state for empty color,
reflection and shadow submissions, failed-upload retention, nonempty draws,
and all 159 actor states advancing 35 km away and matching reentry.

The production layout was regenerated and validated after the source change.
Only its two actor-source hash fields changed; placements, terrain includes
and inspection cameras remained byte-identical. The [hash audit](revalidated-actor-source-hashes.json)
and [layout proof log](revalidate-layout.log) preserve that check.

The [final walk](walk-empty-draws/results.json) completed all 432.633 m and ten
waypoints, with zero collision/slope/sliding blocks, zero mid-walk loading,
120 warmup frames and zero return error. All 7,560 post-warmup frame intervals
remain in the [complete CSV](walk-empty-draws/rainforest-walk.json.csv).
Preferences and frozen runtime hashes matched before/after measurement.
The 360 distance-based footsteps match independently integrated frame-end
displacement; corners explain the small difference from route arc length.

Its average was 56.16 FPS and slowest 1% was 44.65 FPS, with 37 isolated frames
over 22.222 ms, no consecutive over-budget frames and a 25.735 ms worst frame.
The strict 45 FPS gate therefore **failed**. This single rerun does not prove
that the small empty-draw cleanup caused the timing differences from the
earlier walk. The severe earlier pool stalls also remain unexplained. The
atmosphere and loading correctness checks pass; a consistent 45 FPS minimum
has not been achieved. The full earlier failures remain above.

## Earlier full-demo evidence

The previous [finishing report](../demo-finish/FINISH_REPORT.md) records eleven
full-suite cases and five fresh-shader-cache repeats. Those sixteen cases
belong to the earlier finished-demo build, before this jungle-atmosphere and
wildlife-loading work. They are historical evidence and do not replace the
current targeted measurements or final walk.
