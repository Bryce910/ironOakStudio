# Emerald Expanse — short playable jungle route

**Primary slice: 5–8 minutes of grounded exploration, using the existing expanded jungle.** Walk the complete path and southern loop, inspect a hidden pool and ruined shrine, watch the canopy birds and ground wildlife, and return to the entry. This is actual player movement and terrain/scenery collision, not a camera slideshow. The animals are ambient; there is no animal combat or new jungle mission. The existing combat/report quest is a separate optional test below.

This plan was prepared by reading current code and recorded native results on 2 November. **A fresh playtest run on the final binary has not been performed for this plan.** Root will launch a disposable project copy; no source assets/project were changed and no snapshot was duplicated by this audit. [route.json](/home/computer/Desktop/final_engine/new_engine/docs/playtest-slices/2026-11-02/jungle/route.json) records the exact route, launch arguments, existing test commands and acceptance fields.

## Launch the actual walking slice

Use the current `rpg` with a disposable copy of `projects/valwood_visual`, including its `jungle_expansion.json`, `world.studio`, private models and runtime banks. Root owns creating that copy or the lightweight Windows overlay. Do not replace the protected project or build the older `valwood_world` jungle.

From the workspace, with `PLAYTEST_PROJECT` set to that copy:

```sh
env VALWOOD_PRESENTATION=1 VALWOOD_SHOWCASE=jungle \
  ./new_engine/rpg --project "$PLAYTEST_PROJECT" --play \
  --scenic rainforest-milestone --width 1600 --height 900
```

**Omit `--no-hud`:** it hides the jungle map and hints. The existing `Launch_Valwood_Jungle.sh` delegates to the presentation launcher with that flag, so the direct command above is the recommended playtest entry. Start is `rainforest-milestone`, native X/Z **−26737 / 9736**, yaw **1.2 radians**, terrain height +1.75m eye height. The review handler sets walking, clears vertical velocity and enables the expanded minimap. Wait for loading to finish before evaluating movement.

WASD moves; hold right mouse to look; Shift sprints; V changes first/third person. With an SDL controller use left stick to move, right stick to look, and right-stick click to change camera. B/back or Escape leaves this review; LB/RB switches review areas, so avoid those during the route. Physical-controller acceptance remains a hands-on check. The existing virtual-controller test passed 41 assertions on 31 October.

## Walk the 5–8 minute loop

Allow about five minutes of ordinary walking plus one to three minutes of observation. The measured centerline is **1,031.04448m**, plus the short approximately 10m entry connector. The mini-map shows the trail, player position/facing and nearest destination. Follow the path instead of walking straight toward a distant marker through dense foliage.

1. **Entry and original trail, 0:00–1:00.** Move to X/Z −26727 / 9735.497, then follow the worn path south through −26710 / 9700.497, −26720 / 9660.497, −26707 / 9615.497 and −26717 / 9575.497 to the loop junction at −26700 / 9530.497. Look up for macaws; inspect foliage on both sides.
2. **West loop and pool, 1:00–2:30.** Take the western branch through −26734 / 9501.497, −26767 / 9470.497 and −26780 / 9436.497. At −26778 / 9392.497 take the short bank spur through −26791 / 9387.497 to −26795 / 9382.497. Inspect the Hidden Pool from this dry bank, then retrace the spur. The pool destination itself is at −26795 / 9372.497; entering water is not required.
3. **Shrine and canopy, 2:30–3:30.** Continue south/east through −26755 / 9350.497 to −26720 / 9311.497. The Fallen Shrine is 7m north of this path. Continue to Canopy Watch at −26678 / 9311.497 and pause to inspect birds, foliage and lighting. Boars/leopards wander near their glades; sightings are not guaranteed at an exact frame. Remain on the loop rather than chasing animals.
4. **Eastern return, 3:30–5:00.** Follow −26647 / 9344.497, −26640 / 9387.497, −26651 / 9426.497, −26674 / 9462.497 and −26690 / 9497.497 back to the junction. Return along the original trail to the entry. Test one short sprint, first/third-person switching and controller movement if available.

