# Landscape, surfaces and longsword demo pass — 28 October 2026

The current production build uses the supplied MC longsword FBX source pack in the custom Valwood engine. The seven-case native pass is recorded in `accepted/`. The subsequent first-person sleeve/camera correction changes only the arm assets and their placement, with a targeted native repeat in `final-review/`; its source delta is recorded explicitly. The earlier folders are development experiments and pre-promotion measurements.

## What changed

- Eight stronger banks and four framing ridges, up to 13 metres high, with shallow drainage cuts, four scanned rock-face landmarks, exposed roots, two additional fallen trunks, and planted transition pockets. The authored layer has 535 placements, reusing the existing owned meshes and images. The 216.316-metre jungle path, camp approach, pool, and surveyed wildlife contact patches remain protected.
- Brown photographed leaf litter, irregular dirt shoulders, damp runoff, rock exposures and moss connect the path, forest floor and banks. Rock exposure uses the previously unused alpha of the existing canopy field, without another texture allocation or terrain sample.
- The purchased archive was preserved and inventoried. Its 180 Unity-skeleton FBXs and three object FBXs were copied into `assets/purchased/longsword_mocap/source`; 13 distinct animation files supply the demo's 14 motion families. The duplicated UE5 set and macOS metadata were not needed for this importer. No source-file scripts were executed or source assets modified.
- The owned knight and helmet now use retargeted captured left/right/high/low attacks and a genuine thrust, five guard adaptations, hit/parry reactions, idle and walking. Full captured attack/reaction curves are sampled into the existing interpolated pose renderer. The new sword mesh reuses the owned sword's PBR atlas through a derivative UV transfer. Its silhouette is unchanged; angle-based bevel normals remove roughly 7,800 unnecessary split vertices from each body and arms pose.
- Both hands share a fixed hilt constraint. Stationary training stances include reach-corrected ankle planting. The cooked asset validator records the measured residual. These are baked constraints for the training floor, rather than runtime skeletal IK for arbitrary uneven terrain.
- First person uses actual textured forearms, hands and the purchased longsword, with its own depth buffer and the same HDR display pipeline. A separate shoulder-to-wrist reach adaptation preserves the rigid hands/weapon relationship while keeping the sleeve ends outside the first-person image; full-body poses are byte-identical to the accepted captured bank (`viewmodel-body-identity.json`). Sky/ground illumination follows world up when looking around. The renderer is warmed at the real scene resolution before play. Its HUD moves aside to keep the hands visible.
- The new opponent remains in the training lane before E, through bouts and after retries. Guard lowering joins the shared neutral pose before a swing; contact still occurs at the existing gameplay boundary. Facing turns are bounded, and gait starts/stops remain eased and distance-driven.

## Performance changes and settings

The baseline was GPU-bound: roughly 9.5 ms in main foliage versus about 1 ms of active CPU render submission, with very little editor cost. The final profile retains the fog, clouds, contact shadows, 2048 shadow map, reflections, vegetation density and authored scenery. It uses a fixed 70% scene scale: 1120 × 630 internally, displayed at 1600 × 900. This is an explicit sharpness tradeoff. F10 exposes the existing higher-quality settings. `graphics-before.settings` preserves the previous 85% setting.

The existing matching tree meshes now crossfade over 44–64 metres. Conservative six-plane culling removes tree instances outside the directional shadow volume using complete crown bounds and the existing four-metre guard. Main/reflection roots remain intact, with separate preallocated shadow instance streams. The fixed-time on/off screenshot comparison was pixel-identical (`shadow-cull-trial/pixel-parity.json`). That A/B test reduced the shadow pass from about 2.60 to 2.45 ms.

A full foliage depth prepass and synchronized presentation in the jungle were tested and rejected because they cost more time. The small forest shader specialization was also removed. Automatic render-scale changes produced allocation spikes, so this machine's demo profile holds its scale steady.

## Validation

- CPU checks cover all five attack angles, correct/wrong/timed guards, parries, stamina, anti-mashing, reach, dodging, three-round win/loss/retry and pause behavior at multiple update rates.
- Motion-bank checks exercise 216 pose loads, 333 prelinked batches, 882 atomic failure points, guard-to-attack continuity, gait stops, no hot allocations and no draw-time instance uploads.
- Independent GLB inspection verifies finite geometry, unit normals, fixed UV/index/tangent-sign topology, shared material images, actual movement in every action family, common attack endpoints and the original source hashes.
- GPU transform feedback verifies real purchased body and adapted arm pose interpolation, orthonormal tangent frames and matching color/reflection/shadow geometry. The origin-preserving viewmodel loader has a separate bounds regression.
- Landform checks cover 248,031 support probes and protected route/camp/pool/wildlife samples. The production atlas regression retains the map, 14 other foundations and climate exclusions; the jungle path's maximum measured grade is 0.03912.
- The camp native audit samples 120 seconds of campers and wildlife and all three approach lanes against actual terrain/collision support. Native runs must finish without GL errors or remaining texture references.

