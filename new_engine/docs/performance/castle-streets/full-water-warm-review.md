# Full water startup preparation: source review

Scope: read-only review of the shared startup/ordinary reflection and water paths in `engine/world_viewer.cpp`. This review ran no builds, tests, rendering jobs or benchmarks and makes no performance claim.

The current implementation uses the same `draw_water_reflection` and `draw_water_layers` helpers for ordinary gameplay and four hidden cardinal-direction startup passes. It exercises reflected sky, terrain, foliage, scenery, residents, other actors, flames, the opaque snapshot and ocean/creek water materials. Startup initializes shadow depth and supplies the normal directional-light matrix, torch-light data and explicit foliage wind-history inputs.

The four hidden passes are reported separately as `creek_water_preparation.startup_render_passes`. They do not increment walking-frame, first-reflection, play-allocation or profiling-query counters. Module-level cumulative draw counters, including population, sky and flames, may include startup submissions; those totals must not be described as measured-play-only counts.

Startup does not advance simulation time, actor poses, walking progress, temporal sample indices or temporal history. It restores the original scenery selection. Sky has no temporal feedback or mutable random state, and the first ordinary water/reflection frame overwrites the prepared targets before sampling them. The opaque snapshot remains a separate framebuffer, avoiding color/depth feedback. Preparation failure stops startup and leaves partially owned resources to the existing cleanup paths. Synchronization completes before the ordinary frame clock and profiler begin.

No concrete GL-state, feedback-loop, ownership or pixel-history defect was found in the reviewed helpers and their normal call sites. This is a source-level conclusion; it does not prove that every possible material/state variant has been warmed.

The earlier allocation-only preparation was insufficient evidence of a robust fix. Although one `native-water-ready` four-case batch reported **16.72926 ms at frame 392**, the subsequent isolated `native-water-ready-final` route still reported **95.39251 ms at frame 392**, with startup preparation successful and all three play-allocation counters zero. The repeated first-visibility stall therefore survived target allocation alone; the earlier fast observation cannot establish reliability.

The `native-full-water-warm` route-first run was still in progress when these notes were written. Its results require separate native validation; none are inferred here.
