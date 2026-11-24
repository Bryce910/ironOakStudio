# Streamed-world authoring and demo pass

Implemented in the existing SDL/OpenGL engine. [Tool guide](../STREAMED_WORLD_STUDIO.md) covers the controls and current limits; [plan](PLAN.md) records the accepted scope. Source snapshots from before this pass are under `before/`.

## Delivered

- Actual-mesh/background picking, Ctrl multi-selection, XYZ move handles, yaw ring, uniform object scale/XZ group spacing, snap, duplicate/delete, named groups and nearby hierarchy. Draft wire bounds precede Apply; actual geometry/collision commit transactionally. History/load clears stale selection. X11 mouse reconciliation is covered by the real ImGui lifecycle test; passive gameplay makes no added global pointer query.
- Static embedded GLB import/reimport with immutable managed revisions, metres-per-source-unit conversion, per-LOD inspection, map roles/resolutions/color spaces, source normal scale, geometry and reference texture-cost diagnostics. Editable LOD distances and collision modes, plus actual selected-object LOD preview. Preserve-source collision retains authored doorways. Unsupported formats/rigs remain in their existing cooking/character workflows.
- Radiance HDR panorama import, rotation/intensity/blend, raw/filtered cube, SH9 diffuse, GGX specular and BRDF LUT integrated into sky/models/terrain/water with canopy/weather/local-probe shelter retained. Measured 256-side GPU resource allocation: **6,356,968 bytes**. No generated QA panorama is shipped as an artistic environment.
- Thirteen restrained building/plant/rock material calibrations and seven planted-pocket ground-integration patches, using measured existing placement manifests. No additional mesh/image resources, navigation/terrain changes or NPC/animal route changes. See [exact look document](candidate.world.studio) and [provenance](candidate.manifest.json). Reproduce with `tools/build_studio_demo.py --output /tmp/review-demo/world.studio`; existing outputs are protected.
- Surface projection now skips pixels outside the exact union of its active rotated boxes; fine grain fades with pixel footprint. Native rendered-pixel tests compare the production function with the unculled reference across rotated edges, empty/inside/outside/high-crown cases and retain identical output. This saves work without reducing fog, lighting, vegetation or render scale.

## Correctness evidence

Studio/document and transform CPU tests, bounded asset diagnostics, immutable cooker/unit/LOD/source-preservation tests, real ImGui active/passive lifecycle, native HDR/GL-state/ownership/energy, scenery/parser/collision/LOD/geometry-transfer, sky, sampler/material, lighting, post, weather and ecology contracts passed. Logs are saved beside this report. `cpu.log` retains the initial missing mock declarations; `ui.log` records the corrected passing lifecycle. The initial render log retains the sampler target's incompatible forced-software attempt; `render-final.log` records its native pass. Native opt-in is now respected by that Make target.

The [native UI workflow](native-interaction.log) verified a real mouse mesh click, X-handle drag/release and a single Undo restoring the original X position. It also verified two successful commits, two rejected edits (missing reimport and bad shader), geometry reuse, saved reload and unchanged source/preferences. [Counters](native/authoring-interaction.json.world.json) record one drag/finish/mesh hit/Undo and zero GL errors/remaining texture owners. The native test camera is stationary for the pointer test. The workflow deliberately includes Apply/loading pauses, including a roughly one-second rebuild; its aggregate frame rate is **not** a walking-performance result. A separate [HDR/editor reload](native/authoring-reload.json) measured 49.72 average / 45.80 1% low, with no frames below 45 in 480 measured samples.

## Native route performance

Hardware: AMD Radeon RX 5300. Output 1600×900, scene 1120×630 (70% saved render scale), 2048 shadows, half-resolution reflections, contact shading, fog and clouds enabled; auto quality off. Tests use the saved preferences, no quality override. Audio uses the dummy backend for repeatable rendering measurements. Frame-time-only capture avoids GPU timer-query overhead. The first 120 warmup frames are excluded; explicit editor/loading operations are separate tests.

All four routes completed and returned exactly, with zero collision blocks, correct camera mode, zero GL errors and zero remaining texture-cache owners. Raw CSVs and sidecars are under `routes/`.

| Route | Length | Average FPS | 1% low FPS | Frames below 45 | Worst frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| [Jungle first person](routes/jungle-full-first.png) | 432.63m | 59.39 | 50.62 | 2/7580 | 24.276ms |
| [Jungle third person](routes/jungle-full-third.png) | 432.63m | 59.18 | 50.15 | 0/7580 | 21.123ms |
| [Castle streets / tavern third person](routes/castle-streets-third.png) | 585.07m | 59.95 | 53.97 | 0/11880 | 21.954ms |
| [Castle arcade first person](routes/castle-arcade-first.png) | 216.41m | 59.95 | 48.55 | 1/3880 | 22.628ms |

Across 30,920 measured frames, 3 exceeded the 22.222 ms budget. The arcade miss was 22.628 ms. These are sampled route results, not a universal minimum-FPS guarantee.

The preliminary first-person look averaged 59.34 FPS but had 55 frames over 22.222 ms. The pixel-identical surface cull reduced this to 2/7580, with a 50.62 FPS 1% low. The two final first-person misses were 22.894 ms and 24.276 ms; one was the first measured frame after warmup. Thus these tests do **not** prove an absolute 45 FPS floor on every frame or under every desktop workload.

Saved graphics settings SHA256: `3a63846ab05e2c8c0e2bec898bac44a9402df8c78178af1413ceb415866fbf0c`.

Reproduce after building and closing the live engine:

```bash
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/validate_world_authoring.py --output docs/studio/local-authoring-check
env VALWOOD_TEST_NATIVE_GL=1 python3 tools/validate_world_studio.py --routes-only --demo-document projects/valwood_world/world.studio --output docs/studio/local-route-check
```

The authoring harness targets only its PID/executable-verified X11 window and cleans its generated model/HDR fixtures. It preserves the real document and settings. Route verification copies the candidate into a private `/tmp` document and does not save into the production project.

## Final visual review

Reviewed the native captures of the street, tavern and rainforest. They retain the warm tavern practicals and gray, misty jungle canopy. These are the tested saved-quality renders, not offline promotional renders.

| View | Average FPS | 1% low FPS | Frames below 45 |
| --- | ---: | ---: | ---: |
| [castle-finished-arcade](visual/castle-finished-arcade.png) | 59.97 | 54.15 | 0/300 |
| [castle-tavern-interior](visual/castle-tavern-interior.png) | 59.97 | 53.38 | 0/300 |
| [rainforest-purchased-understory](visual/rainforest-purchased-understory.png) | 57.32 | 50.66 | 0/300 |

Installed the exact tested candidate as `projects/valwood_world/world.studio` after verifying the file was absent. All nine source placement files match their earlier hashes ([proof](source-preservation.json)); graphics preferences are unchanged. Generated QA models/HDR were removed. The updated native Studio was reopened and verified at the castle arcade with PulseAudio and no test flags.

## Practical limits

This is a functional authoring milestone, not a claim that the game or editor is AAA/commercially complete. Gizmos support yaw/uniform scale, not pitch/roll/nonuniform or vertical group scaling. Picking ignores alpha texels. Static GLB import does not replace animation/FBX pipelines. Inspector costs are estimates; assets can exceed a 3 GB GPU's budget. Procedural patches are not arbitrary image decals. Local practical lights share 8 slots and add no local shadow maps. Moving architecture does not move fixed NPC/FX/paving attachments.

NPC/nav, particle/water/wind-volume editors, a shader graph, richer collision-shape authoring, broader import formats and distribution packaging remain future systems. No new asset purchase is required for this pass.
