# Integrated demo pass — reviewed build

The new Linux review build combines a playable castle–jungle journey, actual NPC recall/experience, safe persistent progress, supported character motion improvements and a revised landscape. This is a working demo milestone, not completion of the entire AAA game.

Executable SHA-256: `ddb2674d5bde320fdc422c8a1405755745d300ba6d0a2dd341990258f0fd274f`.

## Implemented behavior

The master of arms and jungle steward recall public authored knowledge and recognize the player's visits. Nearby witnessed training outcomes change recognition. An explicit memory Goal tagged `duty` and `guard`, `work` or `rest` selects an already-authored physical destination; sleep/meals keep priority. Private memories never enter public dialogue. Authoritative quests/rewards remain separate from NPC claims. No language-model inference or network call is added.

The runtime records only committed meetings/choices and witnessed outcomes. It holds at most 32 minds and 256 aggregate experience records, with deterministic oldest-record eviction. Profile binding uses persistent scene IDs and checks any nonzero logical actor ID. The native memory inspector can bind a browser export and save portable embedded profiles in `ai/npc_minds.vwminds`. Authored data remains separate from player progress.

*A place in the watch* is an optional complete demo arc: start with the master of arms, win the existing three-round directional sparring test, collect the jungle steward's report, return and receive 25 coin plus one watch reputation. Dialogue conditions and counters prevent repeated rewards, including duplicate queued choices in one transaction. Sparring can be retried after defeat or completion. These rewards are saved demo progression; shopping, a full inventory, hostile-world combat and a campaign are not added.

J opens the journal. F5/F8 save/load player feet/camera/view, quest/reward/dialogue/pending-event state, NPC experience, physical prop poses/velocities/doors and authoritative living-world resources/schedules/debt. Save/load happens at explicit boundaries; active sparring must finish, and flying cannot be saved as walking. Atomic replacement, bounded codecs/checksums and system signatures reject malformed or incompatible state before commit. Editor Play rollback also restores NPC experience. This does not serialize an ongoing sword swing, migrate old saves automatically or replace the authored world's separate files.

## Landscape and motion

The pool's large smooth western mound was replaced by a narrower, lower spine, two offset spurs and an open saddle. Surveyed stone shelves support the original rigid bases at the new elevation. The shared CPU surface drives rendered terrain, movement, rain and prop support. The earlier eight small banks, water core, main route, camp and surveyed animal contacts remain protected. Changes reach 11.265 m relative to the old mound; the largest contour height is 14.462 m. The 1 mm continuity probe's maximum change is 3.546 mm.

A separate optional scenery file appends 36 measured placements in six vignettes: fallen wood, roots, rocks, ferns, sorrel and clover. It reuses owned models and images. All nine previous instance files retain their exact bytes, order and IDs. Admission checked rotated footprints, 5×5 dry support grids, rigid/trunk overlaps, walking/approach envelopes and camp/animal clearances. The final terrain comparison checked 197,729 grid points, 6,141 protected camp points and 6,475 support points across 259 rigid footprints; no support-relief regression exceeded the 0.16 m gate. See `landscape-support.json` and `saddle-stage/manifest.json`.

The existing shelter layer also carries broken mineral exposure. Path shoulders use photographic leaf/stone contrast as an edge-height proxy, plus stable soil/mineral color variation; this is material blending, not new geometric leaf displacement. The terrain changes add no texture allocation or terrain pass. The 36 prop/plant instances have their normal geometry/shading costs and add no new model files or texture images.

Mapped live knights have bounded world-space stance anchoring, eased plant/release, terrain IK, teleport reset and snapshot validation. This reduces sole sliding without changing movement routes or importing an unsupported rig. Existing breathing idles, acceleration/turn handling, directional attacks/guards/parries/reactions and owned first-person arms remain. Paused/unchanged combat pose packets now skip redundant instance uploads; four program-cache slots avoid churn. Faces, individual fingers, new hairstyles and social interaction animation still need compatible authored assets and rig/animation work.

## Verification

