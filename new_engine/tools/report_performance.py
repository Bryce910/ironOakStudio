"""Summarize checked-in native measurements without hiding frame-time tails."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];D=ROOT/'docs/performance'
names=['baseline-play','cinematic-play','baseline-editor','cinematic-editor']
rows=[json.loads((D/(name+'.json')).read_text()) for name in names]
metrics=[('Startup (s)','startup_ms',.001),('Mean FPS (capped)','mean_fps',1),('Median frame (ms)','median_ms',1),('p95 frame (ms)','p95_ms',1),('p99 frame (ms)','p99_ms',1),('Worst frame (ms)','max_ms',1),('Terrain GPU (ms)','mean_terrain_gpu_ms',1),('Shadow GPU (ms)','mean_shadow_gpu_ms',1),('Prop GPU (ms)','mean_props_gpu_ms',1),('Grass GPU (ms)','mean_grass_gpu_ms',1)]
text='''# Cinematic rendering and responsiveness results

Tested 23 October 2026 on the native AMD Radeon RX 5300 (3 GiB), Mesa 25.2.8, at 1600×900. Each matching run starts a fresh process, follows the same deterministic moving-camera route for 360 frames, and excludes 60 warm-up frames. VSync is enabled at 60 Hz. Source/OS/shader caches can affect startup. Baseline contains the original sparse scene; the cinematic build also renders substantially denser vegetation, additional dressing and full static-object PBR. The comparison therefore includes visual-quality work, not only optimization.

| Measurement | Baseline play | Cinematic play | Baseline editor | Cinematic editor |
| --- | ---: | ---: | ---: | ---: |
'''
for title,key,scale in metrics:text+='| '+title+' | '+' | '.join(f'{r[key]*scale:.2f}' for r in rows)+' |\n'
text+='''
Both versions largely hit the display cap. These results establish reduced startup and terrain GPU cost while adding detail; they do not establish a higher uncapped average frame rate. Rare 32–34 ms frames persist, including worse maximum frame times than the baseline in these final short runs. Occasional first-load/streaming and presentation stalls remain. Do not describe this build as hitch-free or commercially certified.

Static-resource telemetry in the final runs records 20 unique cached assets serving 165 live instances, 147 cache hits and about 944 MiB of estimated unique static GPU storage. That estimate excludes terrain, animated characters, render targets and driver allocations; it is not total VRAM consumption. Baseline cache fields are zero because that system did not exist, not because it used no memory. The warm-cache eviction target cannot evict assets that still have live instances.

GPU stage values average smoothed asynchronous timestamp measurements. CPU sections include driver blocking, so the large `editor_ms` value must not be interpreted as pure UI computation. JSON and per-frame CSV data are retained beside this report. The earlier 720-frame out-and-back run is retained as `cinematic-play-long.json`; it predates the final terrain culling/material polish and is a separate extended check.

## Validation

`make -j4` and `make test` pass. Tests cover launch validation, import copies and duplicate protection, malformed input, shared audio shutdown/reinitialization under AddressSanitizer, actual GLB placement/transform/collision, Save World/readable/terrain/light round trips, PBR/terrain/grass shader linkage, shared-resource lifetime across release/reload, terrain vertex/index buffer destruction and async thumbnail generation cancellation. Existing unrelated compiler warnings remain in legacy engine code; the newly added cache/profiling/thumbnail modules introduce no compiler warnings in the final build.

Native arrival, river and editor captures were reviewed. These are renderer outputs; no generated concept image replaces gameplay evidence. Original project files before the scene-dressing pass are retained under `projects/seldonne/backups/cinematic-pass`.

## Inspect and reproduce

- [Original arrival](captures/baseline-arrival.png)
- [Final arrival](captures/cinematic-arrival.png)
- [Final river](captures/cinematic-river.png)
- [Final editor](captures/cinematic-editor.png)

From the engine root: `./tools/benchmark_valwood.sh play` or `./tools/benchmark_valwood.sh editor`. Local runs write `local-play.json` / `local-editor.json`, preserving these reference measurements. `--frames 720` extends the travel check. Run on the native desktop GPU; offscreen llvmpipe results are not comparable.

## Remaining visual and engineering work

The scene still has mixed stylized and realistic assets, thin/aliased tree silhouettes, simple grass geometry, abrupt mesh/impostor transitions and limited indirect lighting/reflection quality. Temporal AA, higher-fidelity intermediate foliage LODs, consistent hero assets, reflection probes and broader streaming/editor soak tests are the next milestone. See [the production roadmap](../PRODUCTION_ROADMAP.md). This pass is a foundation for the AAA target; it does not meet that target yet.
'''
(D/'RESULTS.md').write_text(text)
print(D/'RESULTS.md')
