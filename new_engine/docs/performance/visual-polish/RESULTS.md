# Silverpine Waystation — visual polish and native verification

The starting-area visual pass is implemented in the existing native Valwood engine. It improves forest enclosure, ground-material transitions, fine canopy coverage, volumetric sky and local contact lighting. The scene is ready for another walk-through. **It remains a prototype; these changes do not establish AAA production fidelity.**

Run `../Launch_Valwood_Starting_Area.sh` from the engine folder. WASD moves, right-drag looks, Shift runs, Space jumps, H toggles tools, F toggles flight and Tab opens the atlas. The scenic buttons return to the five inspection pockets.

## Visible changes

- **Forest composition:** 384 additional placements bring the new scene to 1,356 records, alongside the preserved 283 original records: 1,639 total. The starting layout now has 113 trees, compared with 32 before this pass, with overlapping fir, broadleaf and young-pine groups. Fern beds, logs and boulders connect their edges. Connected low wall returns enclose the workshop, grove and overlook.
- **Ground:** smaller irregular paving footprints preserve the level foundations while returning vegetation around their margins. Bounded material patches break up the grass, moss and litter, retain photographed surface colors and align litter color/normal/ARM sampling. Controlled clearing grass retention rises from 22% to 50%; solid footprints and walking paths remain excluded.
- **Foliage:** fine needles survive strongly minified alpha mips in both color and shadows. Stable geometric normals keep broad ambient illumination consistent as normal-map detail fades. The existing complementary LOD intervals remain intact.
- **Sky:** clouds now occupy a 2700–4100 m volume with density erosion, approximate self-shadowing and aerial extinction. Twelve bounded view steps run at half resolution; a small tent filter reduces marching grain during upsampling. Main and reflected views cache separate targets.
- **Grounding:** a new depth-aware pass adds local ambient obscurance and short sun-contact shadows. It reads a separate indirect-light attachment, so ambient obscurance does not darken direct sunlight. The visible improvement around prop bases is subtle; this is screen-space contact shading rather than full scene-wide indirect light transport.

## Actual native views

All five are 1600×900 captures at normal 1.75 m eye height, using the final renderer and local models. They are game renders, not concept images.

| View | Before | Current |
| --- | --- | --- |
| Arrival | [Before](before-views/start-view.png) | [Current](start-view.png) |
| Courtyard | [Before](before-views/courtyard-view.png) | [Current](courtyard-view.png) |
| Workshop | [Before](before-views/workshop-view.png) | [Current](workshop-view.png) |
| Overlook | [Before](before-views/overlook-view.png) | [Current](overlook-view.png) |
| Grove | [Before](before-views/grove-view.png) | [Current](grove-view.png) |

The [contact-disabled grove](contact-disabled/grove-view.png) is retained as a controlled diagnostic comparison. The final color-view culling changes do not remove scene content or shadow casters.

## Performance and correctness

All measured runs used the **AMD Radeon RX 5300, visible native driver, 1600×900 output, normal HUD and 60 Hz presentation**. Each discards 120 warm-up frames. Runs were sequential, with the live game and other engine test jobs closed. Software GL tests are correctness evidence only. Raw JSON, frame CSVs, captures and resource reports accompany each run.

| Scene | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames over 22.222 ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [route](route-final.json) | 6,120 | 59.81 | 52.50 | 18.078 | 21.155 | 0 | 14.519 |
| [start](start-final.json) | 1,680 | 56.70 | 46.19 | 21.055 | 22.445 | 2 | 16.453 |
| [courtyard](courtyard-final.json) | 1,680 | 59.95 | 59.64 | 16.718 | 17.197 | 0 | 13.935 |
| [workshop](workshop-final.json) | 1,680 | 59.39 | 49.30 | 19.723 | 21.738 | 0 | 14.849 |
| [overlook](overlook-final.json) | 1,680 | 59.88 | 51.43 | 18.880 | 21.857 | 0 | 14.872 |
| [grove](grove-final.json) | 1,680 | 59.95 | 51.39 | 18.724 | 22.263 | 1 | 14.706 |
| [forest](regression/forest-final.json) | 1,680 | 59.74 | 49.12 | 19.712 | 22.141 | 0 | 13.911 |
| [lake](regression/lake-final.json) | 1,680 | 59.95 | 59.61 | 16.723 | 17.205 | 0 | 13.395 |

Across **17,880 measured frames**, three isolated frames exceeded the 45 FPS budget: **99.98322%** stayed within it. The six starting-area runs account for 14,520 frames and the same three misses. The worst frame, 22.445 ms, corresponds to 44.55 FPS. These measurements support the 45–60 FPS target for this tested area and machine, with rare brief dips; they do not establish an absolute minimum under every desktop workload or world location.

