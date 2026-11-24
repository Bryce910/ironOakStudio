Open [Launch Valwood Combat Arena](/home/computer/Desktop/final_engine/Launch_Valwood_Combat_Arena.sh). Choose **1–8 total opponents** and **1–8 simultaneous opponents**, up to your selected total, then select **Start fight**. The default is eight opponents, one at a time. The normal camera follows your player; it does not automatically frame an opponent. Lock onto a target only when you choose.

To adjust a count:

- **Controller:** highlight it with the left stick or D-pad, briefly press **A**, adjust with left/right, then briefly press **A** again.
- **Keyboard:** highlight it with the arrows, press **Enter**, type the number, then press **Enter** again.
- **Mouse:** click or drag the slider.

A complete victory earns **10 arena coins per opponent**: 10 for one opponent or 80 for eight. Your arena purse saves automatically, separately from story progress. Losing or ending a run earns no coins. If saving fails, use **Retry saving victory coins** on the results screen.

| Action | Controller | Keyboard / mouse |
| --- | --- | --- |
| Move | Left stick | WASD |
| Sword cut | Flick right stick in the swing direction | Select direction with arrows; left click |
| Three-cut combo | Center the stick, then make two fresh flicks during recovery | Make two fresh left clicks during recovery |
| Directional block | Hold LT and point right stick | Hold right mouse; select direction with arrows or mouse |
| Look around with sword | Hold LB and use right stick | Hold Q and move mouse |
| Optional target lock / release | RB | Middle mouse button |
| Sword / bow | D-pad left / right | 1 / 2 |
| Bow aim / steady | Right stick / hold LT | Mouse / hold right mouse |
| Draw and shoot | Hold RT, then release | Hold left mouse, then release |
| Dodge | B | Space |
| First / third person | Right-stick click | V |
| Pause / resume | Start / Menu | Escape |
| Navigate / select menus | Left stick or D-pad / A | Arrows / Enter or mouse |

Each sword cut costs **16 stamina**. Three cuts cost **48**, leaving **52 from a full 100** before regeneration or other actions. Make each new click or flick while the preceding cut recovers. Holding a direction does not repeat attacks. LT or right mouse cancels a queued cut. After looking with LB, center the right stick before releasing LB and attacking. After menus or pauses, release buttons and triggers and center both sticks.

Results offer **Fight again** and **Change opponents**. During a fight, **R** ends the run and opens opponent settings. Escape/B returns from results to setup. Open graphics settings through Pause or **F10**. Coins save automatically; fights restart as fresh runs.

**Arena status: accepted on Linux, camera-v2.** The current public launcher is bound to [the accepted arena camera build](/home/computer/Desktop/final_engine/new_engine/build/arena-camera/accepted-v2/bin/rpg). Its [native rehearsal](/home/computer/Desktop/final_engine/new_engine/docs/arena-camera/2026-11-05/native-v2/results.json) passed **60 SDL checks and 33 captured checkpoints**, including optional lock/release, normal player-centered framing, three-cut combos, physical arrows, eight independent attackers and exactly 90 saved coins. The tested RX 5300 sequence averaged about 60 FPS at 1280×720 and 80% render scale; this is not a locked-frame-rate guarantee. Physical Windows/controller hardware is not certified.

The [C21/runtime-v11 opening](/home/computer/Desktop/final_engine/Launch_Valwood_Opening.sh) includes the same three-cut story combat. Its [standalone Windows opening demo](/home/computer/Desktop/final_engine/new_engine/releases/Valwood-Demo-Windows-2026-11-05.zip) passed all 68 journey checks and ordinary launch/closure verification under Wine. Physical Windows testing remains to be done. The arena’s optional target lock is separate from the opening controls.