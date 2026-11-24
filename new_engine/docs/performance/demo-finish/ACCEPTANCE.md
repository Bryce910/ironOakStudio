# Finished demo acceptance

Run `python3 tools/check_demo_finish.py --help` from the engine root for the
current command syntax. The helper never starts a renderer without `--native`.
Source validation preserves the original seven scenery sources, original model
and graphics-preference hashes, and checks the additive 582-record layer for a
total of 7,242 instances across eight sources: 382 castle-district records and
200 rainforest records. Castle dressing includes 28 infill homes, eight market
canopies, 15 measured facade attachments, and 18 counter goods. Rainforest gates
require the exact seven planting-pocket inventories, 18 palms, production-atlas
support, 5-by-5 footprint water probes, and preserved trail/root/pool sightlines.
The complete 108.206 m arcade branch requires all seven old rigid sources and
new rigid footprints checked over 10,272 OBB/segment pairs, with at least 0.4 m
extra clearance, plus 1,329 dry terrain checks across three walking lanes.
The finishing recipe is validated
against its actual model LOD hashes, production CPU atlas, roads, actor routes,
and unchanged exterior moat. Validation occurs while the shared instance lock
is held, before any measured renderer is started.

The full native acceptance command is:

```bash
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/check_demo_finish.py \
  --native --require-probes --require-audio \
  --output docs/performance/demo-finish/native-final
```

Use a fresh output directory for every run. The suite covers the complete
585.072 m walking route, the complete 216.412 m arcade out-and-back walk,
Bakers Arcade, finished west and east streets, market,
tavern interior, moat garden, castle garden, rainforest pool, and rainforest
understory: eleven cases in total. Stationary cases default to 1,800 frames;
the original route receives 10,320 frames and the arcade walk 3,960 frames.
Saved 85% render scale, manual 60 FPS target, 2K shadows,
reflections, contact shadows, volumetric fog and clouds remain active.

The helper freezes the executable, Makefile, all engine sources including local
probes/audio/viewer, both launchers, root and actual project shaders, models, scenery, ground,
particles, audio, the atlas image, graphics preferences, acceptance/generator
scripts, derived texture blocks, and the pinned BC7 encoder and cooking sources. It
records file metadata between jobs and performs complete hashes only before and
after all rendering. It never builds, cooks, hashes large assets, or launches a
second renderer while a measured job is running.

Every interval after the declared 120-frame warmup remains in the timing result.
The helper independently recomputes average FPS, the slowest 1% average, maximum
frame time and slow-frame counts from the complete CSV, and rejects missing
frames or a summary that excludes hitches. Both average and 1% low must reach
45 FPS for each case. Whether **every individual frame** stays within 22.222 ms
is a separate reported result. A passing 1% low does not establish that stronger
claim. Correctness and performance gates remain separate; a correctness failure
does not discard an otherwise valid timing record.

Probe telemetry must show ready 128-square captures, bounded memory, matching
face/readback inventories, zero failures, and no baking during walking. Audio
must load all eight ambience beds and twelve footstep variants, update each
frame, respect its fixed voice/gain budget, and generate footsteps from grounded
distance only. Automated native jobs use the dummy audio driver while reporting
the actual backend. This checks initialization and runtime behavior; listening
on the actual audio device remains a manual review.

Separate fixed-time, untimed visual captures:

```bash
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/check_demo_finish.py \
  --native --captures --require-probes --require-audio \
  --output docs/performance/demo-finish/final-captures
```

Capture mode excludes both walking routes and makes no FPS claim. `--runs` selects
individual cases. `--frames` changes stationary duration and is recorded in each
result; both complete routes retain their full budgets. The `--no-require-probes` and
`--no-require-audio` switches are diagnostic controls, explicitly recorded in the
evidence. The finished-demo acceptance invocation requires both features.

Helper validation performed without launching the engine: the existing complete
10,320-frame route CSV retained all 28 frames above the 45 FPS budget, including
its 38.619 ms worst interval. A synthetic 100 ms hitch failed the 1% low gate
while its average still exceeded 45 FPS. Altered summaries and missing CSV rows
were rejected. These are helper correctness checks, not results for the new demo.
