# Modular Knights skeletal motion bake

Run this module inside Blender after importing the purchased target rig:

```python
import sys
sys.path.insert(0, "/home/computer/Desktop/final_engine/new_engine/tools")
from retarget_modular_knight import bake_actions

actions, report = bake_actions(target_armature, fps=30)
bpy.context.scene.render.fps = report["sample_fps"]
bpy.context.scene.render.fps_base = 1
target_armature.animation_data.action = actions["Walk_Fwd"]
bpy.context.scene.frame_set(1)
```

The reusable derivative is already baked at:

`assets/purchased/modular_knights/derived/knight_rig_actions.blend`

It contains one target armature and 15 actions: an armed idle, eight directional
walks, a two-cycle forward run and five original captured attacks. Geometry,
materials, weapons, export and gameplay integration remain caller-owned. All
actions have fake users and carry JSON provenance in `action["valwood_clip"]`.
The adjacent `retarget_report.json` contains clip names, exact frame ranges,
phase markers, mapping and numerical measurements.

The target has 94 bones. Its anatomical `pelvis` is a child of the stationary
floor `root`. The audited 61-bone source map uses the same UE semantic names;
source finger metacarpals and extra twist helpers are not guessed onto absent
target bones. World-space bind compensation respects `target.matrix_world`.
Target rest bones and source FBX files stay unchanged. Source pelvis XYZ sway,
foot roll and swing height survive; no stationary foot planting, hand-grip IK,
per-frame floor adjustment or eight-pose mesh bake is used.

The imported FBX's default pose includes unwanted 100x root/helper scales.
Each action explicitly keys static rest TRS to override those values. Reset
the target pose to rest before binding/exporting newly reconstructed geometry.
The `.blend` derivative has already been reset, with no active action.

`clips=["Idle", "Walk_Fwd"]` can bake a subset. `target_map` accepts an explicit
actual-target-name to source-semantic-name dictionary. `seam_frames=4` applies
the measured small endpoint-offset correction, and `align_gait_phase=True`
aligns walks to left support/right toe apex. The duplicate final source hold is
excluded. The run deliberately retains two captured cycles; runtime phase
blending must retain that cycle count rather than treating it as one stride.

The five attacks retain their source timing and captured advancing translation.
They require collision-aware root-motion integration or an explicit in-place
derivative before being substituted into live gameplay. Contact source frames
are recorded as metadata; this module does not change combat hit timing.

The function restores the previous active action, pose, NLA mute states, scene
time and FPS. New actions use quaternion channels; the rig uses quaternion
rotation mode for their playback. Import failures remove partial new source
objects/actions and newly created derivative actions.

CPU joint checks establish finite transforms, stationary floor root, preserved
rest bone lengths and closed sampled loop endpoints. They do not approve
animation flow, foot contact, grips, armor clipping, GPU appearance or combat.
The in-game acceptance review remains necessary.