- 410 integrated quest/retry/reward/memory/private-recall/duty/persistence checks passed, including fresh-runtime resume, queued choice retention, corrupt-file rollback, Play rollback, portable bindings and bounded experience eviction.
- Live-component contracts passed, including 1,200 advancing full-bank memory frames with zero C++ allocations, plus the existing physical and gameplay heap checks.
- The memory editor/native suite retained version-one compatibility, physical-attribute round trips, 240 exact JS/C++ recall comparisons and 2,000 allocation-free full-bank recalls.
- Existing living/policy/gameplay/animation/Jolt/audio regression checks passed. Combat retained 216 owned poses, 333 prelinked batches, five attack families, 882 failure-injection cases and zero hot allocations; unchanged paused packets produce no repeated uploads.
- Exact-rig live animation tests passed stance displacement, eased contact, teleport reset, rollback, malformed-state rejection, full skin/culling bounds and existing captured-motion/idle contracts.
- 28 asset-inventory and 11 packaging contracts passed. The native project validator accepted the shipped project.
- Final Radeon journey QA passed actual E input/dialogue, the training host's completion handoff, jungle report and return reward, witnessed recognition, full save/resume and no repeat reward. Training result injection is used only for the host test; CPU combat tests exercise actual fighting rules. Final native run had GL0/ref0, no physics failures, no gameplay tick failures and no heap fallback. Source world/settings/authored AI files stayed unchanged.
- Five fresh final visual captures passed at saved quality: pool, understory, camp, castle arcade and tavern. The pool's former dome is visibly reduced; the scene still needs further art direction toward AAA production. Evidence: `art-reviewed/`.

## Final walking performance

Four frozen full return walks used the RX 5300, 1600×900 output and the saved 1120×630 internal resolution (70%). Automatic quality was disabled; reflections, contact shadows, volumetric fog/clouds and scenery settings were retained. No build, asset import, encode or competing native test ran during measurement. Each route excluded 120 warmup frames. The jungle route covers 432.633 m and the castle route 585.070 m; both completed without blocked steps or return error.

| Route | Camera | Average FPS | 1% low FPS | Sub-45 frames |
|---|---|---:|---:|---:|
| Jungle | Third | 57.18 | 46.37 | 6 / 7,580 |
| Castle–tavern | Third | 59.88 | 50.92 | 0 / 10,380 |
| Jungle | First | 57.62 | 45.93 | 18 / 7,580 |
| Castle–tavern | First | 59.91 | 47.63 | 5 / 10,380 |

All average/1% low gates, quality checks, route correctness and source freezes passed. The strict every-frame 45 FPS gate failed: 29 of 35,920 frames (0.081%) exceeded 22.222 ms, all isolated, with a worst frame of 24.345 ms. First-person jungle headroom is narrow. This is not a locked-floor or whole-world performance guarantee. Raw reports preserve every sample and failed floor gate under `routes-reviewed-third/` and `routes-reviewed-first/`; `performance-reviewed.json` is the final summary.

The earlier `871f…` candidate was tested before the pool re-authoring. Its four statistical gates passed, but a 45.486 ms castle presentation stall and a two-frame streak remain in the evidence. That is not the final build's measurement and was not deleted. Early native test failures are also retained: the first sent an injected E before streaming completed; the second completed the journey but the old QA terminal-state guard still expected phase 8. The corrected final run reached phase 20 and exited successfully.

## Portable package

This relocated Linux package contains the tested executable, selected runtime assets, this report, the guide and complete offline Memory Studio. See package-manifest.json for its exact content ID and file/byte counts. Run launch.sh to play or memory-studio.sh for the browser authoring tool. The source workspace also provides Launch_Valwood_Journey_Demo.sh. The package retains the README's Linux/OpenGL/system-library requirements.

## Remaining production work

The integrated demo is ready for review; the entire 100 km world is not authored or certified to this standard. Facial/finger animation, distinctive character silhouettes, broader terrain/settlement art direction, additional encounters/quests, voice/audio content and a complete campaign still need production work. More general scenery purchases are not required to test this build. A later character upgrade needs compatible facial/finger rigs and authored animation coverage, not simply more posed figures. The strict minimum frame-rate goal also remains unmet.
