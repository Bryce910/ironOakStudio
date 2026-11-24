# Runtime module review — 2026-10-29

Scope: runtime animation/physics module files, their tests, and the new `engine/world_studio_physics_view.inc`. No central viewer, build, project, or user scene files changed by this reviewer. Central integration is owned by the parent agent.

## Completed verification

- Animation CPU contracts passed a strict `-Wall -Wextra -Wpedantic -Werror` compile and AddressSanitizer/UndefinedBehaviorSanitizer execution. Leak detection disabled because this host's ptrace environment does not support it. The allocation-tracking test operators use noinline to avoid GCC's erroneous optimized mismatched-allocation warning for their intentional malloc/free overrides.
- Actual Jolt adapter CPU tests passed **899 contracts**, including gravity, physical contacts, sensors, doors, casts/rays, snapshot restore, partial-file rejection, atomic codec preservation, and owner-thread rejection. 900 ordinary frames with queries and 120 maximum-capacity frames with 512 bodies/32 sensors produced zero C++ allocations and zero Jolt heap fallback requests. Address/Undefined sanitizers instrumented the adapter/tests; the pinned Jolt archive remains its existing Release build.
- GPU renderer source/test compile and link successfully. Existing loading-only `gl_helpers.h` truncation warning is exempted from Werror. **Native GPU execution passed** on the AMD Radeon RX 5300. The first run exposed a test-only portability issue: the existing shared linear uploader requests legal unsized GL_RGBA, which this driver reports as that enum rather than GL_RGBA8. The test now verifies all four component widths are actually eight bits before accepting this equivalent spelling; sRGB checks remain strict. The corrected tiny hidden-context contract test passed. No process or user window was closed by this agent.

## Concrete fixes

1. Animation validation now rejects NaN tangent handedness, which previously passed an absolute-value comparison and could inject NaNs into the shader's bitangent. Regression added.
2. Repeated skeleton validation now checks duplicate palette membership with a fixed-size bitmap, replacing its quadratic scan. It stays allocation-free.
3. Physics v1 loading requires its gravity and all eight layer records. A truncated document previously inherited permissive collision masks silently. Two partial-file regressions now verify failure preserves the caller's old document.

## Integration contract

Detailed animation/render guides remain `/tmp/valwood_animation_integration.md` and `/tmp/valwood_animation_render_integration.md`.

Physics is an actual Jolt component, not a separate engine. Compile its adapter with `-DJPH_DOUBLE_PRECISION -DNDEBUG -fno-rtti -msse2 -mfpmath=sse -ffp-contract=off -pthread -Ithird_party/jolt`; link `build/physics/libJolt.a` after adapter objects. The ABI is verified at runtime. `World` must be created, configured, stepped, queried, restored, and destroyed on its owner thread. `configure(Document)` and `restore(Snapshot)` are loading boundaries with scene replacement; do not invoke them during ordinary frames. `step(dt)` runs at most four 60 Hz substeps and counts/drops excess whole-step debt. Contact events are bounded and expose overflow counters. `World::raycast/cast_capsule/sweep_body` return call validity; separately inspect `Hit.hit`.

Host work is still required before claiming finished runtime/editor features:

- Complete full-engine host integration. The new Physics tab now provides separate-file body/shape/layer/hinge editing, bounded eight-entry own Undo/Redo, explicit Jolt Configure/Reset/Run/Pause/60 Hz step, selected-body impulse/state, camera raycast, and bounded event/allocation/debt inspection. The real ImGui inspector passes 24 CPU-only contracts; central inclusion/build integration remains parent-owned.
- Bind physics body IDs to persistent entity IDs and render poses; coordinate Play/Stop snapshots with scene/session state. A default scene with hundreds of all-pairs physical contacts must be profiled; the maximum-capacity CPU fixture deliberately uses collision masks and sensors.
- Route character capsule motion/grounding and dynamic props/doors through physics; existing village collision/controller is not replaced by this standalone adapter.
- Link the new skeletal VS against production foliage color/shadow fragments, warm programs after link/epoch changes, then upload one committed palette per actor batch and reuse it unchanged in color/reflection/shadow passes.
- Use known host GL state when restoring bindings; `capture_bindings()` is loading/test only. Destroy renderers before texture-cache/context teardown.
- Bind imported skeletons and clips to actual NPC entities, author locomotion/combat states, retarget maps, root-motion collision, sockets, and terrain-relative foot IK. The current villagers are not replaced merely by building these libraries.
- Add dynamic animation bounds/LOD/culling and profile actual skinned crowds. Skinning motion vectors and a full animation-state-machine authoring UI remain unimplemented.
- The coordinated hidden-context GPU contract test has passed. Full native scene stability and frame-time routes remain necessary. CPU tests or this synthetic GPU test do not establish 45–60 FPS.

The GPU test binary is `/tmp/world_animation_review_render_test`; native execution requires `VALWOOD_TEST_NATIVE_GL=1` and the parent's released single-context window. It must not run concurrently with native frame-time measurements.

## Reproducible logs

See sibling `runtime-animation-cpu.log`, `runtime-physics-cpu.log`, `runtime-physics-inspector-cpu.log`, and `runtime-skeletal-native-gl.log`. CPU logs are fresh executions of the reviewed binaries; the GL log preserves the observed native process output.

## Physics inspector host hook

Include `engine/world_studio_physics_view.inc` inside Studio after the production helpers. Call `physics_panel(cam)` inside the tab bar; it handles tab/requested_tab 20 and services its own CPU preview while active. `physics_service(now_seconds, allow_work)` is also available if the host elects a global owner-thread service. Its document is deliberately separate from scene Play/Stop snapshots, and editing/running is held while the main scene is Playing or restoring. No collision overlay or scenery transform binding is implemented by this panel.
