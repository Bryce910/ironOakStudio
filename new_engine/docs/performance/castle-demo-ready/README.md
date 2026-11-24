# Castle and jungle walking-demo verification

This pass adds 95 placements to the existing polish layer, bringing it to 316 placements / 7,558 authored instances in nine scenery sources. The original eight instance sources are preserved. Taverns gain stocked shelves and counters, food/tankards and hearth supplies; arcade foundations gain low plants and delivery details. Two additional owned shield knights guard the arcade and tavern approaches, for six shield-watch actors sharing the same eight pose assets and textures. Dark Death Lord remains reference-only.

From `new_engine`, launch `../Launch_Valwood_Demo.sh` or `../Launch_Valwood_Jungle_Camp.sh`. WASD walks, hold right mouse to look, Shift sprints, F9 advances through nine destinations, and F10 opens graphics settings. This is the visual walking demo requested; no playable objective is added.

## Rendering changes

- Opaque depth draws bind the shared neutral color texture, avoiding bindings of unused large color atlases. Masked alpha coverage and ordinary color/reflection draws remain unchanged. Actual-GL fixtures compare static and blended-pose depth/geometry and scoped-program restoration.
- Eligible opaque facade RGB auxiliary maps join the existing full-resolution BC7 cache. Production alpha-edge RGB preparation and native mip generation happen first; only unused result alpha is then normalized for encoding. Every-mip RGB/error/bias and base-PSNR guards remain; rejected maps retain original RGBA. Source images, normal/ARM maps and masked maps are preserved. Native tests check 43,341,957 values across 117 mip levels, source utility-alpha cases and upload fallback.
- The added cache coverage saves another 40 MiB of named texture storage. The earlier file-backed texture-key change separately reduces private CPU memory; it does not compress GPU images.
- Village frames complete GPU work before the unchanged VSync swap. Paired native tests improved slow-frame pacing. The forest retains its original overlap: forcing completion there lowered 1% results. All waits remain in measured frame intervals. The rejected BRDF experiment is retained as diagnostic evidence only.

## Final native results

Hardware: AMD Radeon RX 5300. Saved Custom/manual 60 FPS settings are unchanged: 1600×900 output, 1360×765 scene, 2K shadows, half-resolution reflections, contact shadows, nine-tap soft shadows, clouds/fog and rainforest rain. Firefox remains open and minimized; Steam/Discord remain open. This desktop condition materially affects performance. No application was closed.

| Test | Mean FPS | Slowest 1% FPS | Frames below 45 / samples | Worst frame |
| --- | ---: | ---: | ---: | ---: |
| Castle street and tavern return | 59.95 | 48.14 | 7 / 10200 | 25.143 ms |
| Arcade return | 59.95 | 51.27 | 0 / 3840 | 20.691 ms |
| Tavern sweep | 59.95 | 52.07 | 0 / 1680 | 20.275 ms |
| Camp clearing | 59.81 | 50.47 | 0 / 1680 | 20.308 ms |
| Camp approach and return | 58.75 | 48.21 | 0 / 2160 | 22.130 ms |
| Full jungle return | 56.31 | 46.55 | 4 / 7560 | 23.402 ms |

Mean-and-slowest-1% gate: **PASS**. Every-measured-frame 45 FPS gate: **FAIL**. Every post-warmup interval is retained, with 120 warmup frames and no filtering of hitches. No concurrent cook/build/hash scan runs during measured play. These results do not establish an absolute frame floor or achieved AAA visual quality.

All final route/resource/scene checks pass. Castle routes complete 585.072 m and 216.412 m; jungle 432.633 m and camp 117.153 m return walks have no blocked/sliding/slope-rejected steps or walking loading holds. The camp passes 3,600 actual support/separation samples over two simulated minutes and three-lane approach checks. All six knight stations have measured terrain and capsule clearance. Native captures were visually reviewed; timing captures are not visual-quality or FPS claims.

The nine-stop integration completes with both retained probe regions and clean resource release. Automated audio uses the dummy device; the interactive demo uses normal audio. See `readiness.json`, `regional-castle/`, `regional-forest/`, `tour-regional/`, `visuals-final/`, `color-native-test.log`, `models-native-test.log` and `watch-pose-regressions.log`. Earlier failed candidates/results remain intact.

## Reproduce

Close the running demo first and run one native rendering job at a time from `new_engine`:

```bash
python3 tools/check_demo_finish.py --native --frame-times-only --runs route arcade_route tavern --frames 1800 --output docs/performance/castle-demo-ready/recheck-castle
python3 tools/check_demo_polish_camp.py --native --output docs/performance/castle-demo-ready/recheck-camp
python3 tools/check_rainforest_performance.py --native --frame-times-only --output docs/performance/castle-demo-ready/recheck-jungle
```
