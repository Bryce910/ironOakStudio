# Eight-challenger combat arena

Open [Launch_Valwood_Combat_Arena.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Combat_Arena.sh). The compact fenced practice ring opens its setup menu: choose 1–8 total opponents and how many fight simultaneously, then select **Start**. The default is eight knights one at a time. Waiting knights stay out of combat. Defeated knights collapse where they were struck and remain on the arena floor until restarting all eight. Health and stamina carry between opponents.

| Action | Keyboard / mouse | Controller |
| --- | --- | --- |
| Equip sword | **1** | **D-pad left** |
| Equip bow | **2** | **D-pad right** |
| Move | WASD | Left stick |
| Target lock / release | Middle mouse | **RB / R1** |
| Look around during sword combat | Hold Q + mouse | Hold LB / L1 + right stick |
| Sword swing | Left mouse; mouse motion/arrows select angle | Flick right stick left/right/up/down; center between swings |
| Sword block | Right mouse; match incoming angle | Hold **LT/L2** and point right stick in the block direction |
| Bow aim | Mouse; hold right mouse to steady | Right stick; hold **LT/L2** to steady |
| Bow shoot | Hold left mouse to draw, release to shoot | Hold **RT/R2** to draw, release to shoot |
| Dodge | Space | B / Circle |
| First / third person | V | Right-stick click |
| Pause / resume | Esc | Start / Options |
| Restart all eight | R | Pause → Restart all eight |

The normal camera stays centered on your character, including fights with several enemies. **RB / middle mouse** locks onto a living opponent in front of you; press again to release. The target stays fixed when other enemies move closer. A gold ring and LOCKED label identify it. The camera turns smoothly around you without shifting its center toward that enemy. Defeating the target releases the lock. Equipping the bow clears lock-on for manual aiming. The right stick retains directional sword attacks; hold LB / Q to look around. In the arena, RB and middle mouse select targets rather than thrusting; keyboard T selects thrust for the next left-mouse attack.

Sword up is a rising swing, down is an overhead swing. For blocks, stick up guards high and stick down guards low. Press LT late to parry; holding a block does not repeatedly parry. Weapon changes wait until the current swing finishes. Arrows have travel time and gravity; every active opponent can be hit. Target lock keeps the chosen opponent as the guard/HUD focus, while all active enemies keep fighting independently.

Body damage produces red blood, a solid impact sound and a **BODY HIT** damage cue. Weapon contact produces bright iron sparks, a metal ring and **STEEL BLOCK** or **PARRY / STEEL**. The opponent's current guard appears in the top panel. Missed swings do not produce either impact effect. The latest arena has quicker attacks, shorter recoveries and enemies who approach before winding up; center the stick between swings.

The current Linux arena uses three heavier **Volumetric Blood Fluids** mesh animations for sword and arrow body hits. Each hit produces two short directional sprays and dark falling droplets; finishing hits produce a larger burst. The source fluid finishes within one second, mist lasts at most 0.12 seconds, and airborne blood is clipped at the ground. Growing wet floor pools and purchased splatters stay at their ground impact locations between challengers. They freeze during pause and clear when restarting all eight or leaving the arena. The arena retains up to 128 marks; the oldest is replaced only if that budget is filled. Weapon blocks and parries keep their distinct iron sparks.

Pause, focus loss and weapon changes cancel a drawn bow. Release the trigger/button before drawing again. After victory or defeat, restart for a fresh set of eight. This practice arena has its own settings and does not award or save story progress.

The arena uses a separate runtime and private project. The city, jungle, Grandstone and apartment launchers keep their existing versions. The arena is a local Linux playtest; the previous Windows transport ZIPs do not include it.

The current Linux arena also uses five directional flesh-hit reactions, a 1.8-second authored collapse, weighted swing transitions, and 2K normal/material maps recovered from the owned knight sources. Defeats use animation rather than a physics ragdoll. These changes require no new asset purchase.
