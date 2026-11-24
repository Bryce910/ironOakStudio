# City and countryside walking slice

Start the derived playtest copy with `--play --scenic city-walk`. This is an
explicit grounded viewpoint in the current viewer. Its eye height is sampled
from the active walking surface, rather than taken from an aerial camera.

The tested start is X **−1298.4**, Z **−10212.584747455**, yaw
**−2.52781591453 radians**. Current eye Y is **25.72848643 m** and supporting
ground Y is **23.97848643 m**. Original source paving is 4.997 cm above that
support, matching the intended collision offset.

## Suggested light playtest

Allow about 6–10 minutes including stops to look around.

1. Walk from the start along the statue lane into the square (10 m), across the
   square (8 m), then along the market lane (10 m). Look at stalls, house faces,
   doors, windows, bunting and paving at walking height. Return by the same lanes.
2. Take the west lane out of town, passing the reviewed house opening. The first
   turn is X −1324.9, Z −10213.334747455; the town edge is X −1359.175606537,
   Z −10201.834747455.
3. Follow the earth trail through the meadow and around the trees. The trail
   passes under the supported city walls; the distant hilltop castle is a view.
4. At the trail's east bend, turn back at X −1393.4, Z −10202.334747455 and retrace
   the trail into town. The exact 213-point route is
   `city-market-country-return.walk`; all route landmark coordinates and ground
   heights are in `readiness.json`.

The complete tested out-and-return route is **852.51 m**. The shorter one-way
town-to-countryside trail is **398.25 m**. The separate northern residential
district is outside this recommended route.

## Current acceptance

The CPU check uses the game's Jolt adapter, 0.37 m radius capsule, 0.125 m sweep
steps, primary city field with countryside fallback, and the native uphill
slope rule. On the current installed data:

- 53 static source-collision bodies; 6,914 route queries; zero blocked steps,
  slope failures, missing-ground samples or query heap fallbacks.
- All four cardinal spawn movements pass.
- A deliberately blocked house crossing registers 63 blocked sweeps. Collision
  is therefore present, rather than a clear result from missing physics.
- Conservative tree/rock bounds leave at least 2.1185 m clearance beyond the
  player's capsule and an additional 5 cm guard.
- Nineteen protected project/physics/field/scenery files hash identically before
  and after the audit. The source project was not modified.

No physical route repair is needed for this bounded playtest.

## Reproduce

Run from `/home/computer/Desktop/final_engine`:

```sh
python3 new_engine/tools/check_city_playtest_slice.py \
  --project new_engine/projects/valwood_city_showcase \
  --output new_engine/docs/playtest-slices/2026-11-02/city \
  --build-checker
```

The optional checker build compiles the existing CPU helper into isolated `/tmp`
files, linking existing native physics objects. It does not rebuild the engine,
launch the game or use the GPU. The report records exact catalog, field, checker
and route hashes. Use a different output folder for checking a derived copy.

## Boundaries for the playable copy

This is an environment traversal slice. The old main quest, enemies, rewards and
NPC interactions remain at their original coordinates; this check does not
claim those systems are populated in the city. Source building UCX is a fixed,
separately authored collision document. Moving architecture in the editor
requires collision regeneration before walking acceptance.

The hilltop castle is a backdrop; its interiors and summit are outside this
walking route. Decorative grass and small dressing are intentionally
noncolliding. Actual player/controller movement, new interactions, native
Windows behavior and sustained frame pacing still require an interactive run.

For the derived playtest copy, retain `city-walk` as the startup view. Other
publisher camera rows are mostly flight views, and shoulder-button view cycling
can enter those views. A walking-only view list in the derived copy avoids
turning the playtest into a camera tour. Keep the source project's view list
unchanged.
