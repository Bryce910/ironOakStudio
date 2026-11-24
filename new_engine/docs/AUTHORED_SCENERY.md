# Authored scenery foundation

`projects/valwood_world/scenery/authored_instances.txt` is the editable source for
283 deliberately grouped scenery placements. It adds 26 trees, 74 rocks, 168
ferns, 12 fallen trunks and three coastal outcrops. The forest entrance and its
first 350 meters have twelve framing trees, forty stones, 120 ferns and eight
logs. Smaller pockets mark the rest of the 2.28 km trail, Ulasse shore and the
Crosswind headland. The alpine viewpoint has no placements in this layer.

Each non-comment line contains six fields:

```text
alias map_u map_v yaw_degrees scale burial_meters
```

UVs refer to the full source map, east/right and south/down. Trees and ferns use
the atlas root height. Rigid rocks, cliffs and logs use the lowest terrain sample
under their rotated/scaled actual model bounds, minus explicit burial and 5 cm
of contact slack. This bake runs once at load, using a bounded 9×5 footprint for
cliffs and 5×5 for other rigid props; updates add no terrain samples. Yaw rotates
about world Y. Cliff and sloping-ground log compositions follow local contours
so their long axes avoid spanning the downhill gradient. Inline comments label individual placements.
Changing a line changes the next load; live editing/save UI is a later step.
Two 22 m/21.5 m broadleaf heroes frame opposite trail edges about 44 m and 66 m
ahead of the forest entrance. Broadleaf scale is bounded at 2.5; other tree types
remain bounded at 2.0. The full crown footprints still clear the walking surface.

The fixed aliases are `pine`, `fir`, `broadleaf`, `pine_young`, `rock0`–`rock2`,
`fern0`–`fern2`, `deadwood` and `cliff`. They resolve only to known models. The
data cannot request an arbitrary asset path. All referenced assets and their
LODs must load successfully; errors include a line number or missing asset.
Malformed/nonfinite records, duplicates, wet roots, invalid scales/burial and
models whose horizontal bounds intrude on the trail are rejected. The footprint
test conservatively preserves the full 3–4 m walking surface, including rotated
props. These placements do not add collision or gameplay logic.

Tree color bands match the existing viewer: near 0–82 m, mid 62–265 m, and far
215–1,680 m, with the existing shader's overlap fades. Each batch sets actual
model height for projected-size rejection; a 3.2 m sapling uses its own height.
The pine/fir cards compensate their existing 0.75/0.66 m transparent root
padding. Broadleaf/young-pine cards have zero padding. Fallen wood switches from
6,500 to 1,500 triangles at 80 m and stops at 260 m. Ferns stop at 115 m, rocks
at 240 m and the three large outcrops at 1,000 m. Far-card/fern visibility includes
both the main and reflected view. Near shadow casters remain around the camera.

The implementation has at most 768 source records and 21 model batches. Vectors
are reserved at load, updates allocate no storage, and an unchanged selection
does not upload instances. A 128 m snapped camera-relative anchor preserves
foreground precision. Failed uploads retain their previous batch and anchor.
The module caches two programs' uniform locations. Shared material images use
the existing texture cache; mesh ownership remains per loaded model asset.
These are bounded component costs, not a frame-rate guarantee.

To regenerate the initial composition from the frozen atlas, from the engine
directory:

```sh
python3 tools/build_authored_scenery.py --force
```

The tool compiles a temporary CPU-only atlas sampler and uses the real trail
spine. It preserves an existing editable file by default; `--force` explicitly
replaces it. It uses fixed asymmetric compositions rather than a global random
scatter. Direct file edits are the normal authoring workflow after bootstrap.

`tests/world_scenery_test.cpp` uses the actual atlas with model/GL stubs to test
parsing, selection bands, zero update allocations, stable uploads, upload-failure
publication, uniform caching and teardown. `tests/world_scenery_gl_test.cpp`
loads actual GLBs, checks their real footprints, forces partial-load cleanup,
and renders color/shadow pixels at four destinations. The GL fixture defaults
to offscreen; `VALWOOD_TEST_NATIVE_GL=1` preserves the native driver. It accepts
the absolute engine directory as its first argument. It is a correctness test,
not a performance measurement. It also independently samples real terrain at
0.5 m spacing under actual rigid-model bounds. It checks final support and proves
the prior center-height cliff placement would leave a multi-meter gap. This
guards the authored compositions; the finite support bake is not a general
surface-conforming mesh or collision solution. The read-only
`world_scenery_placement()` inspector exposes the grounded pose and loaded bounds
for this QA and future authoring tools.

## Temporal resolve integration and limits

`engine/world_temporal.h` supplies the separate HDR temporal resolve. Generate
its eight-sample pixel jitter, apply it to the main projection and analytical
sky, and pass the exact jittered VP, jitter pixels and double camera origin in
`WorldTemporalFrame`. Resolve only newly rendered views. Bind the matching
`world_temporal_output()` and `world_temporal_depth()` textures for post; reuse
them during loading holds. Reset on travel or lighting discontinuities, and
free the module before destroying the GL context. Reflection/shadow projections
remain unjittered. Scene alpha is optional temporal stability, where one means
static; animated grass, leaves and water use lower confidence.

One resolve pass uses approximately sixteen texture taps and two ping-pong
RGBA16F/R32F sets, about 32.96 MiB at 1600×900. It corrects camera-relative
origins, rejects disoccluded history including the full bilinear footprint,
clamps history to the current neighborhood, and automatically invalidates on
resize, clipping-plane changes or camera cuts. Animated objects have no exact
motion vectors; reactive confidence limits their history rather than claiming
accurate wind reprojection. Low-resolution photographic cards, repeated assets
and authored scene composition remain separate visual limitations.

CPU tests cover reprojection precision, jitter bounds and invalid matrices.
The GL fixture checks accumulation, depth/sky rejection, camera cuts, resize,
reactive pixels, held outputs and synthetic diagonal convergence with actual
shaders. These tests establish correctness of the exercised cases, not final
scene frame rate, all-motion ghosting quality or finished AAA production fidelity.
