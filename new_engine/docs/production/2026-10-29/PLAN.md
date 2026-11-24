# Production engine implementation

The requested destination is a custom, production-capable engine and design
workspace for Valwood. Retain the SDL/OpenGL runtime, purchased originals,
existing demo and saved graphics preferences. A working bounded first version
of a tool is not completion of the full commercial production system.

## Implementation sequence and verification

1. Connected authoring: persistent entity IDs, hierarchy, connected prefab
   capture/instantiate/transform, atomic document persistence and source repair.
   Prove a building with light, emitter and NPC route moves/duplicates/reloads
   coherently and rejects bad references/cycles/budget overflows atomically.
2. Recovery/testing: recoverable drafts, retained backup generations, explicit
   recovery/discard, safe Play/Stop document snapshots and clean-save handling.
   Simulate restart, corrupted/stale recovery and interrupted operations.
3. Project production: asset/dependency database, immutable revisions,
   deterministic package manifests and a project package that runs without
   the original workspace. Validate on a clean destination; do not include
   private archives or claim unsupported operating systems are packaged.
4. Finish the NPC/navigation, particle/water and shader-graph pass already
   underway in the other active chat. Coordinate file ownership before changing
   its Studio document, viewer, shaders and Makefile.
5. Region/layer authoring and coordinated asynchronous asset residency. Preserve
   bounded CPU, RAM, VRAM and GPU-upload work during ordinary traversal.
6. Production terrain/material/road/river workflows, full transforms and
   collision editing. Existing local sculpt/masks/splines remain available.
7. General skeletal animation import, retargeting, animation states/blends,
   events, sockets, root motion and terrain-aware character contacts.
8. Reusable gameplay/dialogue/encounter authoring and deterministic persistent
   living-world simulation, with bounded validated micro-model integration.
9. Spatial audio zones/mixing, cinematic timelines and coordinated reusable
   effect/lighting presets. Continue rendering quality and motion stability.
10. Integrated profiling/budget warnings, long editing/traversal/recovery tests,
    target-hardware matrix and repeatable build/package regression gates.

## Shared-file ownership

This chat initially implements new prefab/entity, recovery/session and
asset/package modules in separate files. Another active chat is implementing
the runtime editors in `world_studio.*`, `world_viewer.cpp`, runtime shaders and
the Makefile. Central integration must preserve that work. The user authorized coordination between the chats. Surface/shader/look work is reserved for the other chat; this production pass owns the remaining core tools and serializes central integration and native GPU checks.

## Performance and release rules

- Measure normal walking separately from explicit Apply/load/cook operations.
- Keep the user's chosen render scale/effects unchanged in paired tests.
- Record every post-warmup interval; retain failures and repeated runs.
- `tools/check_world_performance.py` defaults to zero frames over the requested
  45 FPS budget. Any relaxed allowance is explicit in its output. Its trace
  export contains measured counters, not fabricated CPU/GPU execution spans.
- Run builds/CPU/render fixtures before native timed routes. No other game,
  compiler, asset hash pass or expensive diagnostic runs during GPU timing.
- Commercial/AAA readiness requires reviewed tool workflows, visual quality,
  reproducible packages and sustained performance; new tabs alone do not pass.

## Current work

The frozen Studio v4 foundation build is compiled, CPU-tested and natively
verified. Connected prefab and Play/Stop workflows pass; the Linux package
verifies and launches after relocation. Eight production panels are connected.
The full commercial engine is unfinished. Both completed walking routes miss
the strict45 FPS floor; advanced animation/physics/gameplay runtime bindings
and automatic scenery residency remain open. See [measured results and remaining
work](RESULTS.md). The authorized surface/material chat now owns subsequent
shared-file and runtime-asset integration; it must rebuild and repeat validation.
