# Opt-in native playtest controls check

`engine/world_playtest_input_qa.inc` defines
`playtest_input_workflow(uint64_t frame)`. The host enables it only for an
explicit playtest, bounded `--frames`, non-benchmark mode and
`VALWOOD_QA_PLAYTEST_INPUT`. The host also admits and filters its tagged SDL
events (`keysym.unused = 0x56414c54`) before ordinary event handling.

The sequence tests the actual SDL/ImGui paths for restart, both camera modes,
session save and restore, pause, default-focused Resume, blocked editor shortcut,
graphics settings and return to play. Enter is held for one frame and released
on the next. Save occurs in third person; the second V switches to first person,
then F8 must restore third person, position and progress. This proves a session
was restored, rather than checking only that a save file exists.

Expected completion is 10 passing cases and 20 tagged events for city/jungle.
Grandstone adds one E section-travel case (11 cases, 22 events). That case sets
the route completion index synthetically only to expose the E prompt, then
uses the real E event and supported destination load. Its log and
`playtest_input_synthetic_completion` explicitly distinguish control coverage
from the separately tested walking route.

The harness waits for destination loading and terrain readiness, fails with a
phase-specific reason and nonzero exit code, and has a 3,600-frame total guard.
Successful completion sets `playtest_input_done` and exits. The host must also
reject reaching its requested frame cap without that completion flag.

The production include passes a standalone syntax check using actual SDL and
route/session headers with `-std=c++17 -Wall -Wextra -Werror`.

The root-owned native run is complete in `native-input/results.json`, using
Linux and the AMD Radeon RX 5300 with binary SHA256
`0d336b887b8fb6be27d5ed14ccafae3d90572bd2e480ac0845d30c681d5ff138`.
City and jungle each passed all 10 cases/20 events. Grandstone passed all 11
cases/22 events and entered the first hall after the explicitly recorded
synthetic control-only completion. Source state and the binary remained
unchanged. These input runs do not certify walking-route completion or a frame
rate floor. Native Windows input/GPU acceptance remains pending.
