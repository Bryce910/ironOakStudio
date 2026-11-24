# Three playable environment slices — 2 November 2026

**The three environment slices passed their bounded Linux native walking and session checks and are ready for light playtesting:** city/market and countryside, approximately 853m; the expanded jungle loop, approximately 1,031m; and Grandstone, approximately 42m across two supported sections with an explicit transition. These are player-controlled environments with route guidance, collision, restart and separate sessions. This does not approve navigation throughout every imported scene or add new quests.

Start with [PLAYTEST_SLICES.md](/home/computer/Desktop/final_engine/PLAYTEST_SLICES.md), or use the direct local launchers:

- [City](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_City.sh): market stalls → west town edge → meadow → woodland → town return.
- [Jungle](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_Jungle.sh): pool bank → fallen shrine → canopy watch → eastern trail → jungle entry.
- [Grandstone](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_Grandstone.sh): short snowbound approach, then E/controller confirm at the final marker to enter the supported first hall. The transition bridges separate sectors; it does not establish a continuous gate-to-hall walk.

## Actual walking and controls

All intended areas are present in the completed native walking reports, with final source-state and binary guards true. Each route reached every node and visible milestone, with **zero blocked or slope-blocked steps**:

| Area | Actual distance | Completed coverage | Session save/restore checks |
|---|---:|---|---:|
| City |852.458759m|213/213 nodes;5/5 milestones;return to town|1|
| Jungle |1,031.013992m|28/28 nodes;5/5 milestones;closed loop|1|
| Grandstone |41.9m|Both supported sections;final hall7/7 nodes,3/3 milestones|2|

Evidence: [complete city/jungle walks](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/native-walk-trails/results.json), [corrected Grandstone walk](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/native-grandstone-final/results.json). The first Grandstone traversal exposed a real resident blocking the last approximately 2.5m of the original hall route. Its endpoint was moved to 31m, keeping the collision and residents, then the complete two-section route passed. The approach yaw now faces the gate. This is why the earlier static CPU checks and opening captures alone were insufficient.

All three Linux input workflows passed **31 cases**: city 10, jungle 10, Grandstone 11. The Windows executable also passed all 31 cases under Wine, with final source/binary guards true. They exercised restart, camera modes, save/restore, pause/resume, graphics, guarded editor access and Grandstone travel. The travel-control case deliberately sets completion synthetically to expose the real E prompt; the separate walks above establish traversal. Evidence: [Linux input](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/native-input/results.json), [Windows/Wine input](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-input/results.json), [harness scope](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/input-qa.md).

Three unsafe invocations—missing playtest guard, editor mode, and mismatched area—were correctly rejected. Local project preparation passed 9 CPU isolation tests; the route/session codec passed its strict parsing, ordered progress, height tolerance, stale-save and actual route fixtures. Original sources were fingerprinted before/after. See [isolation results](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/runtime-isolation.json), [local copies](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/local-copy-readiness.json) and [route tests](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/runtime-route-tests.json).

All three Windows opening/HUD checks passed **under Wine on this Linux host**, using the AMD GPU, with final source/binary guards true. Actual images show readable guidance, expected grounded starts and textured geometry, with no unexpected resource/physics failures. The final set includes the corrected Grandstone gate-facing camera. Grandstone retains optional HIGH refinement with zero failed/critical assets. These captures do not establish walking by themselves. Evidence: [final three-area openings](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-opening-final/results.json), [image review](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-opening/visual-review.md), [corrected Grandstone opening](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-grandstone-final/results.json).

The tested dedicated Linux runtime is SHA256 `0d336b887b8fb6be27d5ed14ccafae3d90572bd2e480ac0845d30c681d5ff138`; the Windows playtest executable is `24600deb1683fe5364ac0bbc919977db4303a5b08de6a39f5256bb75917a0a87`. [Runtime records](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/runtime-current.json) distinguish these from the preserved ordinary Linux launchers' binaries and the original orientation delivery.

## Performance observations

Linux native RX 5300, 1600×900 display, fixed 70% scene scale. These are whole-run samples including streaming, held save/restore checks and Grandstone travel; no capture was taken during walking. All dips remain included:

| Area | Mean FPS |1% low FPS|Worst frame|Frames below 45 FPS|
|---|---:|---:|---:|---:|
| City |59.95|54.28|81.56ms|8|
| Jungle |53.89|32.95|185.77ms|3,117|
| Grandstone |48.92|8.76|753.82ms|217|

These passes certify bounded route functionality, **not a locked 45 FPS floor**. Jungle and Grandstone retain meaningful dips; the mixed loading/session samples are not a pure steady-walking benchmark.

## Controls, isolation and delivery

WASD/right mouse walk/look; Shift sprint; Space jump; V camera; R restart; Escape pause; F10 graphics; F5 save/F8 restore the **separate playtest session**. Controller movement/look, sprint, jump, camera and pause/menu use the existing mapping; Grandstone uses E/A/Cross for section travel. Sessions preserve position, view, route progress and section, and reject changed route fingerprints. They do not overwrite the old quest checkpoint.

Each local copy has private settings, AI, shaders and saves. Large immutable banks are shared to avoid duplicating gigabytes; authoring and unrelated travel are guarded. Original project/art banks and ordinary Linux runtimes remain in place; only the dedicated opt-in build runs these slices. Structural editing belongs in a separate artist workspace.

The [final source audit](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/source-audit-final.json) passed: all 40 original state files and 203/202/200 guarded city/jungle/Grandstone source files were unchanged; runtime hashes, private directories and shared-bank targets match. This was not a full bank rehash. Only the three QA-generated playtest saves were [archived and cleared for first use](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/fresh-start.json); personal/source saves were untouched.

Windows delivery uses the **existing orientation base plus a separate add-on**, installed into a new playtest seed. `Valwood_Windows_Playtest_2026-11-02.zip` is **9,762,061 bytes**, SHA256 `62e0c30231974aef034160aad75805793438795e0bf8f68b56ad2a413de20c97`. The completed [final archive verification](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-overlay-verification.json) streamed all 31 add-on members, checked manifest/dependency closure and current patch bytes, and recorded zero overwritten original transport members. It reuses the already verified base; it did not re-extract or rehash the entire base archive.

The actual bundled Windows-Python installer passed [two tiny independent seed fixtures](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/windows-overlay-installer/results.json), including existing-destination/traversal rejection and unchanged base files. This did **not** extract and launch full 15–24GB production seeds. The base ZIP, old seeds and artist work are retained; selected installations still require their full materialized disk space. Follow the base-plus-add-on instructions in the root guide.

Use [PLAYTEST_CHECKLIST.csv](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/PLAYTEST_CHECKLIST.csv) on the tester's computer. Environment traversal does not relocate town stores, combat or quests, replace the father/knight hero, or complete the rest of Grandstone navigation. Jungle animals remain ambient. Native Windows hardware/input/audio acceptance and a locked 45 FPS floor are unproven; retain screenshots, settings and logs when reporting rough areas or stalls.
