# Old Tanthoon bandit camp validation

Crows Rest is a walkable outlaw camp in Old Tanthoon at map UV (0.295635, 0.657759), world X/Z (-20,600 m, 8,946.321 m). Its natural ground lies around 187.6–188.1 m elevation. The camp adds 296 authored scenery records, including six shelters, two firepits, barricades, supplies and six practical-light/flame emitters. Ten bandits occupy it: five patrol walkers, one courier and four sentries.

The requested online pack was not downloaded. This implementation uses the existing licensed medieval asset library and the locally supplied Meshy Bandit Ranger. It does not claim to have imported the requested vendor pack. The sentry breathing and lowered-arm stance are explicitly authored from a supplied pose; patrol and courier locomotion use the supplied motion clips.

Open `Launch_Valwood_Bandit_Camp.sh` in the `final_engine` folder to enter the camp. The launcher hides the tools initially. Press **H**, then use **Scenic trails** for the four views: `bandit-camp` (entrance), `bandit-supplies`, `bandit-lookout` and `bandit-fire`. Use **WASD** to walk, hold the **right mouse button** to look and hold **Shift** to sprint. **F10** opens graphics settings; the saved Custom 85% manual profile retains the 60 FPS target, 2048 shadows, half-resolution reflections, contact lighting, mist and clouds. No normal lighting or effect flags were removed for the ordinary day/dusk runs.

This folder separates authored-scene checks, runtime correctness, visual captures and measured performance. The final five-case native batch passed. Its measured camp sweeps and walking loop met the strict 45 FPS gate; this is finite scene evidence, not a whole-world performance guarantee. Final illustrative captures are being recorded separately.

The validation helper is `tools/check_tanthoon_bandit_camp.py`. It uses the actual generated layout/manifest and the `bandit_camp` runtime report. It does not build, cook, modify scenery, or overwrite an earlier evidence directory.

## Required evidence

- The new camp source must retain the original four authored sources byte for byte. Its record count, aliases and model hashes must match the generated manifest, with no duplicate poses or unexpected model paths.
- Validate the real terrain and combined scene through the actual scenery/capsule fixture: a 0.37 m radius, 1.75 m tall player must traverse the entrance and authored loop in both directions. Probe each segment at most 0.5 m apart, including both path edges. Test actual collider geometry rather than a second approximate implementation.
- Check actor support over complete movement routes with the runtime's actual foot/contact footprint, dry ground and obstacle rules. Walking and running use their real source clips. The sentry stance is authored from one supplied pose with arm lowering and a small procedural breathing cycle; it is not a supplied idle or work animation. Source period zero and authored cycle length are recorded separately by the importer.
- Test residency outside the exit radius, entering the preload radius, remaining within hysteresis, leaving, and returning. Runtime updates must not perform model IO, allocations or GL work. A failed load must release partial resources, and all borrowed pose-blend resources must release before their source poses.
- Independent main/reflection/shadow batches must use each pass's actual camera. Nearby offscreen actors may still cast shadows. Rendering must report zero GL errors, failed tiles, unsafe actor roots, load failures, graphics-apply failures and remaining texture-cache owners.

## Native runs

Native jobs must run sequentially through `Launch_Valwood_World.sh`, which holds the existing game-instance lock. Use a 1600×900 visible window with the saved Custom 85% fixed-resolution setting, 60 FPS target, 2048 shadow map, half-resolution water reflections, contact lighting, mist and clouds enabled. Preserve `projects/valwood_world/graphics.settings` exactly; read and validate its state without rewriting it.

Separate day and dusk stationary sweeps use 1,800 total frames and a 120-frame warmup. The ordinary `bandit-camp` / `river` walking loop uses 2,520 total frames including 120 warmup, covering the 130.112 m loop at 3.5 m/s. Its original timing JSON/CSV retains loading, upload and target-transition costs. The 900-frame correctness-only return check starts resident and uses `VALWOOD_QA_BANDIT_RETURN=1` to travel through the usual destination path to the market at frame 480. This demonstrates unloading; it does not claim a continuous outside-to-inside approach or an FPS result for travel. `VALWOOD_QA_BANDIT_LIGHTS_OFF=1` disables only camp practical lights and flames for the matched dusk A/B; actors and scenery remain unchanged.

Captures use 300 frames without profiling and must retain the full 1600×900 BMP, metadata and PNG. Captures are visual evidence, not an FPS measurement. The helper must reject offscreen/software output and verify the actual AMD renderer.

Fingerprint executable, source, shader, model, scenery and ground inputs before and after a native batch. The helper takes the shared game lock around full hashing, then releases it for the independently locked launcher. Only cheap metadata checks and the tiny preferences-file check run between scenes. Preserve all logs and profiles, including failures. Report mean FPS, 1% low, p99, maximum frame time, GPU stages and memory separately from correctness; the strict performance gate is mean and 1% low both at least 45 FPS. A failed performance gate does not prevent later scenes from being measured.

## Measured native evidence

The completed AMD native batch is recorded in `docs/performance/tanthoon-bandits/native/results.json`, with the original per-case timing JSON/CSV, logs and full-size captures alongside it. Display resolution was 1600×900; the saved fixed 85% profile rendered the scene at 1360×765 with the normal Balanced effects retained. Each timed case excluded its declared 120-frame warmup and retained every subsequent frame interval.

