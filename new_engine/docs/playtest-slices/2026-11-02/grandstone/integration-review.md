# Grandstone playtest integration review

Read-only review on 2 November 2026, before the owner's final native runs. No runtime, binary, project, shader or purchased asset was changed during this review.

The integrated paths are suitable for the two certified walking sectors. This is approximately 42 m of guided source-floor walking across an explicit gate-to-hall transition, with bounded additional exploration. It does not certify continuous navigation through the whole castle, combat, quests or final visual/performance quality.

## Reviewed contracts

- `engine/world_viewer.cpp:598`: horizontal movement calls the active `Support::can_move` before legacy scenery, actor and runtime filters. The terrain-only slope sampler is bypassed for the source sector because actual stair risers are handled by the support service. Grounded movement adopts the checked source floor; airborne movement retains the body's feet reference.
- `engine/world_viewer.cpp:641`: vertical movement sweeps the same player capsule through source collision. Its radius 0.37 m and cylindrical half-height 0.50 m give a conservative 1.74 m capsule centred 0.90 m above the feet.
- `engine/world_viewer.cpp:2787`: a project-local origin marker plus route rejects an environment-unset invocation before project runtime loading. The opt-in mode additionally requires a matching project, presentation/play mode and a valid route; editor launches with the mode enabled are rejected. Grandstone support loads before initial camera placement. The installed route spawn and settings spawn match, and the settings loader validates actual floor and headroom.
- `engine/world_playtest_runtime.inc:50`: restore stages the selected sector and route, checks route fingerprint/progress, supported saved feet and capsule headroom, then commits. The separate `saves/playtest.progress` atomic write does not replace presentation progress. Reset and sector travel select the same tested grounded spawn.
- `engine/world_viewer.cpp:4740`: the third-person follow callback checks source/city capsule clearance only in playtest mode. The camera helper supplies a conservative enclosing capsule for each short sweep. First-person and third-person mode remain independent of the logical player position.
- `engine/world_presentation_runtime.inc:122` and `engine/world_playtest_runtime.inc:83`: playtest key filtering runs before the generic editor, flight, scenic-travel and atlas shortcuts. F11 is consumed; V and Space remain player controls. The controller's source-view shoulder shortcut is disabled in playtest mode.

## Checks repeated

- `/tmp/valwood-world-playtest-walk-test`: PASS. Actual Jolt checks cover overlapping storeys, reference heights during a jump, a real stair riser, walls, low headroom, a true unsupported hole, sector bounds and atomic malformed/unsafe-spawn rejection. Zero query heap fallback.
- `python3 -B -m unittest discover -s tests -p 'world_playtest_slice_test.py' -v`: 3 tests PASS, including actual city/jungle route parsing and ordered completion plus strict route/session handling.
- All six installed Grandstone files under `build/playtest-slices/grandstone/project` match `READY.json` byte sizes and SHA-256 digests. Total runtime data remains 6,777,818 bytes.
- Existing actual-source evidence remains 219 dense 5 cm approach steps and 620 hall steps, with real gate/column collision and unsupported-void/bounds negative checks. Their reports are `approach-cpu.json` and `hall-cpu.json`.

`engine/world_playtest_input_qa.inc` tests actual tagged SDL and ImGui control paths. Its Grandstone E-travel case explicitly sets completion synthetically to expose the control; it makes no walking-completion claim. The separate walking driver uses normal `move_camera`, ordered route progress, completion and saved-session replay. The external checker reports whole-run timings including streaming and held save/load checks, not a strict 45 FPS floor or AAA certification. Native results are owned by the final runner, not inferred from these CPU checks.

## Guard hardening reviewed

The previously reported environment-unset editor invocation is resolved in the reviewed source. `engine/world_viewer.cpp:2787` rejects a lightweight copy identified by `../local-origin.json` and `playtest.route` when its playtest environment is absent. At the end of `engine/world_playtest_runtime.inc`, startup now rejects a scene that opened Studio for repair instead of hiding that failure. `engine/world_viewer.cpp:4196` aborts the main loop if Studio opens while playtesting. These checks protect the copies' shared asset banks from authoring through these launch paths. This follow-up is code-review evidence; native runtime acceptance of the new guards remains pending with the owner's final runs.

## Narrow remaining caveat

Route-spawn headroom at section selection currently inherits the validated settings spawn's guarantee. The frozen route/settings spawns are identical and clear. If those files become independently editable, section selection should also sweep the route spawn directly instead of relying on that equality.
