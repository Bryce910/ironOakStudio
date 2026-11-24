# Castle expansion verification

The castle now has a west training court, an east Defender monument, an armory terrace, two full-size guard barracks/quartermaster wings, and a creek outside the outer town walls with a lowered timber drawbridge. **116 appended placements** preserve the previous 2,321 castle objects: 63 training/statue/armory objects, one bridge, 16 wing placements and 36 bank details. The final district contains **2,437 objects / 5,048 combined scenery placements**, 55 residential homes, 128 townspeople, seven training actors and 12 pets.

## Implemented scene

- Six poleaxe pupils practise in pairs or at padded dummies while an instructor supervises. Genuine source attack motion and an authored recovery blend through 24 drill poses; four resident idle poses supply the instructor. Targets, weapon racks, benches, rails, livery and stores define the yard.
- A **6.98 m stone Defender monument** combines the actual resident character and poleaxe geometry with an authored pedestal and crest. Paved courts and paths connect it to the castle approach and armory.
- The barracks and quartermaster use full-size purchased building meshes. Their two paved approaches have measured source clearance; two supply items use actual five-ray mesh support.
- The creek lies outside the outer town walls, whose settlement boundary is approximately ±205 m. The south channel crosses the avenue at z=219 m, and the lowered bridge extends from z=212.5 m to the dry exterior approach at z=225 m. Water, local reflections, planted banks, stones and deadwood surround the separate bridge walking floor. The closed creek course remains outside the settlement; the entire enclosed town, inner castle courts, wing foundations and former inner-creek coordinates stay dry.

The additions reuse resident image and sampler policies: **zero new GPU texture policies**. They do add geometry and animation buffers. Castle inspection views retain the selected afternoon lighting look, 3072-pixel shadow maps and nine soft-shadow taps. Source attribution and animation adaptations are recorded in [CREDITS_CASTLE_TRAINING.txt](../../../projects/valwood_world/CREDITS_CASTLE_TRAINING.txt).

## Completed checks

[Exterior scene acceptance](scene-acceptance.json) verifies placement bytes, canonical preserved prefixes, source/model hashes, runtime aliases, generated paving/collision data, complete route clearance, and the new castle layers. The successful [exterior regression rerun](exterior-regression-rerun.log) includes **32 acceptance tests**, the retained castle authoring checks, expansion/wing/bank validation, training geometry proof and cache checks. The [final acceptance log](exterior-acceptance-tests.log) records the refreshed camera/metadata pass. Earlier logs retain failed intermediate attempts; the linked component reruns are the relevant results.

The **848.94 m castle return route** starts at the exterior bridge approach, crosses the outer town gate, walks the main avenue through town, visits the training, monument and armory loop, and returns across the drawbridge. Its 26 waypoints use **14,854 frames** including warmup and capture padding. The original 338.94 m inner-court loop remains within it. CPU acceptance checks **1,725 paved stations / 24,100 solid segments**, with **1.936 m minimum capsule clearance**. Both wing approaches retain **1.401 m minimum capsule clearance**. Native route completion and timing remain pending below.

[Exterior terrain checks](creek/exterior-terrain-unit.log) prove all **674,041 enclosed-town samples dry** and all 21 creek segment envelopes outside the walls. [Integrated atlas/stream checks](creek/exterior-atlas-stream-proof.log) cover **804,593 samples**, including 27,848 wet and 22,220 grass-masked samples. They also test 152,100 real terrain vertices across 36 tiles: 1,617 submerged vertices and 14 beneath the bridge. [Source support proof](creek/exterior-source-support-proof.json) checks **3,617,927 actual source-base rays across 55 models**, 43,191 road-width samples and 53,600 route samples, preserving the 2,400 non-bridge/non-bank records. [Bank checks](creek/exterior-bank-dressing-tests.json) cover all 36 details with 97,884 actual source-vertex terrain rays. The [actual-shader water check](exterior-water-check.log) rejects water in 125 dry enclosure/former-creek cases without relying on opaque depth, while all 22 exterior positive controls render water.

The training proof checks **461,616 actual vertices and 461,616 adjacent-pose vertex segments** against nearby prop envelopes, covering every GPU interpolation weight. [Training CPU checks](training-cpu-test.log) cover 600 seconds of absolute-time animation, zero update allocations, atomic load failures and color/reflection/shadow parity. [Visibility checks](visibility-cpu-test.log) cover 1,223,168 cached-camera/source-corner tests; [continuous source coverage](wall-visibility-source-proof.json) proves the 50 occlusion wall pieces have opaque, uninterrupted cores.