| Case | Measured intervals | Mean FPS | 1% low FPS | p99 (ms) | Maximum (ms) | Within 22.222 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Day sweep | 1,680 | 59.95 | 59.62 | 16.739 | 16.970 | 100% |
| Dusk sweep | 1,680 | 59.95 | 59.70 | 16.726 | 16.786 | 100% |
| Dusk, camp lights/flames off | 1,680 | 59.95 | 59.71 | 16.727 | 16.865 | 100% |
| Full walking loop | 2,400 | 59.95 | 59.70 | 16.726 | 16.837 | 100% |

All 7,440 measured intervals met the 45 FPS frame budget, and every timed case passed the separate mean-and-1%-low gate. These results cover the specified camp views and one loop on this machine; they do not establish performance for every location, future content, startup, unrelated system activity or a long-duration whole-world run.

The walking case completed 130.112455 m at 3.5 m/s in 2,231 walking frames, with zero collision-blocked, slope-blocked or sliding steps and zero return error. The separate 900-frame return check loaded the actor bank once, unloaded it once and finished with zero resident actors or pose assets. It is a correctness result, not a travel FPS measurement.

Normal cases retained all ten actors, five walkers, one courier, four sentries and forty pose assets, with six practical lights and flame submissions. The scoped light-off case retained the same actors and scene while reporting zero active camp lights and zero flame draws. Both dusk cases were VSync limited at 59.95 FPS. Their observed total GPU means differed by −0.042 ms (on minus off), while main-foliage timing differed by +0.126 ms; mixed stage differences and driver/system variation prevent treating these values as a causal light-cost estimate.

All five cases passed the GL, tile, actor safety, loading, graphics-apply and final resource-owner checks appropriate to each case. Runtime source fingerprints and the user's original `graphics.settings` were preserved. Peak aggregate device VRAM use was 2,972,573,696 bytes (about 2.77 GiB); this public device counter includes other desktop/application use and is not engine-only memory attribution.

## Commands

Run static checks only:

```sh
python3 tools/check_tanthoon_bandit_camp.py
```

After the parent task authorizes the native GPU slot, run the day/dusk/light-cost profiles, full walking loop and correctness-only unload check sequentially:

```sh
python3 tools/check_tanthoon_bandit_camp.py --native --output docs/performance/tanthoon-bandits/native
```

Capture the four views in day and dusk, without timing:

```sh
python3 tools/check_tanthoon_bandit_camp.py --native --captures --output docs/performance/tanthoon-bandits/captures
```

Use `--runs` to select `day`, `dusk`, `lights-off`, `route` or `return`; use `--views` to select `bandit-camp`, `bandit-supplies`, `bandit-lookout` or `bandit-fire`. `--verify-existing` validates saved reports without launching, full input hashing or rewriting captures. An explicit short `--frames` value cannot silently turn a full route into a partial success.

## Current status

Static inventory/hash validation passed for 296 camp records, 5,585 combined records and the original four unchanged source files. Independent continuous segment-to-rotated-source-box checks cover the entire visual path width plus a 0.37 m player capsule: minimum additional margins are 0.572 m on the loop, 1.005 m on the approach and 0.466 m on the escape path. These are conservative full-source bounds, including foliage, rather than a substitute implementation of the engine's compound colliders.

All ten actor routes/stance roots clear measured scenery using their complete blended-source XZ envelopes. The minimum extra scenery margin is 0.461 m at the leader/table. After the gate sentry moved to local (-4,24), its stationary-to-moving capsule separation has at least 0.319 m extra margin; the two rounded moving paths have at least 0.496 m beyond two 0.4 m body capsules.

The environment owner's final CPU scene fixture passed with all five source files, actual GLB position bounds, the real atlas, real `world_scenery.cpp` compound colliders and real bandit runtime: 1,572 three-lane main-walk sweeps, 1,289 rounded NPC-path sweeps, four final stations, and 600 simulated seconds / 18,001 updates with ten actors, zero unsafe roots, zero hot allocations and zero remaining model owners. The final full-pose contact rectangles include a 15 mm margin; maximum terrain spread across the nine oriented support probes is 38.605 mm, below the 50 mm runtime limit.

All 182 nonvegetation camp footprints, including barricades, also passed 9×9 rotated source-bound grass-exclusion probes: 14,742 probes cover corners, edges and interiors, with minimum cached exclusion 0.999824. The 114 authored trees/plants are intentionally exempt from their own footprint-mask assertion. These are mask changes with no terrain-height edits. The final log is `docs/performance/tanthoon-bandits/scene-cpu.log`. This is runtime correctness evidence, not rendered graphics or FPS evidence.

The checker itself passed syntax/help checks, independent exact geometry fixtures, negative controls for a relocated source obstruction and the old gate-sentry overlap, and mocked five-run native orchestration. The mock proved that an FPS failure does not stop later cases, preferences remain untouched, the full route uses 2,520/120 frames, the return run has no FPS gate, scoped light A/B deltas are retained, and `--verify-existing` does not rewrite evidence. Its log is `/tmp/valwood-tanthoon-checker-tests.log`; no launcher or GPU workload was run by those mock tests. The completed native measurements are reported above; the final illustrative capture batch remains separate.