The new lighting costs are bounded: measured sky time ranges from 0.150–0.263 ms and contact composition from 0.789–1.017 ms. Those are delayed GPU-query estimates. Added contact targets plus the indirect attachment use 27.47 MiB; sky noise and targets use 3.89 MiB inland or 4.58 MiB with reflection at this output size. These are added resources, not total VRAM usage.

An initial complete run exposed unnecessary offscreen color submissions. The final version packs visible instances at the front of each reusable buffer, draws that prefix for color, and keeps the full nearby list for shadows. Culling respects actual aspect, vertical FOV and the mirrored view. A bounded 80 KiB CPU instance-staging array is reserved per imported model, avoiding repeated upload allocations. The retained [pre-culling profiles](iteration-pre-cull/) show the same denser scene: route mean GPU time fell from 15.719 to 14.519 ms, while mean FPS rose from 58.11 to 59.81. The arrival view benefits less because most nearby objects are visible. The earlier [pre-polish results](before-RESULTS.md) remain a separate, lighter-scene baseline.

The actual movement route completed **350.256672 m** in 6,005 ordinary walking frames / 100.083 simulated seconds. It reached all 18 waypoints, returned to its origin and recorded zero collision blocks, slope blocks or sliding retries. Every final profile reported zero terrain-loading holds, failed tiles, GL errors or remaining texture-cache owners.

Verification evidence:

- [Complete final test suite](tests-final.log) passed. The final binary also passed all eight Radeon fixtures: [water](water-native.log), [foliage](foliage-native.log), [lighting](lighting-native.log), [scenery](scenery-native.log), [temporal AA](temporal-native.log), [models](models-native.log), [volumetric sky](sky-native.log) and [contact lighting](occlusion-native.log).
- Native model/scenery fixtures directly query primitives to prove zero offscreen color drawing with retained shadows, safe prefix bounds and mirrored/aspect handling. Model upload tests prove zero C++ allocations during first, maximum and varied uploads and preservation of existing placements after invalid input.
- [Layout validation](layout-validation.log) uses actual transformed GLB geometry. The combined scene has 39 render groups and 438 compound colliders; 1,872 player-capsule sweeps pass.
- [Preservation audit](preservation.json): all 35 original Seldonne authored files, the original 283 placement records and supplied map bytes remain unchanged. [Build fingerprints](build-manifest.json) match all 215 executable/source/asset inputs before and after final profiles. [Machine-readable summary](summary.json) was cross-checked against raw frame CSVs.

## Remaining visual work

The native images still show repeated grass tufts, low-relief ground, some stippled foliage edges, simple intact wall tops and recognizable modular props. The clouds have real thickness but retain a broad procedural weather footprint and limited sampling; their silhouettes can still feel layered. Contact lighting lacks hidden or offscreen geometry and does not provide bounced light. The scene needs further authored architecture, surface breakup, vegetation presentation and art direction to approach AAA imagery. Motion shimmer and more weather/time combinations need dedicated review beyond these fixed views.

The tower remains a closed exterior; lanterns and firepits remain unlit visual props. This pass adds no quests, NPC behavior or persistent micro-AI simulation. The wider atlas scale remains unchanged; this 350 m walk is not proof of an eight-hour connected world crossing.

[Editing layout](../../STARTING_AREA_PLAN.md) · [Asset provenance](../../STARTING_AREA_ASSET_SOURCES.md) · [Sky implementation](../../VOLUMETRIC_SKY.md) · [Contact-lighting implementation](../../CONTACT_LIGHTING.md)

## Reproduce

Close the live game and keep GPU jobs sequential. Native runs need host Radeon access; the restricted sandbox otherwise selects llvmpipe. Use a fresh output directory to retain these measurements.

```sh
make -j4
make test
python3 tools/validate_starting_native.py --output docs/performance/visual-polish-recheck
python3 tools/record_starting_area_build.py --output docs/performance/visual-polish-recheck/build-manifest.json
python3 tools/benchmark_starting_area.py --captures --output docs/performance/visual-polish-recheck
python3 tools/benchmark_starting_area.py --output docs/performance/visual-polish-recheck
python3 tools/benchmark_cinematic_scenery.py --scene forest --frames 1800 --expect-authored --expect-lighting --expect-integrated --output docs/performance/visual-polish-recheck/regression
python3 tools/benchmark_cinematic_scenery.py --scene lake --frames 1800 --expect-authored --expect-lighting --expect-integrated --output docs/performance/visual-polish-recheck/regression
python3 tools/check_starting_area_preservation.py --output docs/performance/visual-polish-recheck/preservation.json
python3 tools/record_starting_area_build.py --verify --output docs/performance/visual-polish-recheck/build-manifest.json
```
