# Skeletal hero inspection

Build and run from the repository root on Linux:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I new_engine \
  new_engine/tools/check_hero_skeletal.cpp new_engine/engine/world_animation.cpp \
  -o /tmp/valwood-check-hero-skeletal
/tmp/valwood-check-hero-skeletal \
  new_engine/models/npc/white_knight_brown_armor.glb \
  --socket RightHand --socket LeftHand --socket Head \
  > /tmp/valwood-hero-skeletal-inspection.json
```

The tool uses the actual engine importer and validator. It checks the rest pose,
every clip at 60 Hz plus all exact authored key times, and each clip's loop
boundary. Every sampled vertex is deformed using the engine's CPU reference;
positions, normals, all joint/skin matrices and every unambiguous named joint
socket are checked. Bounds are reported in asset coordinates, including the
skinned mesh node's transform. Skeleton, geometry, PBR material bindings and
per-clip measurements are emitted as one JSON object on stdout. It opens no
window, needs no GPU, and changes no project, runtime or launcher.

Use `--hz 120` for a denser grid, or a lower rate for a cheaper inspection. The
allowed rate is 1–240 Hz, with a total budget of 100,000 samples. Repeated
`--socket JOINT` options require those exact names to exist unambiguously; every
other unique joint socket is also checked. Duplicate names are reported and
still receive ordinary joint matrix checks. Failures return a nonzero exit code
and a JSON error. A rig without animation can pass structural checks, with
`has_animations: false` reported explicitly.

**A pass proves only the reported import and sampled numerical checks.** It does
not establish smooth motion, seamless loops, correct retargeting, foot planting,
weapon grips, clipping, decoded texture quality, GPU appearance, combat behavior
or sustained frame rate. Those require the separate in-game acceptance review.
The tool applies the engine's existing GLB limits; it does not alter them.
