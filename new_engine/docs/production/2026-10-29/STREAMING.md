# Live scenery residency

The live renderer now uses `world_regions::Coordinator` for real scenery model
ownership, eviction, worker decoding and incremental owner-thread uploads.

## Integration

Use `world_scenery_load_files_streamed(paths, count, models, camera_x, camera_z,
config, error, capacity)` at the loading boundary. The original eager loaders
remain available for import/QA compatibility. An existing eager scene can opt in
with `world_scenery_streaming(scene, true, config, error, capacity)`.

`world_scenery_update` automatically requests/evicts/restores the enabled scene.
No separate frame service is needed. Display
`world_scenery_streaming_stats(scene)` for resident/wanted/failed groups, shared
registered image bytes, geometry bytes, this-frame upload bytes, fallback use,
loads/evictions/cancellations and the latest admission/import error.

`world_scenery_prepare_files` suspends and joins the previous worker at the Apply
boundary, returns its committed model ownership to the old scene, and carries
the current camera/configuration into the prepared scene. It can reuse permanent
metadata when a near asset has never been GPU resident. Failed preparation leaves
the old scene and fallback geometry valid. Streaming resumes on the next update,
after the caller has restored its previous alias registry. Commit still transfers
borrowed model handles exactly once. Scene destruction joins the worker first.

Disabling streaming is a loading-boundary operation: it restores missing near
assets synchronously and then returns to eager ownership.

## Residency and visual quality

- Exact transformed source vertex bounds, grounded placements and authored
  colliders remain resident independently of model GPU resources.
- Streamed startup reads near-source position metadata without image decoding or
  allocating distant near/shadow GPU geometry.
- Mid/far and single-LOD assets remain pinned. High-detail near geometry is
  retained through its authored draw interval plus 384 m of unload hysteresis.
- Missing near assets are requested 256 m before their draw interval. Demand is
  the minimum distance to actual active placements, never a giant world-spanning
  alias AABB. A pinned mid mesh temporarily covers a missing near interval.
- Images retain their original resolution and PBR/facade data. Valid offline BC7
  cache blocks retain the existing opaque-color policy and source certificate;
  otherwise the worker creates full RGBA mip chains, averaging sRGB in linear
  light. No texture resizing or automatic visual-quality reduction is applied.
- Teleporting ahead of the preload interval can briefly expose the pinned mid
  fallback. Normal walking gives the worker substantial preload time.
- A source whose geometry bounds change behind the renderer is rejected on
  restore, preserving collision agreement until explicit reimport.

## Thread and resource limits

Default limits are one CPU worker, 512 MiB prepared CPU data, 192 MiB model
geometry, 1,536 MiB registered shared image storage and 2 MiB of owner uploads per
frame. The geometry budget is separate from the image budget so shared textures
are counted once. Initial/pinned geometry may exceed the configured geometry
budget; fallback quality is preserved and the counters expose that state.

`world_model_cpu::prepare` performs GLB parsing, transforms, basis generation,
image/BC7 reads, image decoding, mip generation and exact texture-key preparation
on the worker. `bounds_only` reads exact source positions without decoding images.
`world_model_stage` and `world_model_stage_step` consume prepared data with
incremental image rows and VBO/EBO slices. The owner path performs no image
decoding or source-key temporary-file writes. A prepared asset's cache-key tokens
belong to one upload transaction; prepare a new asset for another transaction.

Successful/cancelled CPU blobs are released on the worker after their owner GPU
transaction ends. Their bytes remain reserved until cleanup finishes. A loader
that violates its own reservation is rejected immediately. Upload calls enforce
the byte allowance, but GPU storage allocation is driver work and cannot promise
a fixed wall-clock duration from byte accounting alone.

Texture admission failure becomes an explicit failed/fallback state, rather than
permanently blocking the upload pipeline. It retries on re-entry or after a
meaningful release of registered texture memory. Meshes warm the known scene
programs before publication; ordinary draw keeps the existing cached uniforms.

## Runtime entity binding

`WorldSceneryPlacement` reports immutable merged index, source-file index,
source-record ordinal and source text line. Resolve explicit input-file/record
bindings once with `world_scenery_find_source_record`.

`world_scenery_runtime_active` suppresses/restores both static rendering and
collision without allocation. `world_scenery_runtime_pose` updates root XYZ/yaw
and all corresponding collision boxes without allocation. Full quaternion props
use the runtime renderer and suppress their original static record.

## Verification

CPU checks completed:

- Existing scenery parser/selection/collision suite plus actual coordinator
  eviction/worker restoration, streamed startup, Apply/commit, runtime suppression
  and moved-collider fixtures.
- Real GLB CPU preparation: 6,500 triangles, three complete images, exact bounds,
  complete mips, repeatability, budgets and cancellation.
- Texture-cache prepared-key sharing, legacy interoperability, ownership,
  registered GPU byte counters and full teardown.
- Coordinator owner/thread/reservation/cancellation contracts and 10,000 steady
  owner ticks without C++ allocations. ASan/UBSan passed for the coordinator and
  the real GLB CPU decoder.

`tests/world_model_stream_gl_test.cpp` is the real GL regression. Link it with
`world_models.cpp`, `world_model_cpu.cpp` and C-compiled `texture_cache.c`, using
`--wrap=stbi_load_from_memory`, `--wrap=fopen`, and `--wrap=pwrite`. It compares
legacy/shared/fresh rendered output, bounds, texture lifetime, <=64 KiB upload
steps, partial cancellation and teardown. The wrappers reject owner-stage image
decoding and file opens/source writes. Run with `VALWOOD_TEST_NATIVE_GL=1` in the
serialized native graphics test window.

The staged GL fixture passed on the native AMD Radeon RX 5300 on October 29,
2026. Legacy/shared-cache pixels were identical; fresh full-mip uploads also
rendered identically (maximum byte difference zero). Owner chunks stayed within
64 KiB; decoder/file-open/source-write wrappers, cancellation and final ownership
checks passed. See [native log](streaming-native-gl.log). The test exited normally
and closed its context. Timed routes still require the final integrated build;
this functional GL regression does not certify FPS.
