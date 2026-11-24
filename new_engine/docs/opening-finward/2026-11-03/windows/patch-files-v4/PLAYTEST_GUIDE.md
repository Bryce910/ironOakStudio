# Valwood Opening Finward r2 — Windows

Run **Launch_Opening.bat** in this independent installed seed. Choose **Begin the Journey** for the complete apartment entrance. **Continue Checkpoint** resumes the saved playable step and bypasses the entrance film.

The father comes home exhausted, overworked and struggling with ordinary living costs. Read the bills on the coffee table, use the console beneath the television, and choose **NEW GAME** to transport into the Meadows of Valois. Follow the meadow path to the waystone to finish the preview.

## Furniture upgrade

Selected furniture and fixtures use the purchased Finward Studios [House Props & Furniture Vol. 2](https://assetstore.unity.com/packages/3d/props/interior/house-props-furniture-vol-2-265523). The private project also contains 15 reviewed assemblies in near and mid GLB versions under `project/models/finward_house_props`. Their PBR images are embedded; Unity is unnecessary for playtesting. `opening/finward.json`, `opening/finward-license.json` and the library manifest record the conversion and purchase provenance. Purchased content remains subject to the [Unity Asset Store terms](https://unity.com/legal/as-terms); this private handoff does not authorize public stand-alone asset redistribution.

The previous free Poly Haven prop/material work and evening lighting are retained. Their CC0 provenance remains in `opening/art-upgrade.json`. The current house collision proxies accompany the updated furniture. Do not mix older home models or physics into this seed.

## Controls

- WASD moves; mouse looks; Left Shift sprints; Space jumps; E interacts.
- Controller: left stick moves, right stick looks, LB/L1 or left-stick click sprints, X/Square jumps and A/Cross interacts.
- At the TV menu, Enter or A/Cross starts New Game. Escape or B/Circle steps away while retaining bills progress. Release interaction before confirming.
- Hold Enter/Space or A/Cross to skip a cinematic. Held input cannot skip successive scenes or activate an interaction on handoff.
- Escape or Start/Options pauses. J/Tab or Y/Triangle opens the journal.
- F10 or pause opens graphics/display settings. Pause also provides sensitivity, deadzone, invert Y and reduced motion.

## Light switches and progress

Rooms have independent E/A light switches. The television begins in standby, opens its game menu when activated, then changes to transport effects after New Game. The front door does not trigger transport.

F5 saves the supported story checkpoint; F8 restores it; R retries during gameplay. Pause offers the same actions. Checkpoints retain completed story steps and room-light states and restore authored safe positions. Begin the Journey resets the opening and lights.

This seed owns its saves and graphics settings. Keep previous installed folders and original purchased sources. The bathroom door remains parked open, and furniture fixtures are decorative. Father body/hand acting, voice recording, armor progression and a first combat encounter remain separate work. The apartment remains first person; later third-person gameplay uses the main game's existing character and camera systems.

## Receiving computer

Check graphics, sound and the physical controller on the receiving Windows PC. CPU package verification establishes transport integrity and dependency closure; it does not establish native Windows GPU performance or final art quality. OpenGL 3.3 driver support is required. Keep `logs/opening-latest.log` if startup fails.