Final timed native runs are separate from screenshots. Every frame after the initial 120-frame warmup is retained in the CSVs. The reported 1% low is the reciprocal of the mean duration of the slowest 1% of those frames. Destination-tour timings include loading and are a functional travel check, not a walking FPS claim. A percentile passing 45 FPS does not mean every individual frame did so.

Native RX 5300 measurements at the settings above, with all post-warmup frames retained:

| Scene | Average FPS | 1% low FPS | Frames below 45 | Samples | Evidence |
| --- | ---: | ---: | ---: | ---: | --- |
| Jungle, third person, full return route | 58.84 | 45.57 | 11 | 7560 | clean-route-repeat/ |
| Jungle, first person, full return route | 59.45 | 46.47 | 18 | 7560 | accepted/ |
| Camp and approaches | 59.95 | 42.59 | 31 | 2280 | final-review/ |
| Rainforest pool | 59.95 | 52.79 | 0 | 1680 | accepted/ |
| Training, third person, three rounds | 59.95 | 50.66 | 2 | 2880 | accepted/ |
| Training, first person, three rounds | 59.95 | 49.96 | 2 | 2880 | final-review/ |
| Destination loading tour (functional only) | 58.79 | 17.21 | 10 | 2280 | accepted/ |

The seven-case pass completed with no GL errors, leaked texture references, blocked route steps or frozen-input/preference changes. Both combat views completed three rounds. The final viewmodel adjustment was independently validated across all 216 assets and repeated on the native GPU in first-person combat, the full jungle route and the camp. Its 111-file source delta is restricted to the executable, the placement source, the arm meshes and their manifest; all 108 body meshes are byte-identical. Unaffected earlier cases are identified in the Evidence column.

First-person final combat exercised all five directions, scored 13 hits and 11 parries, completed three rounds and retained first person throughout. It measured 59.95 FPS average / 49.96 FPS 1% low. The final clean third-person jungle replay completed the entire 432.633-metre return without collision or slope-blocked steps, at 58.84 FPS average / 45.57 FPS 1% low. It retained 11 isolated sub-45 intervals, including a 38.54 ms maximum.

All measurements are retained: the original seven-case jungle run measured 59.34 average / 50.75 FPS 1% low with no sub-45 intervals. The post-viewmodel run measured 59.26 / 45.02 with 35 sub-45 intervals; a roughly two-second asset-hash check ran in the background during that run. The subsequent `clean-route-repeat/` replay had no such background validation work and is used in the table. These repeats show that the minimum-frame target still has limited headroom. The supplied historical baseline was 49 average / 41 FPS 1% low; this is a new measurement, not a controlled comparison with that older run.

Five of the six walking/combat cases passed the average-and-1%-low 45 FPS statistical gate. The final camp run did not: its slowest 1% averaged 42.59 FPS, with 31 of 2,280 intervals below 45 FPS and a 24.46 ms maximum. Other cases also retained isolated sub-45 intervals despite passing the statistical gate. The build is available for visual and combat review, but an uninterrupted 45 FPS minimum across all areas remains unfulfilled.

The camp was also repeated with native GPU clock/memory sampling (`camp-diagnostics/`): 59.95 FPS average / 43.54 FPS 1% low. Named GPU stores total 2,211,283,233 bytes (about 2.06 GiB), excluding driver-private stores and other applications. GPU clocks reached the card's 1,750 MHz state during play. A 1024 shadow-map trial (`camp-shadow-1024/`) still produced a 42.83 FPS 1% low, so the sharper 2048 map was retained. No shadow-resolution reduction is included in the delivered settings.

The loading tour is functional evidence, excluded from walking performance acceptance. Its 17.21 FPS 1% low includes loading and must not be advertised as a walking result. Untimed visual captures in `captures/` are not used in the table. `training-first-guard.png` and `training-first-round-break.png` show the final adapted arm bank, with all 216 assets verified byte-identical to production. `training-first.png` retains the earlier framing check as development evidence.

## Try it

Use the normal Valwood training launcher. E starts training near the master of arms; V switches first/third person. Mouse flicks or arrow keys select the attack/guard angle; LMB swings, MMB thrusts, RMB guards, T selects thrust guard, Space quick-steps and Esc leaves training. F10 opens graphics settings. The destination menu and F9 demo stops provide access to the jungle, camp, castle streets and tavern.

This is a substantial demo pass, not a claim that the entire game has finished AAA production art, a complete animation graph, or runtime uneven-ground foot IK. The supplied assets cover the current landscape and training work; further asset buying should follow the next in-game review.
