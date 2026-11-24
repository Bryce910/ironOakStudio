# Grandstone stairs, doorways and sword-combat playtest

Run Launch_Playtest.bat inside this NEW installed combat playtest seed. No Unity or separate Python installation is needed. Graphics settings are under F10; start with Balanced/Performance on a small GPU and raise quality on stronger hardware.

## Start here

**This version starts inside the first hall**, with the armed hero and practice knight ready for a challenge. Press Esc and use the travel button to visit the snowy entrance immediately; pause can also return you to the hall. The approach and hall are separate compact playable sectors with explicit travel, rather than a continuous walk through the publisher scene. Together their guided walking segments total about 42 metres, with wider space to inspect each sector. Near the knight, E prioritizes the duel; use pause Travel for a reliable section switch.

## Stairs and doorways

This revision expands the walking collision sectors around both hall side stairrooms and the complete exterior staircase. After trying the duel, choose **Continue exploring** and inspect the hall floors, door thresholds, side flights and upper landings. Use pause > **Travel to snowy entrance** to review the approach and exterior stairs, then return through pause Travel.

Test climbing and descending at normal speed before sprinting, and switch between first and third person near stairs, walls and pillars. Visible closed gates and doors remain solid. The original source architecture has not been moved; the two sectors still use explicit travel, and distant castle districts are outside this walking test. Record any remaining snag, step-up failure, disappearing floor or camera clipping in the checklist.

## Explore and fight

The game starts grounded in third person, with the existing hero carrying a sword. The nearby practice knight is a dedicated opponent; ambient residents are not combat targets. Press **E or Enter** near the knight, or choose **Start sword challenge** in pause, to begin a repeatable duel. Starting a challenge faces the opponent and restores third person.

During combat, watch your health, stamina and the opponent's health. Choose an attack angle that differs from their guard, match incoming attacks to guard, and time a late guard for a parry. Defeat and victory show a result panel. **E / Enter** or its retry button starts a fresh fight with restored health and stamina. Choose **Continue exploring** to close the result panel and resume walking. **R** aborts the duel and resets the current section and its route. These practice fights do not award quest progress, equipment or rewards.

## Keyboard and mouse

| Action | Control |
| --- | --- |
| Walk / sprint | WASD / hold Shift |
| Look while exploring | Hold right mouse and move mouse |
| First / third person | V |
| Start or retry duel | E or Enter near the practice knight / result panel |
| Attack | Left mouse |
| Guard / parry in combat | Hold right mouse; match the incoming angle, or press late to parry |
| Choose combat angle | Arrow keys or mouse motion |
| Thrust | T selects thrust, then left mouse attacks; middle mouse thrusts immediately |
| Jump while exploring / dodge in combat | Space |
| Reset section and duel | R |
| Pause / graphics | Esc / F10 |

Combat faces the opponent automatically and keeps movement near the duel. Outside combat, follow the gold markers and minimap to explore.

Controller (Xbox / PlayStation labels): left stick moves; right stick looks while exploring and chooses combat angle during a duel. RT/R2 or X/Square attacks, LT/L2 guards, D-pad chooses angle, RB/R1 thrusts and B/Circle dodges. A/Cross starts a challenge or uses a travel prompt. X/Square jumps outside combat. LB/L1 or left-stick click sprints; right-stick click switches camera; Start/Options pauses. Use left stick/D-pad and confirm/back in menus. After a result, select **Continue exploring** to resume walking, or choose retry for another duel.

## Save and restore

**F5 saves and F8 restores exploration only**: position, camera, route progress and section. Both are blocked during an active fight, including its countdown. Pause provides the same Save/Load controls. Duel health and victories are not saved. R resets the current section without overwriting a manual save. Saves are private to this newly installed playtest folder; incompatible route saves are rejected.

## Report issues

Try a fight, a defeat and retry, a victory, and a reset. Then explore, switch views, test pause and save/restore, and inspect ground/building transitions. Record area, nearest landmark, screenshot, graphics preset, GPU/driver and logs/playtest-latest.log. Complete PLAYTEST_CHECKLIST.csv for your PC; do not assume a 45 FPS floor.

These are independent playtest projects. Use the original orientation workspace for art editing. Authoring and unrelated travel shortcuts are guarded in gameplay.