Specific successful assertions in [living-district regression tests](regression-tests.log) cover 600 seconds of the full crowd and pets, 292,608,000 human pairs and 55,297,536 human–animal pairs, plus actual GL interpolation/resource checks. Camera, project, village, ground and scenery checks also pass in [exterior component regressions](exterior-regressions.log); its later stale bank-pattern reader failure was corrected and the complete [targeted rerun](exterior-regression-rerun.log) passes. No blanket pass is claimed for intermediate logs that end with an unrelated failed component.

The hidden-HUD path also consumes queued ImGui input without running SDL desktop mouse/cursor/gamepad queries. Normal render submission remains in place. [Passive UI checks](passive-ui-checks.log) use the real ImGui lifecycle and counted backend mocks: 401 events, 414 balanced submissions, no hidden input backlog, released text/mouse capture and normal HUD resumption. This applies to ordinary interactive walking as well as timed runs. Earlier intermittent stall evidence is preserved in `native-before-passive-ui/`; it is not treated as a passing result.

## Final native measurements

All **eight native tests pass** at **1600×900**, capped at 60 FPS, on the AMD Radeon RX 5300. The full 128 townspeople, seven training actors, 12 pets, creek reflections, 3072-pixel shadows and nine shadow taps remain enabled. Exterior creek views correctly cull pets beyond their retained town routes; their full simulation and source inventory remain active.

| View or route | Mean FPS | 1% low FPS | p99 ms | Longest frame ms |
|---|---:|---:|---:|---:|
| Exterior drawbridge | 59.95 | 59.69 | 16.725 | 16.809 |
| Exterior creek | 59.95 | 59.77 | 16.720 | 16.769 |
| Training yard | 59.95 | 58.97 | 16.740 | 18.527 |
| Defender monument | 59.95 | 59.72 | 16.718 | 16.860 |
| Armory / wings | 59.95 | 59.78 | 16.716 | 16.791 |
| Retained hero street | 59.95 | 59.78 | 16.718 | 16.773 |
| 848.94 m castle return | 59.92 | 55.73 | 16.827 | 21.226 |
| 1,011.5 m district return | 59.81 | 49.45 | 18.127 | 145.636 |

Both complete routes returned exactly to their starts with **zero collision-blocked, slope-blocked or sliding steps**. All eight tests report zero GL errors, failed terrain tiles, travel-loading frames, unmanaged image uploads and remaining texture owners. Runtime/model hashes were frozen throughout and independently checked after the last run. [Machine-readable verification](verification.json), [final scene acceptance](scene-acceptance.json), [raw native results](native/results.json) and [run log](native-run.log) retain the measurements.

The district walk contains a brief hitch burst around frame 2660, including one **145.636 ms frame**. Its mean, 1% low and p99 still meet the configured 45 FPS acceptance threshold; this does **not** establish hitch-free rendering. The new castle return route stays below 22.22 ms throughout. The original spike samples are retained in [district CSV](native/demo-route-final.json.csv); they were not filtered from the result.

Whole-system Radeon memory at 1 Hz peaked at **2849.5/3056.0 MiB VRAM**, leaving **206.5 MiB** minimum sampled headroom. Peak GTT was **2320.2 MiB**. These include the desktop and other applications, and do not attribute usage or paging to the engine. [Driver samples](native/driver-memory.json).

The earlier failed profiles in `native-before-culling/`, `culling-probe/`, `native-exterior-first/` and `native-before-passive-ui/` remain comparisons. They do not replace the final measurements. The exterior-first run also exposed an obsolete acceptance assumption that pets must be visible from every camera; the exterior flank is outside their 220 m draw range, and the current check preserves full simulation/inventory requirements.

## Captured views and scope

These are unmodified **native RX 5300 captures**, taken separately from final performance timing. All five show the exterior-creek layout and current castle details; the drawbridge and creek use the final walking-height framing: [drawbridge](captures/drawbridge-view.png), [creek](captures/creek-view.png), [training court](captures/training-view.png), [Defender monument](captures/statue-view.png), and [armory/barracks backdrop](captures/armory-view.png).

The drawbridge is **static and lowered**; lifting interaction is not implemented. Training actors perform visual drills: player combat, damage, skill progression and XP are outside this demo. Purchased wing doors remain closed and their interiors are not walkable. The receiving hall retains its existing accessible interior. This pass does not certify AAA visual quality or performance throughout the larger world.

[Launch commands and controls](../../CASTLE_DEMO.md) · [Previous market verification](../market-hero/README.md)
