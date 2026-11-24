# Nearby-creek startup preparation review

This is a read-only source review of `engine/world_viewer.cpp`,
`engine/world_sky.cpp`, `engine/world_water_scene.cpp`, and the authored creek
and view coordinates. No executable, build, GPU capture, asset hash, or native
performance test was run for this review. Only this report was written.

## Geographic coverage

`castle_creek_nearby` does not read camera yaw, pitch, aspect, or basis vectors.
It uses the same village-local coordinates, enclosed-hall exclusion, altitude
limit, broad bounds, nearest point on each segment, and distance allowance as
the geographic part of `castle_creek_visible`. The visible test additionally
rejects segments against the camera frustum. Thus the intended nearby gate is
a heading-independent superset, with one small rounding mismatch in the
implementation frozen for the `native-water-ready` batch:

- Nearby computes `max(half_width) + 4` in double precision.
- Visible first converts that radius to `float`.
- For the authored half-widths 3.8m and 1.4m, the float radius rounds upward by
  0.000000190735m and 0.000000095367m respectively. A boundary band that narrow
  can pass visible's distance gate while failing nearby's gate.

The exact strict-superset correction is to use the same `const float radius`
and `120. + radius` in both functions. This was reported to the root agent;
runtime files were left untouched during the native batch. After that batch
finished, the root agent applied this exact radius match and rebuilt for the
separate `native-water-ready-final` route evidence.

After that batch finished, the root agent applied this exact radius correction.
The final predicate now matches the visible test's distance threshold. The
four startup branch decisions below remain identical; the final executable's
route is rechecked separately under `native-water-ready-final/`.

The current four native cases are far from that boundary. Distances below were
calculated from the actual authored polyline and decoded scenic UVs, using
visible's float-rounded radius. A positive margin means geographically inside
the startup gate; a negative margin means outside. Segment indices are zero
based. All four starts pass the shared altitude and broad-bound checks and are
outside the enclosed castle hall.

| Case/start | Village-local X,Z (m) | Best segment | Distance (m) | Signed inclusion margin (m) |
| --- | --- | --- | ---: | ---: |
| West street | -130,116 | 15–16 | 92.020078 | +35.779922 |
| Tavern interior | -145,-77 | 18–19 | 74.000000 | +53.800000 |
| Closed route start | -145,-62 | 18–19 | 74.000000 | +53.800000 |
| Original castle market | 0,50 | 11–12 | 169.000000 | -41.200000 |

Both radius precisions therefore choose exactly the same startup branches for
these cases. The town center at 0,0 is also excluded, by 91.2m. A remote inland
example at 1000,1000 fails the broad bounds immediately. In general, local
X outside [-345,345], Z outside [-343,343], altitude above 1000m, and enclosed
hall eyes remain excluded exactly as in the visible test. Starts elsewhere
retain lazy water resources; this is not a guarantee that a later journey to
a previously distant water body cannot incur its first allocation.

## Resource and history behavior

The preparation block runs before the ordinary frame clock and
`engine_perf_begin`, and it does not depend on benchmark mode. Reflection
allocation uses the saved internal target dimensions and reflection scale.
The reflected sky receives the saved cloud toggle; reflections disabled in
settings skip both reflection-target and reflected-sky preparation. The
opaque-water snapshot is still prepared nearby because it also supports water
refraction without reflections.

The calls have no temporal history feedback:

- `world_sky_draw` renders to its independent view-1 sky target and copies to
  the separate reflection FBO. It has no history input or temporal resolve.
- `world_water_scene_capture` blits the initialized main target into distinct,
  owned color/depth textures. It rejects source/destination aliasing and does
  not modify the source image.
- No temporal resolve, jitter advance, history publication, camera movement,
  weather update, or population update occurs in the preparation block.
  `temporal_cut` remains true and `temporal_sample` remains zero for the first
  real frame.
- The first real reflected pass clears and redraws the reflection target.
  When water becomes active, its ordinary opaque snapshot is refreshed before
  water sampling. Prepared placeholder pixels are therefore not accumulated
  into temporal history or sampled as an active stale water snapshot.

`glFinish` completes allocation/first use only during preparation; it is not
added to the walking loop. The block restores the main FBO, viewport, depth
test/write, culling, blending, RGBA mask, and active texture unit before play.
The existing resource owners still release the warmed targets on shutdown.
Failure sets the normal renderer initialization failure and retains those
owners for cleanup. Settings and authored scene data are not changed.

The `native-water-ready` full-route evidence now records frame392 at
**16.72926ms**, compared with the preceding `native-final` route's 93.71795ms.
Its world report records `startup_ready: true`, startup preparation 42.458ms,
first actual reflection at frame392, and **zero reflection-target,
reflected-sky-target, and opaque-water-snapshot allocations during play**.
These are stored results in
`native-water-ready/route-castle-west-street-final.json.csv` and
`native-water-ready/route-castle-west-street-final.json.world.json`; no rerun was
performed for this review. This confirms the observed first-turn spike was
absent in that completed run. It does not isolate the driver's individual
allocation costs or guarantee every future first-use path: preparation still
does not draw reflected terrain or foliage, so it does not prove all possible
first reflected shader-state specializations are warmed. The radius-only
correction is followed by separate final-executable route evidence.
