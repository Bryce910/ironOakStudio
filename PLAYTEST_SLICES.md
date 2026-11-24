# Valwood playable combat slices

For the new eight-opponent sword/bow practice ring, open [Launch_Valwood_Combat_Arena.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Combat_Arena.sh). It has sequential opponents, **1 for sword / 2 for bow**, controller weapon selection and both camera modes. See [arena controls](/home/computer/Desktop/final_engine/COMBAT_ARENA.md).

All three areas start with the existing armed hero visible in third person and a nearby practice knight to fight. Grandstone now includes the verified stairs, doorway and shading cleanup described below.

Open [Launch_Valwood_Playtest.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest.sh) to choose an area, or use a direct launcher:

- [City and countryside](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_City.sh): practice knight, market streets, meadow and woodland; about 853 m of guided exploration.
- [Jungle](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_Jungle.sh): practice knight and the Emerald Expanse wildlife loop; about 1,031 m.
- [Grandstone](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest_Grandstone.sh): starts in the first hall with a practice knight. Pause > **Travel to snowy entrance** opens the separate gate-approach section. The two sectors retain about 42 m of guided markers, with both hall side stairrooms and upper landings, both barbican stairs and the full 67-piece exterior staircase now supported for local exploration.

## Fight, then explore

Press **E or Enter** near the practice knight to begin. Use **left mouse** to attack, **right mouse** to guard, and **arrow keys or mouse motion** to choose an angle. Match the incoming angle and guard late to parry. **Middle mouse** thrusts; **T** selects thrust for the next attack. **WASD** moves and **Space** dodges during combat. The HUD shows health, stamina, opponent health and incoming attacks.

After victory or defeat, choose **Retry/Fight again** or **Continue exploring** on the result panel. E/Enter can retry from that panel. **R** aborts a fight and resets the current section and route. Ambient residents and wildlife are not combat targets. These practice duels do not award quest progress, gear or rewards.

Outside combat: WASD walks; hold right mouse to look; Shift sprints; Space jumps; **V** switches first/third person; **Esc** pauses; **F10** opens graphics. In Grandstone, pause provides section travel; completing a section also enables its travel prompt.

Controller in combat: push the **right stick left/right to cut left/right, up for a rising cut, or down for an overhead cut**. No attack button is required. Return the stick to center between swings; holding it produces one swing. Hold **LT/L2 + right stick** to block left, right, high or low. Hold LT/L2 with the stick centered to guard against a thrust. Press LT/L2 late to parry; changing the stick direction while holding LT/L2 does not restart the parry window. LT/L2 takes priority over attack inputs, and releasing it with the stick held cannot start a swing. RT/R2 or X/Square remains an alternate attack; RB/R1 thrusts; B/Circle dodges.

Outside combat, the right stick looks and X/Square jumps. Left stick moves; LB/L1 or left-stick click sprints; right-stick click switches view; Start/Options pauses. A/Cross starts a challenge or confirms a travel/menu prompt. Navigate result panels with left stick/D-pad and confirm to retry or continue exploring.

The local Linux launchers include the November 4 sword-stick update and corrected held-block poses. **YOUR BLOCK** identifies your guard; **OPPONENT GUARD** identifies the knight's. The latest [guard verification](/home/computer/Desktop/final_engine/new_engine/docs/sword-guard/2026-11-04/RESULTS.md) passed 43 actual SDL input checks and captured all five settled guards in both camera modes, plus the attacks. The Windows executable compiles, but the older Windows ZIPs below retain their previous controls and poses.

## Save and settings

**F5 saves and F8 restores exploration only**: position, view, route progress and section. Save/Load are blocked throughout an active fight, including countdown. Duel health and results are not serialized. R does not overwrite a manual save. Pause includes Save/Load and graphics controls.

Each playtest uses private settings, AI state and saves. Large local asset banks are shared to save disk space; authoring is guarded. Use the original artist workspace to edit assets. The original source projects and orientation ZIP are retained.

## Grandstone cleanup

The latest Grandstone update fixes false stair/doorway contacts, extends collision over the connected flights and upper landings, reduces tight-camera clipping and corrects over-dark baked stair shading. The native test passed nine routes in both directions (555.55 m); Windows under Wine passed four selected routes (95.83 m). Both platforms passed the ten-case sword-combat workflow with the final shader. Closed gates remain solid, and normal jumping lands on the correct floor.

