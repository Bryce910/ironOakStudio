# Source-scene motion rendering audit

The publisher scene contains spatial mesh batches rather than ordinary root-centered props. Architecture/geology batches span up to roughly150m; backdrop meshes span2–18km. Their original native policy selected/crossfaded entire batches by their root and discarded them at2km. This hid close HIGH geometry at distant batch edges and omitted much of the authored mountain horizon.

Source-only corrections in `world_scenery.cpp` and `world_scenery_streaming.inc`:

- `nm_scene_backdrop_` renders its pinned MID tier without the2km root fade. Publisher confirmed original background LOD0 was disabled; the exporter duplicated MID into both files for compatibility. The redundant HIGH tier is neither eagerly uploaded nor requested by streaming. Its CPU metadata remains available for editor bounds.
- Other `nm_scene_` batches share the union of their near/MID bounds for selection. CPU selection uses nearest/farthest distance to the actual transformed3D AABB; this admits HIGH where a wall is near the camera while retaining MID where distant pieces remain. The private color/shadow fragment shaders use matching3D surface distance for complementary31–39m fades, with no vertex-layout change. Other aliases keep root-distance behavior, and shader state resets after scenery drawing.
- Initial source HIGH batches enter through the existing coordinator rather than being eagerly uploaded before `adopt`. This allows geometry/texture reservations to reject or delay a HIGH request while keeping pinned MID visible. The previous eager path could exceed the geometry budget before adoption. MID fallback remains bounded and resident. Source prefetch uses3D bounds, so other floors well above or below do not request HIGH just because their XZ boxes overlap.

A/B switches, captured when creating a scene:

```
VALWOOD_QA_DISABLE_SOURCE_HORIZON=1
VALWOOD_QA_DISABLE_SOURCE_BOUNDS_LOD=1
```

The second switch restores the old root-distance selection/fade and original eager HIGH behavior for comparative captures. The new default may submit more HIGH vertices because it recovers nearby source geometry. `source-lod-cost-estimate.json` records conservative group/triangle estimates before frustum rejection; these are not measured GPU time or FPS. Native route captures and separate frame-time runs must decide the performance tradeoff on the3GB RX5300.

The native main/reflection far plane is15km; publisher Camera was10km (`publisher-scene-audit.json`, camera field). Across33,406 backdrop MID vertices, recommended town/gate/hall views contain none beyond15km. Upper courtyard contains only6 such vertices (maximum16.77km), already under near-complete10.5–14.9km fog. No projection expansion is needed in this pass.

Existing world-space shadow texel snapping is already active. Its radius160m and125–160m fade remain unchanged, as do detail-normal80–125m fading, source baked indirect lighting, normal variance roughening and temporal edge history. Extending directional shadows would require a separately measured cascade/range design; no speculative global shadow or exposure change was made.

Validation: `make test-world-scenery-cpu` exercises the real atlas/scenery parser/selection/streaming with deterministic model/GL stubs, including ordinary alias behavior, source long-wall selection, per-floor rejection, whole-horizon visibility, redundant HIGH suppression, no hot allocations and a geometry budget fully occupied by pinned MID. `tools/patch_showcase_spatial_lod.py` makes the private shader changes reproducible and is idempotent on candidate copies. Native GPU work is scheduled by the parent agent.

Follow-up admission correction: real CPU preparation along the native gate/bridge smoke route reproduced11 budget failures across38 HIGH candidates (the first23 requests reproduced the native six). Source batches now release a decoder scratch reservation only after the corresponding allocation is freed, instead of accumulating scratch from prior images. Other loaders keep their former conservative policy. Source HIGH is color refinement; the already-pinned MID group is now the single unfaded shadow caster, avoiding a duplicate MID payload and shadow mesh for every HIGH. Native GPU and CPU budgets are unchanged. All38 HIGH candidates subsequently prepared successfully with exact source bounds; see `../visual-finish/source-prepare-validation.json`. Budget-blocked optional HIGH no longer counts as critical while its MID is usable. Native smoke must confirm failure counters and sustained performance after final link.
