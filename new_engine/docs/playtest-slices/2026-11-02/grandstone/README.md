# Grandstone bounded walking sectors

The CPU gate passes for two separately transitioned walking sections. Native rendering/input acceptance remains a separate check. This is a short environment playtest, not certification of continuous navigation through the whole publisher castle.

The publisher scene's visible paving is elevated above its terrain: approximately 14.20 m at the entrance, 16.01 m at the lower-town view and 19.85 m beneath the first Temple hall. The entrance and Temple view are approximately 155 m apart horizontally and 64 m vertically. The original gate/Temple doors are visibly closed. Enabling walking on the original terrain height alone would place the player beneath the architecture.

## Staged data

The six frozen runtime files are in `runtime/`; exact hashes are in `READY.json`. They total 6,777,818 bytes. Copy the two route files to the playtest project's root and the four `.settings`/`.physics` files to its `playtest/` directory. No model, texture, shader, source project, original archive or purchased source was modified. The source scene and all 104 source mesh files read by the cooker were rehashed unchanged.

| Sector | Source-triangle collision | Guided walk | CPU gate |
|---|---|---|---|
| Snowbound Barbican approach |46,290 vertices /85,939 triangles /4,652,182 bytes |10.928m,3 waypoints |219 continuous5cm steps; actual closed gate blocks; actual unsupported source void and sector bounds reject |
| First Temple hall |21,871 vertices /38,296 triangles /2,124,032 bytes |31.000m,7 waypoints |620 continuous5cm steps; actual column blocks; actual unsupported source void and sector bounds reject |

Both use the existing 65,536-vertex /131,072-triangle /16 MiB document limits, with zero query heap fallbacks. The collision is original source MID wall/decor geometry with original near-only components retained; original HIGH floor, carpet and stair triangles are used for walking surfaces. Faces are oriented against their transformed source normals. Positions are welded at 5 decimal places in local metres (at most 5 micrometres per axis); no generic room AABB or invented flat floor is used.

The marked walk is approximately 42 m across the two sections, with additional exploration inside the supported sector footprints. At the sealed gate, an explicit interaction transfers to the first hall; the hall provides an explicit return. This preserves the rendered closed doors and makes no claim that the intervening bridges/stairs have been made navigable. Combat, rewards, NPC interaction and full-castle navigation are outside this CPU certificate.

## Runtime contract

`engine/world_playtest_walk.h` is header-only and wraps the existing native Jolt adapter:

```cpp
bool Support::load(const char *settings, std::string &error);
bool Support::sample(double x, double z, double referenceFeet, float &floor) const;
bool Support::can_move(double ax, double az, double bx, double bz,
                       double feet, float &nextFloor) const;
bool Support::cast_capsule(world_physics::Vec3 center, float radius, float halfHeight,
                           world_physics::Vec3 delta, world_physics::Hit &hit) const;
```

`spawn()`, `bounds()`, `stats()` and `loaded()` provide read-only state. Settings load atomically; invalid data or an unsupported/obstructed spawn leave the prior sector active. Physics paths are portable same-directory filenames. UTF-8 settings and collision paths use the Windows-compatible filesystem path conversion.

Support rays use the current feet height as a storey reference, accept upward surfaces only, and never fall back to the lower terrain through a missing source floor. A small supporting footprint anticipates actual tread contact before the capsule centre crosses a stair. Movement checks every ≤12.5 cm, with step-up/headroom/forward/down capsule casts, 0.6 m maximum steps and 0.8 m maximum supported drops. Ordinary jumps retain their lower support reference. The caller should use this movement contract instead of the old terrain-only slope sampler when the sector is active. Vertical collision and third-person camera clearance must also query its physics.

## Reproduce without a GPU

From `new_engine`, using the already-built native Jolt adapter/library:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  tests/world_playtest_walk_test.cpp engine/world_physics.o build/physics/libJolt.a \
  -pthread -o /tmp/valwood-world-playtest-walk-test
/tmp/valwood-world-playtest-walk-test
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  tools/check_grandstone_playtest.cpp engine/world_physics.o build/physics/libJolt.a \
  -pthread -o /tmp/valwood-check-grandstone-playtest
```

Run that checker with `sector.settings output.route output.json`. `approach-cpu.json` and `hall-cpu.json` contain the actual source results. The synthetic native test additionally checks overlapping storeys, roof rejection, airborne movement, a real stair riser, a blocking wall, low headroom, a true hole, out-of-bounds support and malformed/unsafe-spawn rollback. The runtime routes were parsed by the shared playtest codec and matched the independently regenerated CPU route bytes.

Re-cooking is opt-in with `python3 -B tools/cook_grandstone_playtest.py`; it writes the staging folder only. Do not regenerate a frozen runtime folder during native QA or while preparing a delivery overlay.

## Resident-aware route revision

The first native walk exposed a real static-resident block near the old hall endpoint. The current hall route stops at the guard overlook after 31 m, with 0.95 m clearance beyond the native player-plus-resident body radii. Its 620 dense samples now check both source Jolt support and the exact static-resident predicate from `world_stronghold_can_move`; the old blocking segment is retained as a negative regression. Approach heading now faces its first forward waypoint. `resident-route-repair/report.json` records the evidence and prior runtime hashes. Collision, actor positions, rendered assets and original source files are unchanged. Native completion of this revised route requires the owner's next integration run.