Your local runtime and Grandstone walking data are updated. Original art/source projects, saves and the other private playtest files were preserved. The ordinary artist runtimes and earlier ZIPs remain unchanged. See [Grandstone results and limits](/home/computer/Desktop/final_engine/new_engine/docs/grandstone-cleanup/2026-11-02/RESULTS.md) and [the stairs/doorways checklist](/home/computer/Desktop/final_engine/new_engine/docs/grandstone-cleanup/2026-11-02/PLAYTEST_CHECKLIST.csv).

Grandstone's final whole combat run averaged 38.42 FPS native and 39.63 FPS under Wine at 1600×900 and fixed 70% scene scale. Loading dips remain; this is not a 45 FPS floor. The following three-area combat figures describe the previous baseline.

## Current verification and performance

All three areas passed actual sword-combat acceptance on Linux and the Windows executable under Wine: defeat, retry, victory, timed parries, rendered hero/opponent, VFX, reset and the actual **Continue exploring** button. The native camera/pause/save/load workflow passed 31 cases. Every run retained source-state and executable guards.

Performance remains unfinished on this RX 5300. At 1600×900 and fixed 70% scene scale, the earlier whole combat runs averaged **59.91 FPS city, 36.82 jungle and 29.38 Grandstone**. These runs include loading and capture work; they do not establish a locked frame rate. The earlier completed walking routes are historical traversal verification, not current combat-performance results. Physical Windows driver, controller feel and audible output still require testing on the designer's PC.

See [combat verification and limits](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/RESULTS.md) and use [the current playtest checklist](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/PLAYTEST_CHECKLIST.csv). The replacement father/knight character, modular equipment, relocated city quests/vendors and navigation across the rest of Grandstone remain separate work.

## Windows combat add-on

Use [Valwood_Windows_Combat_Playtest_2026-11-02.zip](/home/computer/Desktop/final_engine/Valwood_Windows_Combat_Playtest_2026-11-02.zip) with a **fresh extracted copy** of the original Valwood_Windows_Orientation_2026-11-02.zip. This new combat add-on does not require the earlier traversal add-on. Extract it into that orientation transport folder, then run Install_Playtest_city.bat, Install_Playtest_jungle.bat or Install_Playtest_grandstone.bat. The installer creates a NEW independent seed; run Launch_Playtest.bat inside it. No Unity or separate Python installation is needed.

The add-on supplies playtest-index.json, Install_Playtest_Bundle.py, the three Install_Playtest batch files and its new content blobs. The original bundle-index.json and orientation files remain intact.

Allow roughly 15.4 GB city, 7 GB jungle or 24.6 GB Grandstone of installed space. Existing seed folders are preserved; choose a different output folder if retaining an older playtest, for example:

```bat
Install_Playtest_city.bat --output "%USERPROFILE%\Desktop\Valwood_city_Combat_Playtest_Seed"
```

The new add-on is 9,781,433 bytes, SHA256 `12b31ff4e06226d608d997e31daa93499affe7d300196ca9df6844b0c5a19369`. Its [final archive verification](/home/computer/Desktop/final_engine/new_engine/docs/playtest-combat/2026-11-02/windows-overlay-verification.json) passed for all 31 transport members, the three complete project closures, and the accepted Windows runtime and section data. Full production-seed installation on the designer's Windows computer remains to be checked.

## Windows Grandstone fix add-on

Use [Valwood_Windows_Grandstone_Fix_2026-11-02.zip](/home/computer/Desktop/final_engine/Valwood_Windows_Grandstone_Fix_2026-11-02.zip) with a **fresh extracted copy** of the original orientation transport for the latest Grandstone. This single-area add-on supplies playtest-index.json, Install_Playtest_Bundle.py and Install_Playtest_grandstone.bat. Extract it into that transport folder and run the Grandstone installer. It does not require the older combat add-on. Use the combat add-on above for the city and jungle.

Choose a new seed folder to preserve any older Grandstone playtest:

```bat
Install_Playtest_grandstone.bat --output "%USERPROFILE%\Desktop\Valwood_Grandstone_Fix_Playtest_Seed"
```

Run Launch_Playtest.bat inside the resulting seed. Allow about 24.54 GB for that independent installation. The fix ZIP is 13,160,342 bytes, SHA256 `e93a574e2c0b58c79c138d37a3e42f431aa72ef04a91b3a166f9ba3fb3b249cb`. [Archive verification](/home/computer/Desktop/final_engine/new_engine/docs/grandstone-cleanup/2026-11-02/windows-overlay-verification.json) checks its exact tested Windows executable, six sector files, private shader, all 18 members and full dependency closure. Original orientation files and earlier deliveries remain intact. Physical Windows installation and controller feel still need the designer's check.
