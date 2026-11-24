# Cinematic rendering and frame-pacing pass

## Review findings and order of work

1. Measure on the actual Radeon RX 5300 (3 GiB), using repeatable arrival and moving-camera/editor runs. Preserve baseline captures and report frame percentiles, startup time and resource reuse separately.
2. Share immutable static-model GPU resources across placed objects and forest LOD instances. Preserve instance transforms, materials and collision; retain an eviction policy and verify ownership through repeated load/delete tests.
3. Replace the static Lambert material path with normal/roughness/metallic/emissive-aware PBR, correct alpha-tested shadows, and coherent atmospheric lighting. Review timer-query nesting so GPU measurements remain valid.
4. Spread grass regeneration across frames, move thumbnail decoding off the render thread, and avoid redundant editor and reflection/post-processing work. Measure frame spikes during camera travel and editing.
5. Improve terrain materials, vegetation composition and directional/sky lighting using actual gameplay comparisons. Keep asset provenance, memory budgets and the remaining AAA production gap explicit.
6. Run ownership, authoring persistence, import, shader compilation and native-GPU rendering checks. Reopen the improved starting area for review.

## Acceptance

Use the same native resolution and camera route for comparisons. No material quality downgrade to manufacture a speedup. Report average, median and p95/p99 frame times, cache reuse and startup costs; preserve captures. The result must retain model importing, scene editing and save/reload behavior. Full commercial readiness and AAA production fidelity require continued asset and renderer work beyond this pass.

## Delivered pass

Implemented shared static glTF resources; PBR object/terrain shading and triplanar cliffs; cutout and correctly projected wide shadows; terrain LOD selection, frustum culling, front-to-back drawing and seam stitching; terrain GPU-buffer cleanup; staged grass builds; bounded async thumbnail decoding; and reproducible native profiling. Added cooked CC0 fir/fern/rock variants, photographic terrain maps and backed-up scene dressing.

The native 1600×900 play/editor runs remain near the 60 Hz cap with substantially more vegetation. Startup is about nine seconds versus nineteen in the baseline. Rare 30–34 ms frames remain; a further long-session/uncapped streaming pass is required before claiming hitch-free operation. See `performance/RESULTS.md` for all metrics and qualifications. AAA visual fidelity remains a production milestone, not a completed claim.

## Grass follow-up

Rebuilt the grass scatter and shading in the custom renderer after reviewing player-height native captures. Removed the excessive meadow-height multipliers and duplicate blade taper; introduced stable irregular clumps, shorter curved leaves, bent/folded normals, restrained material variation, graded distance density and shared terrain cover. Kept paths, prop clearings, water rejection, authored grass/flower masks and interactive bending. Fixed the directional cutoff at the old blade cap; high-density fields now sample the whole circle. Empty distant cells skip world queries, and terrain coverage uploads reuse texture storage.

The profiling investigation also found an older play process still occupying the GPU. Closed that stale instance, added a launcher lock and rejected benchmarks while a launcher-managed game is open. Final isolated native checks average roughly 60 FPS; short frame spikes and the remaining AAA art/renderer gap are documented in `performance/RESULTS.md`.
