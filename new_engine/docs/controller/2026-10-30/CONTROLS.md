# Vertical slice controller support

The source presentation launcher uses the rebuilt `new_engine/rpg`. An already running process keeps its older executable until the player closes and reopens it. The standalone packages have not been refreshed.

## Controls

| Action | Xbox / F310 XInput | PlayStation |
|---|---|---|
| Move / menu navigation | Left stick / D-pad | Left stick / D-pad |
| Look / combat angle | Right stick | Right stick |
| Interact / select menu or dialogue option | A | Cross |
| Jump / alternative combat swing | X | Square |
| Dodge / menu back | B | Circle |
| Swing | RT | R2 |
| Hold guard / time a parry | LT | L2 |
| Thrust | RB | R1 |
| Sprint | Hold LB or left stick click | Hold L1 or L3 |
| Journal | Y | Triangle |
| First/third person camera | Right stick click or View | R3 or Share |
| Pause / resume | Menu | Options |
| Showcase previous / next view | LB / RB | L1 / R1 |
| Portrait orbit / framing | Right stick / LT and RT | Right stick / L2 and R2 |

Save, restore, and Graphics and Display are selectable in the pause menu. Combat blocks saving, as before. The controller operates the player-facing title, pause/settings, journal, dialogue, retry, ending and visual showcase. Authoring tools retain their mouse/keyboard workflows.

## Behavior

Input ownership is contextual: controller confirmation never also starts a new journey, jumps, or interacts behind a menu. The active controller is shared with the ImGui navigation backend, and unrelated devices cannot activate gameplay buttons. The first valid option gets focus when opening a menu or a new dialogue line.

Movement uses a radial, rescaled deadzone and caps diagonal speed. Look sensitivity, vertical inversion, controller deadzone and vibration are available in pause settings. These preview preferences currently last for the running session. Combat vibration is optional and requests feedback only from the controller actively being used. Unsupported rumble is harmless.

Losing focus or unplugging the active controller pauses normal presentation play and releases guard/vibration. Reconnection can open another available mapped SDL controller. Triggers held across a menu or reconnection must return to neutral before attacking or guarding again.

## Verification

The automated rendered test drives an in-process SDL virtual Xbox-compatible controller through the production event, polling, UI and combat paths. It explicitly selects its own device even when physical controllers are connected, so it does not send haptics to physical hardware. It uses an isolated temporary journey save and fingerprints project authoring and graphics files. This does not validate a physical controller's sticks, wireless connection or motors.

Run `python3 tools/check_controller.py --output /tmp/valwood-controller-native` after building. Native runs require the desktop display/GPU. Run `make test-world-controller` for the unit checks. Unit tests are in `tests/world_controller_test.cpp`; rendered test assertions are in `engine/world_controller_qa.inc`. Test outcomes are recorded separately in RESULTS.md and native run directories.
