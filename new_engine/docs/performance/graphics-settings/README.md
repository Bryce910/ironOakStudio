# Graphics settings and native measurements

Press **F10** during play to open **Graphics and performance**. The panel shows FPS, the target, display size and actual scene size. Choose settings, then **Apply and save**; **Undo edits** restores active values and **Recommended** restores the model defaults to the draft. Changes commit at a ready scene after required scene/shadow allocations and persistence succeed. Failed application retains the prior configuration. Pending requests use a snapshot of the selected values.

Valwood's saved project profile is now **Custom: fixed 85% scene scale, automatic mode off, target 60 FPS, with every Balanced effect enabled**: 2048 shadows, half-resolution reflections, contact/ambient shading, volumetric mist and volumetric clouds. At 1600×900 output this renders the scene at 1360×765. This project preference was saved through the settings API and read back; it is separate from the unchanged engine defaults below.

| Preset | Maximum scene scale | Shadow map | Reflection scale | Contact shading | Clouds / mist |
| --- | ---: | ---: | ---: | --- | --- |
| Performance | 85% | 2048 | Quarter | Off | On |
| Balanced | 100% | 2048 | Half | On | On |
| Cinematic | 100% | 4096 | Full | On | On |
| Custom | User selection | User selection | User selection | User selection | User selection |

Reflection scale is relative to the internal scene dimensions. Visual edits mark the preset Custom. Presets preserve the automatic-quality preference and FPS target, resetting the minimum image scale to 70%. They retain world geometry, vegetation coverage, animal animation and physical weather behavior. Clouds off bypasses cloud raymarching while preserving the atmosphere/sun; mist off leaves rain, wind and wetness intact. Water-reflection and contact/ambient-shading passes have separate toggles.

For **fixed quality**, turn **Automatic image quality** off: the selected maximum becomes the fixed scene scale. Optional automatic mode adjusts only internal resolution between the selected 70–100% bounds, using fresh GPU timings and frame cadence, cooldowns and slow recovery. It does not change desktop/UI resolution. Close F10 to resume automatic adjustments; loading, configuration changes and inactive/minimized windows do not vote on adaptation. With no saved file, the model currently defaults to Balanced, automatic mode enabled, target 60 FPS and a 70% minimum. Targets offered are 45, 60, 90 and 120 FPS. These targets are controller goals.

Preferences live in the active project root's `graphics.settings` (`projects/valwood_world/graphics.settings` for Valwood). Version 1 is a complete `key=value` file, at most 4096 bytes. Unknown/duplicate/missing fields, nonfinite numbers and invalid ranges reject the whole file; startup retains defaults. Saving writes and fsyncs a unique sibling temporary file before atomic rename. Transient automatic reductions are not saved as the next launch's permanent ceiling. CLI overrides include `--graphics-preset`, `--auto-quality on|off`, `--target-fps` and `--render-scale`.

## Completed evidence

[CPU results](final-unit-tests.log) cover 16 exact persistence roundtrips, 73 whole-file rejections, byte/range boundaries, save-failure cleanup, controller serial/epoch freshness, cadence gating, cooldown/recovery, exact user bounds and zero policy allocations. The [live native switching check](switching/result.json) passed three apply/save operations at 75/90/100% scene scales, 2048/4096 shadows and all effect toggles, with full 1600×900 output and no GL/resource failures. It restores the original preferences and makes no FPS claim.

The [earlier native run](native/results.json) and [final automatic run](final-native/results.json) use this machine's **AMD Radeon RX 5300**, 1600×900 output, Balanced effects, a 60 FPS target and scripted scene sweeps. Each scene has 1800 frames, with 120 warmup frames and 1680 measured samples. Earlier automatic mode started at 100%; the final run capped it at 90%.

| Scene | Earlier mean / 1% low FPS | Final mean / 1% low FPS | Final scale | Final maximum frame |
| --- | ---: | ---: | ---: | ---: |
| Castle hero | 59.95 / 56.60 | 59.95 / 59.63 | 90% | 16.901 ms |
| Castle market | 59.95 / 56.12 | 59.95 / 59.68 | 90% | 16.832 ms |
| Wildlife farm | 59.95 / 53.80 | 59.95 / 59.71 | 90% | 16.818 ms |
| Wildlife rainforest | 58.93 / 32.89 | 59.55 / 40.03 | 75% | 51.968 ms |

Automatic rainforest resizing still produces visible hitches. The earlier run made six scale changes and reached a 59.507 ms maximum frame. The final run made three reductions, 90→85→80→75%, and retained three measured frames above 33.33 ms; its 1% low **fails the 45 FPS gate**, despite 99.821% of frames meeting the 22.222 ms budget. These resize costs remain in the reported timing statistics. See the [final rainforest profile](final-native/wildlife-rainforest-final.json) and [resize log](final-native/wildlife-rainforest-final.log).

## Stable fixed-resolution native results

The completed [stable native batch](stable-native/results.json) uses **fixed 85% scene scale with Balanced effects**, automatic mode off, on the same RX 5300 at 1600×900 output. All four scenes passed correctness and the mean/1% low 45 FPS gates. Each has 1680 measured samples after 120 warmup frames; **all 6720 measured frames met the 22.222 ms budget**, with no adaptive scale changes. The batch preserved the existing preferences.

| Scene | Mean FPS | 1% low FPS | P99 frame | Maximum frame |
| --- | ---: | ---: | ---: | ---: |
| Castle hero | 59.95 | 59.40 | 16.778 ms | 17.029 ms |
| Castle market | 59.95 | 59.41 | 16.750 ms | 17.205 ms |
| Wildlife farm | 59.95 | 59.69 | 16.726 ms | 16.859 ms |
| Wildlife rainforest | 59.04 | 49.74 | 19.811 ms | 20.703 ms |

This fixed profile is the saved choice for this machine. Automatic mode remains optional; the preceding automatic-run results retain its measured rainforest resize hitches rather than treating the fixed-profile result as evidence that those transitions were eliminated.

These are finite scene-sweep and switching tests on one machine, not an eight-hour traversal or every-weather acceptance run. The benchmark's 45 FPS gate checks mean and 1% low; maximum-frame data separately exposes individual stalls. Current evidence does not establish uninterrupted 45–60 FPS across the full world. The [benchmark tool](../../../tools/benchmark_graphics_settings.py) retains scene reports and full-size captures for review.
