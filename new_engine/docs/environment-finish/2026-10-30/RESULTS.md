# Environment finish and graphics settings — 30 October 2026

This pass spends $0 and updates the source presentation in
`projects/valwood_visual`. The user deferred the replacement hero until
tomorrow. The current character and its animation geometry are retained.

## Environment

- Completed wood/stone PBR channels on window frames, sills, planting-bed edges
  and banner poles, using cached Poly Haven Brown Planks 08 and Cobblestone
  Floor 08. Normal maps use the OpenGL convention expected by glTF.
- Added broad, world-space paving variation and soil in captured stone
  recesses. The selected project's wall material receives irregular foundation
  weathering, restrained moss and vertical runoff.
- Increased photographed Forest Leaves 03 coverage on jungle shoulders while
  retaining the worn soil path.
- Added four low drainage rills along the courtyard wall and six small branch/
  pebble groups beside the jungle path. These additions have no collision.
  No walking-lane or terrain-height changes were made.
- The five material derivatives preserve every original geometry accessor
  byte. Superseded image pages are pruned. New material pages use 1K color and
  512px normal/ARM maps; the existing 2K terrain paving is retained.
- Authoring uses 8,166 of 8,192 scenery records. The new placements and material
  overrides remain editable. Owned and CC0 source archives are unchanged.

[Asset hashes and placement records](assets.json),
[project credits](../../../projects/valwood_visual/assets/environment_finish/CREDITS.txt).

Rebuild the material derivatives and placement layer with
`python3 tools/finish_environment_materials.py`. The selected-project shader
changes are in `projects/valwood_visual/shaders/`; the shared world shaders are
unchanged. The existing appearance is preserved in the earlier milestone
screenshots and source backups.

## Graphics and display

Open **Graphics and Display** from the title or pause menu, or use **F10**.

- Performance, Balanced, Cinematic and Custom quality selections.
- Windowed resolution selection and borderless fullscreen at the monitor's
  native desktop resolution. Unsupported oversized window choices are disabled;
  saved window dimensions are bounded when moving to a smaller display.
- VSync, automatic image scaling, target FPS, shadow resolution, water
  reflection quality, contact shadows, volumetric fog and clouds.
- Apply and save, Undo edits and Recommended controls.
- Version 2 preferences retain display mode, window dimensions and VSync.
  Existing version 1 project preferences migrate with their previous behavior.
  Automated checks use private preferences and saves.

For this RX 5300, retain the current 1600×900 window, Balanced-style effects
and automatic scaling. On a faster computer, choose **Cinematic**,
**Borderless fullscreen**, and disable **Automatic image quality** for full
native pixel resolution, 4096px shadows and full-resolution reflections.
Purchasing or selecting a quality setting does not add detail absent from a
source model.

## Verification

- CPU graphics settings and quality-controller suites pass, including 16 exact
  roundtrips, version 1 migration, invalid display fields, whole-file rejection,
  bounded dimensions and atomic-write behavior.
- SDL controller suite: 57 input/device checks plus ImGui navigation regressions
  pass. Native controller replay: 41 assertions pass for gameplay, menu focus,
  dialogue, combat, save/restore, defeat/retry, hot-plug and graphics navigation.
  The replay uses an SDL virtual controller; hands-on F310 operation is separate.
- Native courtyard and jungle captures pass on the RX 5300, with GL error 0,
  zero remaining texture references and no failed scenery streams.
- Native display cycle switches from 1280×720 windowed/VSync off to native
  2560×1440 borderless/VSync on, then 1600×900 windowed/VSync on. Three quality
  changes save successfully. The physical display test validates 1440p;
  a physical 4K monitor was not tested.
- CPU authoring validation passes. The five material derivatives were compared
  against their source geometry views after buffer repacking.
- The final native complete-journey rehearsal passes actual defeat/retry,
  directional combat, checkpoint save/load, persistent rewards and the ending:
  15 hits, 10 parries, 16 dust bursts and 670 weapon-trail segments, with no VFX
  failures, graphics errors or remaining texture references. Real user saves
  and source authoring remain unchanged.

[Display verification](display-verified/results.json),
[controller replay](controller-verified/results.json),
[courtyard capture](court-v5/court.png),
[jungle capture](jungle-verified/jungle.png),
[CPU settings log](graphics-unit.log),
[authoring validation](authoring-verified.json),
[final complete journey](journey-verified/results.json),
[graphics panel capture](display-verified/display.png).

Final executable SHA-256:
`ddfe420ea347efb917507117323a73bb402b9c6f18ee79f9e1fb766eb9b74ec6`.

## Walking performance

Measured sequentially on the RX 5300 with the older presentation closed,
1600×900 output and fixed 1120×630 internal resolution, VSync and all effects
enabled. No compilation, captures or encoding ran during these intervals.

| Camera | Route | Mean FPS | 1% low FPS | Worst frame | Below 45 FPS |
|---|---|---:|---:|---:|---:|
| Third | Courtyard | 59.95 | 59.12 | 17.21 ms | 0 / 880 |
| First | Courtyard | 59.95 | 58.68 | 17.32 ms | 0 / 880 |
| Third | Jungle | 55.44 | 41.16 | 26.33 ms | 137 / 1,980 |
| First | Jungle | 56.04 | 42.41 | 24.41 ms | 73 / 1,980 |

All four routes complete with zero collision/slope blocks and zero return
position error. The courtyard passes the strict 45 FPS floor. **The jungle
still fails that floor and the 45 FPS statistical target.** This art pass does
not resolve the existing jungle frame-pacing issue. Peak aggregate device VRAM
was 2,958,446,592 bytes, including the desktop.

[Walking measurements](walking-verified/results.json). These runs and the
controller replay used binary
`0930ab453192a33ac2f7b4e54f371a1e7f1d2ac8b5a4bbeae401e1bd90ad3e83`.
The final build changes only the zero-dimension window-size label/comment to
clarify that a live window keeps its current size. Art and render-loop behavior
are identical. Display and complete-journey checks run again on the final build.

The latest source build is reached through `Launch_Valwood_Presentation.sh`.
Older standalone packages retain their previous contents. No new standalone
package was created in this pass.

The updated presentation was reopened with Graphics and Display visible.
Its ordinary startup confirms the RX 5300 renderer, PulseAudio output and the
connected Logitech F310 Gamepad (XInput). See [session log](reopened.log).
