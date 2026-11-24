# Legends of Valwood — A Place in the Watch

Launch `Launch_Valwood_Presentation.sh` from the enclosing workspace. The packaged version also runs through its own `launch.sh`. This preview is prepared for this Linux computer at 1600 × 900. Headphones are recommended.

## The journey

1. Select **Begin journey**. Follow the gold marker to Master Corvin in the west court. Approach and press **E**; choose the first conversation response.
2. Win the three-round watch trial. Flick the mouse or use arrow keys to choose an angle. Left mouse swings; right mouse guards; middle mouse thrusts; Space dodges. Match the incoming direction and raise guard just before impact for a parry. Answer during an opening. Story assist is enabled for the presentation and can be changed under Pause.
3. After the trial, press **E** to follow the south trail. The chapter transition carries the journey to the Emerald Wood. Walk along the worn path toward the gold marker and the wayfarers' clearing.
4. At the crossing, press **E** to challenge the oathbreaker. This is an actual one-opponent directional encounter with health, stamina, parries, victory and defeat. The report becomes available after victory.
5. Find Warden Rowan at the clearing; press **E**, then choose **Take Rowan's sealed report**.
6. Press **E** to return to Valdoa. Speak to Corvin and deliver the report. He recognizes the completed trial. Take the watch oath to reach the ending and earn 25 coin, one reputation and Watch endurance.

Watch endurance increases sprint speed by 15% and unguarded stamina recovery from 22 to 29 points per second during repeat encounters. The reward and NPC experiences persist. The ending offers free exploration, the Copper Lantern tavern, or a new journey.

## Controls

WASD moves; hold right mouse to look; Shift sprints; **E** interacts; **V** changes camera; **J** opens the journal; **Escape** pauses or leaves a conversation. **F5** saves and **F8** restores the last saved checkpoint. Saving is unavailable during an encounter. After defeat, press **Enter** or select **Retry encounter**.

Pause contains look sensitivity, text size, music volume, invert look, reduced camera effects and Story assist. A compatible SDL gamepad can move/look with the sticks, interact with A, dodge with B, open the journal with Y, and pause with Start. During combat, the right stick or D-pad selects an angle; right trigger attacks and left trigger guards. Hardware gamepad behavior requires a connected controller review.

## Presenter notes

Allow the initial scene to finish preparing. Chapter loading is covered by the transition screen. The demo checkpoints chapter progress in `project/saves/presentation.progress`; **Continue journey** resumes it. **Begin journey** resets the preview's running story. Earlier demo packages and the original authored world are separate.

The journey uses a short authored chapter transition between distant regions. It does not require crossing the full map. Present it as a playable vertical slice in Valwood's custom engine.

## Scope

The current polish build adds contact-triggered sparks, ground dust, weapon trails, short impact holds, event-timed combat sounds and closer conversation framing. Reduced camera effects disables contact camera impulses and impact holds. The presentation retains VSync while allowing CPU/GPU overlap.

This preview adds guided chapters, a hostile encounter, meaningful persistent reward, styled player UI, conversation framing/head attention, measured dressing, a brighter selected look, original music, combat Foley and memory/reward cues. Character heads use the existing purchased rig. Facial/lip/finger animation and acted dialogue have not been produced; conversation is readable text with body/head performance. The original score and Foley use deterministic offline synthesis, without downloaded recordings or generated voices.

Optional human voice recordings can be supplied using CHARACTER_ASSETS.md and voice-recording-script.csv. The showing uses the existing actors and text dialogue.

Exact build checks, native screenshots, measured performance and any remaining failures are recorded beside this guide in the source evidence folder. A passing correctness workflow alone is not a universal frame-rate guarantee or AAA certification.

## Visual upgrade

This build adds 20 window assemblies with shutters, recessed backs and reflective panes; photographed CC0 forest litter; 87 grass clusters with varied tufts, player contact and shared color/shadow movement; additional palms, ferns, shrubs and exposed roots; and a brighter jungle canopy. Windows are attached facade assemblies with interior hints, without enterable rooms or cut-through source walls.

The existing combat hero uses a recovered 2K base color and 1K normal/material maps from its original 4K source. The live conversation NPC no longer has accidental emission or metallic skin. These changes retain existing topology and rigs. Facial animation, finger rigs and the final dad-to-knight character replacement remain future asset/integration work.

Output is 1600 × 900, starting at full internal resolution with automatic resolution adjustment down to 70% when needed. Reflections, contact shadows, fog and clouds remain enabled. Fixed-quality walking measurements are recorded separately.

Free material provenance is in project/assets/visual_upgrade/CREDITS.txt. This pass purchased no assets.

The intended replacement protagonist starts as a father in modern everyday clothes and later wears knight armor while retaining one face, body and skeleton. That wardrobe/progression is not part of the current prototype.

Particle shader warm-up uses a private temporary emitter, so disabling or moving authored smoke no longer prevents startup.