Pass for light exploration: the entire loop is walkable without flying/teleporting; no persistent obstruction, falling through terrain, missing textures, unexpected model-loading error or camera-control failure; wildlife/birds animate; player returns to the entry. Record any stuck location and screenshot. Visual quality and frame pacing are feedback, not assumed passes.

## Existing combat, interaction and persistence test

The direct jungle review intentionally disables E interactions, combat, F5/F8 and journey saves. This preserves the existing story while touring. The shrine, pool and wildlife do not have invented quest interactions. Escape/Back returns to the previous screen, rather than creating a jungle checkpoint.

For the real playable **A Place in the Watch** journey, launch the same disposable project with `VALWOOD_PRESENTATION=1` and **without** `VALWOOD_SHOWCASE`, then choose **Begin journey**. This starts in the castle quarter and provides the required trial before the jungle crossing. The UI advertises **10–15 minutes**; do not promise a first-time player can finish it in five minutes.

1. Follow the gold marker to Master Corvin; E, then the first response starts the three-round trial. Keep Story assist enabled for light testing. Choose attack angle with mouse/arrow keys; left mouse swings, right mouse guards/parries, middle mouse thrusts and Space dodges. On controller, right stick/D-pad chooses direction, RT attacks, LT guards, RB thrusts and B dodges.
2. For failure coverage, let the opponent defeat you once. Enter/**Retry encounter**, then accept the retry dialogue and win. Defeat must not award mastery.
3. After victory E takes the chapter transition to `rainforest` at **−26727 / 9735.497**. Follow the worn path, then the clearing approach in the JSON spec. Crossing center is **−26709 / 9695.497**; approaching within 28m reveals the threat objective and E within 17m starts the oathbreaker encounter. Win and inspect hit/parry sparks, dust and weapon trails.
4. Speak to Warden Rowan near **−26693 / 9674.497**; choose **Take Rowan's sealed report**. E returns to Corvin. Deliver the report/take the watch oath: ending, **25 coin and +1 Watch reputation**, with no duplicate reward after reload.
5. Outside combat/loading, F5 saves and F8 restores; the pause menu provides equivalent controller-friendly actions. Saving is intentionally rejected during an encounter. Progress lives in the disposable project's `saves/presentation.progress`. **Begin journey** starts fresh; **Continue journey** restores the saved run. Do not copy a manually edited progress file into the source project.

Optional extended exploration can then use the loop above. The quest follows the older clearing approach; it does not require the entire southern expansion. The historical quest harness also places the player near some NPCs, so its pass does not replace a new manual navigation check between every objective.

## Reusable executable checks — root schedules GPU access

From `new_engine`, use new output directories. These existing commands isolate progress and fingerprint protected inputs; they do not make full project snapshots:

```sh
python3 -B tools/check_jungle_expansion.py \
  --walk --output docs/playtest-slices/2026-11-02/jungle/native-walk
python3 -B tools/check_presentation.py --project projects/valwood_visual \
  --view complete --retry --require-polish --render-scale .7 \
  --output docs/playtest-slices/2026-11-02/jungle/native-journey
python3 -B tools/check_controller.py --project projects/valwood_visual \
  --output docs/playtest-slices/2026-11-02/jungle/native-controller
```

The walk helper is hardcoded to the source visual project and RX 5300 renderer; it is an owner-side regression, not a portable Windows acceptance tool. The full loop uses ordinary movement at 3.5m/s. Its historic 31 October pass visited all 27 subsequent waypoints with no collision/slope blocks, no unsafe animal roots and unchanged protected files. The complete quest separately passed actual defeat/retry, save/resume, reward/ending and VFX checks. All used binary `1ea78f8beea90679156b17fef84b6bd597953758d1d665afccb957f1892aafd4`; the final 2 November opening capture alone is not a fresh full walk/quest pass.

Historical loop performance was 54.89 FPS average / 37.35 FPS 1% low at fixed 70% render scale; a 45 FPS floor is unproven. Source art/hero and text-only dialogue limitations remain. Latest renderer opening evidence is in [native orientation](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/native-orientation/results.json); historical route/combat evidence is in [jungle validation summary](/home/computer/Desktop/final_engine/new_engine/docs/jungle-expansion/2026-10-31/validation-summary.json).
