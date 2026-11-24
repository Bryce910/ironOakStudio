# Visual consistency and character polish — 28 October 2026

Completed the requested polish pass in the existing custom engine. This is a demo-review build, not a claim of finished AAA fidelity or a locked every-frame 45 FPS floor.

## Changes

- Fixed an opaque-material lighting composition bug: directional sky selection no longer replaces forest canopy shelter. Rocks and NPCs now retain the same incident canopy attenuation as foliage. No added texture fetch, shadow pass or sampler.
- Re-authored arcade footing plants and jungle trail-edge pockets with independent deterministic variation in position, yaw and scale. Strict clearance checks rejected unsafe candidates; the final layer has 504 placements, 12 fewer than before, and 7,746 total instances. The other eight instance source files are byte-identical.
- Added conservative render-root sole support curves baked from all 95 actual affine pose pairs. Worst uncorrected geometry hover was 38.55 mm; independent 257-point checks per pair found residual hover up to 1.364 mm with no over-subtraction beyond 1 micrometre tolerance. This is pose support compensation, not skeletal foot IK. Runtime uses a bounded table lookup; navigation heights, paths, source meshes, shadow/reflection parity and original 128 pose phases remain intact. The fastest measured 60 Hz root correction is 16.454 mm during a short opposite-foot handover.
- Appended standing neighbors breathe at slower, varied tempos; close-up route stops use restrained neutral-to-breath gestures. The two western neighbors take 1.65 seconds to pause/turn rather than 1.15 seconds. Bartender counter-facing turns are retained.
- Removed the stationary camp wayfarer’s whole-body swivel, preserving its foot roots while the existing source idle provides upper-body motion.
- Existing separated crowd routes remain the avoidance mechanism. 600 seconds at 60 Hz passed 350,280,000 pair tests and 5,040,000 support/collision probes, with at least 1.4 m overall separation and 1.6 m in the paired districts. No adaptive steering or player-yield AI was introduced.

## Validation

CPU and actual-GL population tests passed, including source mesh contracts, original 128 motion golden snapshots, animation continuity, reflected/depth parity and conservative frustum culling. Camp 600-second grounding/continuity/ownership tests passed. The canopy GLSL test now exercises actual production main-path composition for opaque, foliage, grass, directional and non-directional cases; 232 cases pass, plus both full fragment compilation variants. Native AMD lighting fixtures and final capture review passed. Canonical authoring/terrain/support proofs passed without relaxing renderer guards.

An initial native startup correctly rejected over-buried small sorrel. The authoring tool now checks model-height burial limits; trail burial was corrected to 8 mm and acceptance rerun. Failed attempt evidence remains beside final logs.

## Native performance

AMD RX 5300, 3 GB; 1600×900 output, fixed 1360×765 scene, manual 60 FPS target. 2K shadows, half-resolution reflections, contact shadows, soft shadows, fog/clouds and weather remain enabled. All post-warmup intervals retained and independently recomputed; no build/cook/hash work during timing.

| Test | Average FPS | Slowest 1% FPS | Worst frame ms | Frames below 45 |
|---|---:|---:|---:|---:|
| final-jungle / route | 55.93 | 45.83 | 23.332 | 7 |
| final-camp / clearing | 59.49 | 51.45 | 21.552 | 0 |
| final-camp / approach-return | 58.49 | 48.33 | 21.609 | 0 |
| final-castle / castle-west-street | 59.95 | 53.78 | 23.370 | 2 |
| final-castle / castle-finished-arcade | 59.95 | 53.75 | 20.934 | 0 |
| final-castle / castle-tavern-interior | 59.95 | 54.82 | 18.706 | 0 |

Average and slowest 1% gates passed; see individual-frame counts for remaining hitches. The nine-stop cross-region integration passed with retained lighting caches, camp/animal support and clean GL/resource diagnostics.

## Review

WASD moves; right mouse looks; F9 advances showcase stops; F10 opens graphics settings. Native final captures are in visual-review-v3. Interactive launch details and real-audio verification are recorded in interactive-demo.json. The previously exported jungle video predates this build.
