# Valwood opening prologue

Run [Launch_Valwood_Opening.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Opening.sh), then choose **Begin the Journey**. The route is **house → household bills → console → New Game → Meadows of Valois → waystone**. The father comes home exhausted from work and struggling with everyday costs; choosing escape through a video game starts the journey.

## Supplied example scene rebuild

The active opening recreates the purchased Finward **House Props & Furniture Vol. 2** demo arrangement in the native game. It uses the supplied connected rooms, building shell, furniture, decoration, resolved material assignments and source lighting. This supersedes the earlier pass that only fitted selected furnishings into our original apartment. The full display scene spans approximately 49.5 × 12 metres and contains 701 source prefab assemblies. Fifteen composed near/mid model pairs render the house and new story additions; the original 15 furniture assemblies / 30 near/mid GLBs remain available separately for editor import.

The source's four HDR lightmaps are sampled through the original lightmap UVs, transforms and directional maps. Movable doors, drawers and other unlightmapped pieces use exact source Unity light-probe samples. Interior reflections use room-bounded captures without the outdoor sky fallback. Original geometry, PBR textures and materials are retained. The missing-material background plane from the publisher's source scene is omitted; it is not part of the playable house floor.

The four-shot, sixteen-second opening frames the furnished room, household bills, television and playable handoff. The bills, existing console, animated television and four physical room switches are authored additions. The source arrangement is retained around them. The camera finishes at the player's exact standing view. Hold the skip control to enter gameplay sooner.

Read the bills on the small table, approach the console beside the television and interact. Release the interaction button before confirming **NEW GAME** with Enter or A/Cross. Escape or B/Circle steps away from the menu. Transport moves toward the TV and leads into the existing meadow reveal. Follow the path to the waystone. The front door does not trigger travel.

## Controls and checkpoints

| Action | Keyboard and mouse | Controller |
| --- | --- | --- |
| Move / look | WASD / mouse | Left / right stick |
| Sprint | Left Shift | LB/L1 or left-stick click |
| Jump | Space | X/Square |
| Interact / room switch | E while facing the target | A/Cross |
| Confirm TV New Game | Enter or click | A/Cross after releasing it |
| Cancel TV menu | Escape | B/Circle |
| Pause | Escape | Start/Options |
| Journal | J or Tab | Y/Triangle |
| Skip cinematic | Hold Enter or Space | Hold A/Cross |
| Graphics / display | F10 or pause menu | Pause menu |

Pause also includes sensitivity, deadzone, invert Y and reduced motion. Living room, kitchen, bedroom and bathroom each have a working wall switch. Their states are saved with story checkpoints. The source bake combines daylight and fixture illumination: switches approximate 30% retained daylight plus 70% electric bounce, while native fixture highlights use 10% of source intensity. This is not a separately baked daylight/electric lighting system.

F5 saves, F8 restores and R retries the supported authored checkpoint during gameplay. The new arrangement uses a separate private project and separate progress. Previous opening projects and their saves remain intact; earlier house checkpoints are not migrated into different room geometry. Existing graphics preferences are retained when activating this revision.

## Verification and remaining work

Active runtime and project: `new_engine/build/opening-finward-reference/bin/rpg` and `new_engine/build/opening-finward-reference/v11/project`. Both the [Linux journey](/home/computer/Desktop/final_engine/new_engine/docs/opening-finward-reference/2026-11-03/native-v11/results.json) and [Windows/Wine journey](/home/computer/Desktop/final_engine/new_engine/docs/opening-finward-reference/2026-11-03/wine-v11-paced/results.json) passed all 40 functional checks at 1600 × 900. Each run walked 176.9 metres through room switches, bills, console/menu cancel and reopen, New Game, transport, meadow movement, waystone, save/load and retry using actual input. The regular Linux launcher now opens this revision, with startup verified separately from QA. Final evidence is recorded in [FINAL_REVIEW.json](/home/computer/Desktop/final_engine/new_engine/docs/opening-finward-reference/2026-11-03/FINAL_REVIEW.json).

Production collision preserves thirteen static bodies from source building/furniture colliders, with tiny isolated decorations omitted from collision, plus one enclosure body. The publisher’s open display edges are closed along the stepped floor outline with source wall/floor/ceiling materials. This addition keeps source furnishings in place and uses approximately interpolated source probe lighting. Room-switch routes, physical wall rejection, story paths, all model loads and cinematic sightlines are checked separately. Automated controller checks use an SDL virtual pad; physical controller feel and native Windows hardware still need a receiving-PC rehearsal. Functional acceptance and whole-route timings do not certify a fixed minimum frame rate.

The father model, embodied hand interactions, acted dialogue, armor transformation and subsequent combat encounters remain separate work. The prologue is a first-person preview; the other gameplay previews retain their first/third-person controls. The supplied house now establishes the environment reference, rather than completing every aspect of the game's visual presentation.

The [Windows r4 add-on](/home/computer/Desktop/final_engine/Valwood_Windows_Opening_Finward_2026-11-03_r4.zip) accompanies the frozen reference build. Install it beside a fresh extracted original Windows Orientation bundle into a **new seed folder**; keep earlier installations and saves in place. The [Windows package guide](/home/computer/Desktop/final_engine/new_engine/docs/opening-finward-reference/2026-11-03/windows/OPENING_GUIDE_r4.md) contains the exact steps. No Unity installation is required to run the cooked game. The [SHA-256 file](/home/computer/Desktop/final_engine/Valwood_Windows_Opening_Finward_2026-11-03_r4.zip.sha256) identifies the tested add-on.

Purchased sources and license records remain in the private asset library. Finward content uses its Unity Asset Store license, separately from earlier Poly Haven CC0 sources. Older art/provenance records retained in the project are explicitly historical. The previous furniture-only version's [final review](/home/computer/Desktop/final_engine/new_engine/docs/opening-finward/2026-11-03/FINAL_REVIEW.json) remains available for comparison.
