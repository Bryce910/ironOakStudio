# Old Tanthoon rainforest — native verification

Added a **temperate fantasy rainforest** to the existing custom Valwood engine. Open `../Launch_Valwood_Rainforest.sh`, or select **Old Tanthoon | Emerald rainforest** under Scenic trails. **Rainforest heart** opens a second view. Both arrive on foot.

The regional ecology footprint is approximately 13.1 × 9.6 km, with a smooth boundary and protected shorelines. A 210 × 235 m pocket adds detailed dressing around a 216.316 m path: 50 mature broadleaf trees, 25 firs, 12 young pines, 170 ferns, 45 mossy rocks, 12 fallen logs and 36 flowers. The 350 new records share existing full-resolution licensed models/materials. The two earlier placement files remain unchanged. The world now loads 1,989 authored records in 39 shared model groups, within a 2,048-record cap; the largest alias batch is 552.

Rainforest ecology favors layered broadleaf/fir canopy and close ferns over meadow grass. Damp litter/moss and local humid haze/cloudier lighting distinguish the area. The source map shows wooded Old Tanthoon; this specific biome and path are a compatible production addition, rather than a location explicitly prescribed by the lore.

[Entrance render](rainforest-view.png) · [Deeper canopy render](rainforest-deep-view.png) · [Actual overhead layout](../../../projects/valwood_world/scenery/rainforest_layout.svg) · [Editing and biome API](../../RAINFOREST.md)

## Native measurements

All profiles ran sequentially on the **AMD Radeon RX 5300 at 1600×900**, using the visible native driver and normal HUD. The first 120 frames are discarded. Software GL and CPU tests are correctness evidence, not FPS measurements. Raw JSON, frame CSVs, images and world/resource reports are retained alongside these results.

| Rainforest test | Measured frames | Mean FPS | 1% low FPS | p99 ms | Worst ms | Frames over 22.222 ms | Mean GPU ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| [path](path-final.json) | 7,560 | 59.95 | 59.66 | 16.728 | 16.964 | 0 | 13.749 |
| [rainforest](rainforest-final.json) | 1,680 | 59.95 | 58.92 | 16.722 | 20.671 | 0 | 13.485 |
| [rainforest-deep](rainforest-deep-final.json) | 1,680 | 59.95 | 59.56 | 16.738 | 17.082 | 0 | 13.559 |

All **10,920 measured rainforest frames** met the 45 FPS budget. Every run averaged 59.95 FPS; the worst frame was 20.671 ms (48.38 FPS). These fixed-condition native tests establish smooth performance for this area on this machine, not a guarantee across every world location or desktop workload.

The physical walk covered **432.632946 m**, following the six-point path to its northern end and back. It completed in 7,417 walking frames / 123.616667 simulated seconds at 3.5 m/s, reached all ten subsequent waypoints, returned within 1 mm and recorded **zero collision blocks, slope blocks or sliding retries**. This is an out-and-back walk. The native route is driven by the same movement/collision helpers used by ordinary play.

All new profiles recorded zero terrain-loading holds, failed tiles, GL errors, contact-pass failures or remaining texture-cache owners. New scenery reuses the existing 39 model groups and texture images. Regional sampling allocates no memory; existing cover, instance, streaming and lighting-target budgets remain bounded.

## Existing-area checks

| Check | Measured frames | Mean FPS | 1% low FPS | Worst ms | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| [Original start diagnostic](regression/start-final.json) | 780 | 58.23 | 42.42 | 37.948 | 2 |
| [Original lake](regression/lake-final.json) | 780 | 59.95 | 59.52 | 17.287 | 0 |
| [Original start recheck](regression-recheck/start-final.json) | 1,680 | 57.98 | 46.50 | 22.785 | 2 |

The first starting-area diagnostic had a 37.948 ms frame with 21.768 ms attributed to CPU update time. A longer quiet recheck retained two brief budget misses but did not reproduce that large spike. Both runs are retained. The earlier starting-area pass already identified arrival as the heaviest view; these results do not claim an absolute 45 FPS floor there. The lake remains at 60 FPS with reflection enabled. All three checks report rainforest weight zero and clean renderer/resource status.

## Verification and limits

- [Complete test suite](tests.log) passed. All eight rendering fixtures also passed on the Radeon: [scenery](scenery-native.log), [lighting](lighting-native.log), [foliage](foliage-native.log), [water](water-native.log), [temporal AA](temporal-native.log), [models](models-native.log), [sky](sky-native.log) and [contact lighting](occlusion-native.log).
- The actual-model scenery fixture checks all 1,305 half-meter sweeps across three player-capsule lanes on the rainforest path. Earlier starting-area sweeps remain clear. All nine inspection views draw real color/shadow shaders with zero GL errors and cache owners.
- [Atlas release tests](atlas-release.log) preserve 8,385 terrain samples, 8,254 excluded climate samples and all fifteen settlement centers against pre-change hashes. The new trail follows existing terrain; no coast, water bed, settlement pad or world scale changes were needed.
- [Shader comparison](shader-equivalence.log) matches the original terrain shader exactly in tested disabled/outside cases; inside material and path controls change as intended. [Cover tests](cover-release.log) preserve the four zero-weight instance buffers byte-for-byte while testing close fern density, shared trail exclusion and stable roots.
- Atlas, cover and scenery [sanitizer checks](cover-asan.log) pass with AddressSanitizer/UndefinedBehaviorSanitizer. LeakSanitizer was disabled because it cannot operate under this sandbox’s ptrace. Explicit GL/cache owner assertions cover the tested resource teardown.
- [Authoring validation](author-validation.log) and [deterministic replay/CLI checks](author-cli.log) pass. Existing Seldonne files, original placements and the supplied map pass the [preservation audit](preservation.json). All 222 rendered-build fingerprints match before/after profiles in [the build manifest](build-manifest.json). [Summary JSON](summary.json) retains measured counts and route statistics.

This area uses temperate species and remains a prototype. Native stills show repeated fern clusters, low-relief ground, some leaf-edge stippling and open canopy gaps. The ordinary local tree field uses the rainforest species mix; the distant horizon still uses pine/fir cards. Terrain wetness uses the dry climate channel while vegetation has a finer shoreline fade. Further art and rendering work remains before AAA production fidelity.

Reproduce after closing the live engine, with host Radeon access and other CPU/GPU jobs idle:

```sh
make -j4
make test
python3 tools/build_rainforest_area.py --validate-only
python3 tools/validate_starting_native.py --output docs/performance/rainforest-recheck
python3 tools/benchmark_rainforest.py --captures --output docs/performance/rainforest-recheck
python3 tools/benchmark_rainforest.py --output docs/performance/rainforest-recheck
```

WASD walks, right-drag looks, Shift runs, Space jumps, H toggles tools and F enables preview flight.
