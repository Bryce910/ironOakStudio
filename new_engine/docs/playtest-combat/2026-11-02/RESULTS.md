# Combat slice acceptance — 2026-11-02

The earlier playable delivery provided guided environment traversal and omitted the visible sword-equipped hero and combat expected of a gameplay vertical slice. This revision makes the existing armed hero visible, adds one dedicated repeatable practice opponent near each area start, and verifies real defeat, retry, victory and return to exploration. It is a combat-enabled environment playtest; wider story, asset and performance work remains.

## Player behavior

City and jungle open near their practice knights. Grandstone now opens in the supported first hall, where its challenge is placed; pause offers travel to the snowy gate approach and back. E/Enter starts a nearby challenge, restores third person and faces the opponent. Attacks, directional guard, parries, dodge, health/stamina, hit reactions, sound events, weapon trails and impact VFX use the existing combat system.

Victory and defeat display a focused result panel with retry and **Continue exploring**. R resets the current section, duel and route. Ambient residents/wildlife remain ambient. Practice duels grant no quest or equipment rewards. F5/F8 save and restore exploration state only; active fights and their countdown block both operations. The saved section, camera mode and route fingerprint are checked before restoration.

[Launch and controls](/home/computer/Desktop/final_engine/PLAYTEST_SLICES.md) · [Tester checklist](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/PLAYTEST_CHECKLIST.csv)

## Corrected hero and camera paths

Hero pose loading had been tied to village population, which is disabled for the city and imported stronghold. The player combat bank is now independently enabled in playtest mode, and its first-person weapon target is initialized even when the old training population is absent. Existing owned hero/sword assets and 216 pose assets are reused; this is not the deferred replacement modern-father/knight character.

Third person is the default at start, section travel and restart; V can switch views and saves restore the selected view. A source-collision camera query also collapsed its boom because float `.01f` lay below the physics API's double `.01` minimum. The shared camera capsule helper uses a `.02f` minimum. Actual Jolt tests verify open-space extension, wall contraction and both elevated Grandstone spawn cameras. See [camera CPU evidence](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/camera-cpu.json). Native captures additionally verify the visible hero, sword and opponent in the rendered scene.

## Accepted executable tests

| Run | Areas | Cases | Source-state guard | Executable guard |
| --- | --- | ---: | --- | --- |
| Linux sword combat | City, jungle, Grandstone | 30 | Passed | Passed |
| Linux camera, pause, save/load and section controls | City, jungle, Grandstone | 31 | Passed | Passed |
| Windows executable under Wine, sword combat | City, jungle, Grandstone | 30 | Passed | Passed |

The reports check more than a success flag: every area must complete defeat from real incoming hits, retry, damaging attacks, timed parries, victory, reset and the actual ImGui Continue button using tagged SDL input. No combat-health, damage or phase state was changed by the automated workflow. The result counts are retained before restart, rather than being erased when a new challenge starts.

| Linux area | Combat cases | Hits received before defeat | Winning hits / parries | Hero draw frames | Starts / defeats / wins |
| --- | ---: | ---: | ---: | ---: | ---: |
| City | 10 | 13 | 4 / 2 | 3,209 | 3 / 1 / 1 |
| Jungle | 10 | 13 | 4 / 2 | 3,204 | 3 / 1 / 1 |
| Grandstone | 10 | 13 | 4 / 2 | 3,210 | 3 / 1 / 1 |

Each Windows combat run also passed 10 cases, recorded 13 incoming hits before defeat, 4 damaging winning hits and 2 parries, and retained one defeat and one victory across three starts. Each accepted run loaded 216 owned pose assets with two combat actors, rendered the hero/opponent, produced weapon trails and impact/parry effects, and reported zero combat-VFX failures. Sound events were exercised with test audio output; this does not certify audible playback on another PC.

The 31 native control cases verify third-person startup/restart, both camera modes, saved camera restoration, actual pause/Resume UI, graphics return, editor shortcut blocking and Grandstone E section travel. That Grandstone control case uses an explicitly synthetic route-completion flag solely to enable the travel command; it is not new walking-completion evidence.

Evidence: [native combat](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/native-combat-final/results.json), [native controls](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/native-input-final/results.json), [Windows combat under Wine](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/windows-combat-final/results.json). The three complete reports retain all 40 protected original project/save/settings hashes and the executable hash throughout each run. Opening captures are stored alongside the combat reports as city.png, jungle.png and grandstone.png.

Accepted Linux executable SHA256: `2e131a4713f2f76af58cc4ec5fdc1787d57d58e133c940907d5b894ea49a1e2a`.

Accepted Windows executable SHA256: `02a7019fd2513f46c2dbf6c8159e8d4a487eb4309f91820692419a52d8e6c407`.

## Performance is unfinished

Test machine: AMD Radeon RX 5300, Linux radeonsi; 1600×900 output, fixed 70% scene scale, automatic resolution disabled. These whole combat runs include streaming and final capture work, rather than an isolated sustained performance benchmark. The low rates and stalls remain actionable; no 45 FPS floor is claimed.

| Area | Linux mean FPS | Linux 1% low FPS | Linux worst frame, ms | Windows-under-Wine mean FPS |
| --- | ---: | ---: | ---: | ---: |
| City | 59.91 | 47.32 | 97.12 | 59.98 |
| Jungle | 36.82 | 26.22 | 137.78 | 36.92 |
| Grandstone | 29.38 | 4.58 | 467.27 | 27.79 |

Jungle and Grandstone combat are below the intended frame-rate target on this machine. Earlier walking-only averages must not be used as the current combat result. The historical traversal pass completed about 852.46 m city, 1,031.01 m jungle and 41.9 m across both Grandstone sectors, with no blocked/slope route steps and successful saved-session replay. Those frozen [walking results](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/RESULTS.md) establish the earlier route checks; this combat revision did not repeat the complete long walks or certify navigation across the rest of the imported stronghold.

## Windows delivery

The new [combat add-on](/home/computer/Desktop/final_engine/Valwood_Windows_Combat_Playtest_2026-11-02.zip) is 9,781,433 bytes, SHA256 `12b31ff4e06226d608d997e31daa93499affe7d300196ca9df6844b0c5a19369`. It patches a fresh extracted copy of the original verified orientation transport and does not depend on the old traversal add-on. It reuses the existing large content blobs and installs a new independent playtest seed. The original orientation and traversal ZIPs remain historical artifacts.

[Final archive verification](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/windows-overlay-verification.json) passed: all 31 transport members were streamed and checked; all three project/database closures match their payloads; the exact accepted Windows runtime and current section data match the archive. Grandstone correctly binds its accepted section 1 profile to the seven-node hall route. The verifier reused the previously verified orientation base without extracting or rehashing its large payloads, and overwrote no original transport members.

The current Windows game executable passed the three-area combat workflow under Wine; a full production-seed installation on the designer's physical Windows computer and its driver/controller/audio remain separate acceptance steps. See the [packaging notes](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/windows-packaging.md) and [launch guide](/home/computer/Desktop/final_engine/PLAYTEST_SLICES.md).

## Remaining scope

The current hero is the existing armed prototype. The replacement father/knight character and modular equipment are deferred. City quests/vendors have not been relocated, and the practice fight does not imply those systems are integrated into this purchased city. Grandstone has two separately transitioned supported sectors, with the practice challenge in its first hall; the rest of the large scene is not fully walkable. Sustained combat performance, physical-controller feel and actual Windows-PC acceptance remain unfinished.
