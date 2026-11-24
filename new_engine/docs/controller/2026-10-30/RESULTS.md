# Controller support — 30 October 2026

The source vertical slice now supports controller-only player menus and the presentation route. The presentation was reopened with the tested build after the user approved restarting it. Older standalone packages retain their previous code.

## Delivered

- Context-owned title, pause/settings, journal, dialogue, retry and ending navigation. First-option focus stays visible and responds on the first confirm press after gameplay/showcase transitions.
- Left-stick movement, right-stick camera/combat angle, sprint, jump, swing, guard/parry, thrust, dodge, interaction, camera switch and journal access.
- Reachable save/load and Graphics and Display controls, with visible scrolling on smaller displays.
- Showcase shoulder navigation, controller portrait orbit and trigger framing.
- Xbox/PlayStation button names, automatic input-hint switching, controller deadzone, sensitivity, inversion and optional supported combat vibration.
- Safe focus loss and hot-plug/disconnect handling. Gameplay direction commands do not become held ImGui keyboard keys when entering a menu. The active controller is shared by gameplay and UI.
- Legacy input-runtime instance-ID matching and cached-button cleanup on disconnect/reconnect.

[Control mapping and scope](CONTROLS.md).

## Verification

- `make -j2 rpg`: passed; final build log contains no warnings or errors.
- `make test-world-controller`: **57 SDL device/input checks**, plus actual Dear ImGui focus/confirmation and label-preservation regressions, passed.
- Native unit checks with a physical **Logitech F310 Gamepad (XInput)** connected: passed. The tests send rumble only to their own virtual controller; the legacy reconnect test also verifies safe fallback to the detected F310.
- Existing presentation CPU suite: **209 checks passed**, covering progression, combat, rewards, defeat/retry, save/resume and the purchased rig.
- Final rendered replay on the RX 5300: **41 assertions passed** through the production SDL virtual controller, event routing, polling, UI and combat paths. It covers focused title selection, three showcase views, portrait orbit/framing, forward movement/sprint/jump, journal, paused movement, controller-selected save/restore, two dialogue branches, attack angle, swing/guard/thrust/dodge, combat pause, unplug/reconnect, actual incoming-attack defeat and retry, vibration and scrolling into graphics settings.
- Final replay: **GL error 0**, **0 remaining texture references** at teardown. Authoring, graphics settings and both real save files stayed unchanged. The isolated temporary progress file was 4854 bytes.

Binary SHA-256: `64be788914e27ac7ba42c809cd28070f1400f7553f90f70e31660c86cf5051c9`.

[Final structured results](verified/results.json), [native log](verified/native.log), [controller UI capture](verified/controller.png), [unit log](controller-target.log).

## Practical limits

The rendered test uses an SDL virtual Xbox-compatible controller. The F310 is detected by the native SDL runtime, but hands-on stick/button operation, connection stability and physical motors are not certified by these tests. PlayStation button labels are covered by unit checks; a physical PlayStation controller was not exercised. Preview look/deadzone/vibration preferences currently last for the running session. Editor authoring retains its keyboard/mouse workflow.

The reduced-resolution rendered replay ran alongside the user's existing presentation session. Its frame profile is a functional test, not a sustained frame-pacing benchmark. No asset, graphics-quality or standalone-package update was part of this controller pass.

The user explicitly approved closing and reopening the original demo after being informed of possible unsaved progress loss. The presentation was restarted through `Launch_Valwood_Presentation.sh`; its startup log confirms scene initialization, PulseAudio output and the connected Logitech F310 Gamepad (XInput). See [reopened session log](reopened.log).
