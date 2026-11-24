# Finward source demo collision and switch navigation

`SWITCHES.json` is the runtime builder input: four source-wall switch plates with room bounds and world/local positions, plus five production-validated routes. Origin is `[-3500, 600, -11000]`. Room IDs are living 0, kitchen 1, bedroom 2, bathroom 3. All point arrays use engine right-handed coordinates.

`NAVIGATION.json` records the actual production `world_playtest_walk::Support` and Jolt result. The 0.20 m connectivity grid uses the current 0.37 m radius character capsule; edges pass `can_move` in both directions. Smoothed paths are then replayed at intervals of 0.05 m. Result: five routes, 79.752149 m, 28,790 dense queries, 13 bodies, and zero query heap fallbacks. All four approach-to-switch eye rays are clear up to the final target tolerance. Bathroom first contact is within the final 7 mm of its plate endpoint.

Both blocked-wall negative tests have supported equal-height floors at each endpoint, an intervening body-height ray contact on the source wall, and a rejected production movement crossing. They use the living and bedroom doorway posts rather than steppable furniture.

Runtime collision comes from `../nav-v2/runtime/scene.physics` and `home.settings`. Its conversion report records the original enabled static source colliders, omitted small cosmetic clutter, and explicit omission of the non-house `Scene/Cube` background support. All actual house floor, building, major furniture and window collision remains. The frozen Unity export is preserved at `assets/purchased/finward_house_props/scene_export/scene.json`.

Reproduce from `new_engine`:

```sh
g++ -std=c++17 -O2 -I. docs/opening-finward-reference/2026-11-03/nav-v3/check_scene_navigation.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o docs/opening-finward-reference/2026-11-03/nav-v3/check-scene-navigation
docs/opening-finward-reference/2026-11-03/nav-v3/check-scene-navigation docs/opening-finward-reference/2026-11-03/nav-v2/runtime/home.settings docs/opening-finward-reference/2026-11-03/nav-v3/NAVIGATION.json
python3 docs/opening-finward-reference/2026-11-03/nav-v3/render_nav.py
```

`navigation-map.png` is a measured physics planning diagram. Rendered occlusion, native interaction feel, lighting and complete story playthrough remain separate runtime checks. These tools do not write character poses, launch a renderer, or modify accepted game projects and purchased source assets.
