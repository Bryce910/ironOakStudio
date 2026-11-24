# WildMesh ambient animals

The supplied `100+MODELS+-+WildMesh3D.zip` is integrated as a first authored animal pass in Valwood's own renderer. The original archive and existing terrain/scenery assets are preserved. Eight realistic animal types are imported into three authored habitats.

## What is playable

- Farm: four chickens, three cows, two calves and three goats, outside the inner castle and inside the outer town walls. Five follow gentle circular walking routes; seven play source idle clips.
- Woodland: two bucks, two foxes and two wild boars in a dry clearing. These play genuine breathing/idle clips at fixed roots.
- Rainforest: two leopards, playing source idle clips on natural contour-aligned ground.
- Existing six cats and six dogs remain unchanged.

Open `Launch_Valwood_Animals.sh` from the `/home/computer/Desktop/final_engine` workspace folder to start beside the farm animals. In game, H opens the controls; **Scenic trails** includes **Wildlife | farm**, **Wildlife | woodland**, and **Wildlife | rainforest**. WASD moves; hold the right mouse button to look. These are ambient visual-demo actors, with no combat, hunting, feeding, or player interaction system added by this change.

## Assets and rendering

The source Blender rigs/actions were inspected with source auto-execution disabled. All 96 animation poses are genuine sampled skeletal actions, baked offline into the existing exact-topology GPU pose-blend contract. Eight walk and four coherent idle poses per type retain metric size, source coat UVs, shared albedo atlases, and compatible index/material/tangent-sign layouts. Each type also has static near/mid catalog GLBs. The selected source assets supply albedo rather than complete PBR texture sets; matte roughness is explicit, and fabricated normal maps are not used. Missing optional wildlife fur shells are omitted; the complete bodies remain.

Runtime uses absolute time and measured source foot-contact strides. Adjacent GPU pose interpolation runs independently of frame rate; there is no per-frame CPU skeleton deformation. Main, actual mirrored reflection, and shadow passes share the same actor snapshot but maintain independent fixed-capacity instance batches. Offscreen nearby shadow casters remain selected. Animal wind is disabled; existing grass/tree wind is unchanged. The last 16m of the 90m draw radius use the existing stable world-space coverage fade.

Only one habitat bank is resident at once. Its family poses/textures are loaded during startup or behind the already displayed destination-loading image; ordinary ready updates perform no model/image IO or allocations. Farm enter/exit distances are 150/175m; woodland 115/140m; rainforest 110/140m. Preload includes the full bank extent and a 12m guard ahead of the draw envelope. Leaving releases borrowed blend VAOs before source pose buffers/material references. Load failure leaves a completely empty bank and reports an error. The farm bank is beyond its exit distance from the market.

Resident budgets: farm 48 poses/four shared atlases 13.333 MiB; woodland 36 poses/three 512px atlases, 4 MiB; rainforest 12 poses/one 512px atlas, 1.333 MiB. These are animal texture budgets, not total renderer memory. The catalog contains 20 actors and has a compile-time 32 actor cap.

## Placement and preservation checks

`habitats.json` records actual pose bounds, conservative continuously blended sole-contact rectangles, all actor/camera coordinates, route speeds and margins against existing source meshes, all 24 village roads, crops, trails and water. Moving animals remain on the exact-flat farm terrace. Stationary wildlife faces along natural contours where needed. Runtime rechecks each moved root against water, source colliders and a yaw-oriented 3×3 sole rectangle, rejecting more than 50mm terrain spread. Authoring additionally samples the rectangle interior and complete circular routes with a 49mm limit. This sampled terrain check is not a formal analytic proof of continuous terrain.

Contact rectangles conservatively include six-endpoint triangle hulls over every adjacent pose pair, including wrap pairs. An independent analytic triangle/plane/interpolation audit verified all 96 pose pairs, with at least 10.032mm remaining contact-envelope margin. All 379 pre-existing model/scenery/ground files match the previous frozen production hashes; the four existing scene sources still contain 5,289 records, including 2,678 castle records.

## Verification and evidence

- `make test-project`: all three new scenic launch names accepted; existing invalid-input checks pass.
- `make test-world-wildlife`: 672 complete pose/create/target failure cases, bank replacement and hysteresis, 45/60/mixed absolute-time parity, measured foot rectangles, dry-ground/collision rejection, main/reflection/shadow separation, zero hot allocations/IO, suppressed unchanged uploads, and full ownership cleanup.
- `make test-world-wildlife-habitats`: nine validator regression tests and saved actual-scene revalidation.
- `python3 tools/audit_wildlife_contacts.py --output docs/performance/wildmesh/contact-envelope-audit.json`: independent continuous-contact audit.
- `make test-world-village-watch`: existing watch animation/ownership checks pass.
- Farm asset recook reproduced all 56 GLBs and the manifest exactly. Source/model previews and provenance live under `assets/purchased/wildmesh/{farm,wildlife}`.
- `final-captures/`: actual native 1600×900 screenshots of all three animal habitats, with frozen inputs, zero unsafe roots, zero GL/terrain/load failures and zero texture owners after shutdown.
- `captures/` retains the earlier farm inspection, whose source-freeze assertion correctly rejected an intermediate camera-file change. It is not final measurement evidence.

Native timing and farm-to-market resource-return evidence are recorded separately under `native/` and `return-check/`. See `final-verification.json` for measured frame times and whether each scene meets the 45 FPS mean and 1% low gate. A good mean alone does not establish smooth 45–60 FPS.

## Native results, final frozen build

1600×900, afternoon lighting, 60 FPS cap, 3072px shadows and nine soft-shadow taps, AMD Radeon RX 5300. Four sequential 30 second sweeps, 1680 measured frames each; no concurrent builds, cooks or engine sessions.

| Scene | Mean FPS | 1% low FPS | p99 ms |Worst ms |Frames within 45 FPS budget |
|---|---:|---:|---:|---:|---:|
|farm|58.83|47.17|20.902|21.681|100.000%|
|woodland|59.95|59.63|16.734|16.873|100.000%|
|rainforest|55.48|44.71|22.176|22.921|99.226%|
|market|59.95|59.59|16.735|16.911|100.000%|

Farm, woodland and market pass the mean/1% low 45 FPS gate. Rainforest 1% low 44.71 FPS misses it slightly;45–60 FPS is not locked throughout the world. The mean alone is not used to claim otherwise.

The farm-to-market integration run loads 48 poses then releases all of them. Final whole-scene texture ownership matches a fresh market exactly: 311 entries, 2154 references, 394536779 encoded-source bytes. The explicit destination transition holds the previous image for 120 staging frames; this is a travel/loading check, separate from the steady-state FPS runs. All eight final native capture/profile/return runs report zero GL errors, failed tiles, failed animal loads, unsafe roots and remaining texture owners after shutdown. Native source freezes and all 379 old asset/scenery/ground hashes match. Whole-device observed peak VRAM was 2840.01 MiB of 3056 MiB; this includes all GPU users and is not the incremental cost of the animals.
