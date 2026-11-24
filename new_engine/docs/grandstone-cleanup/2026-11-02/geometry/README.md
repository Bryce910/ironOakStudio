# Grandstone stair and doorway collision cleanup

The old approach bounds ended during the first exterior stair pieces and cut the barbican tower stairs. The hall stopped below the side stair landings. The new collision sectors retain the complete 67-piece exterior staircase, the barbican staircases, both hall side stair rooms through their upper landings, and the solid closed gates.

The purchased source scene also contains objects exported into both LOD bands. The cooker now selects one collision LOD per logical object. It keeps the detailed source floors and stairs, selected source walls and rails, statues, torches, braziers and solid door planks. Thin decorative metal overlays, decals and ornaments do not become snagging collision sheets. No renderer mesh, material, purchased source file or original game project was changed.

The exterior source layout places a small rock on stair piece 37. Its actual mesh is included in the collision instead of allowing the player to pass through it. The source-center audit is in `exterior-rock-intrusions.json`.

| Sector | Vertices | Triangles | Physics document |
|---|---:|---:|---:|
| Approach and complete exterior stair | 135,018 | 263,226 | 14,261,949 bytes |
| Hall and both complete side stair rooms | 96,520 | 180,301 | 9,897,779 bytes |

The validated limits are 147,456 vertices and 294,912 triangles; the existing 16 MiB document cap and 512-body cap remain. This is source geometry with a five-micrometre seam weld, without decimation. `READY.json` records all six staged runtime-file hashes and verifies that the original source scene and 235 source mesh files are unchanged. Routes retain their existing fingerprints.

`tools/check_grandstone_obstacles.cpp` uses the same Jolt adapter and Support movement as the game. Nine independent cases cover both open hall doorways, the first and upper hall stair flights on both sides, both barbican staircases and the complete exterior flight. Each path is walked forwards and backwards in steps of at most 5 cm. Separate capsule casts verify that the two closed gates remain solid, and out-of-bounds samples remain rejected. These independent setup positions are explicit test teleports; they are not a claim that the whole castle is connected in one continuous playtest route.

Shared case definitions and the final accepted paths are in `engine/world_grandstone_qa_cases.h`, for both CPU checks and in-game movement rehearsal. `tools/export_grandstone_qa_paths.py` refuses to export a partially passing report. Frozen paths are then replayed by the CPU checker with its `verified` argument.

```bash
python3 -B tools/cook_grandstone_playtest.py --output docs/grandstone-cleanup/2026-11-02/geometry/runtime
python3 -B -m unittest tests.test_cook_grandstone_playtest -v
c++ -std=c++17 -O2 -I. tools/check_grandstone_obstacles.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-check-grandstone-obstacles
/tmp/valwood-check-grandstone-obstacles docs/grandstone-cleanup/2026-11-02/geometry/runtime docs/grandstone-cleanup/2026-11-02/geometry/obstacles.json
python3 -B tools/export_grandstone_qa_paths.py docs/grandstone-cleanup/2026-11-02/geometry/obstacles.json
```

Final CPU acceptance passed all nine frozen shared-header paths in both directions: 1,024 path nodes, 11,542 steps of at most 5 cm and 555.548 m of movement. Every accepted step also passed two successive source-support samples with drift at most 5 mm. Both solid-gate capsule negatives and out-of-bounds negatives passed, with zero query heap fallback. Results are in `obstacles-verified-header.json`; `READY.json` binds the accepted code and data hashes.

The parent native rendering and actual `move_camera` rehearsal remains required before marking the complete cleanup finished. Wider unconnected castle areas remain outside these sectors.
