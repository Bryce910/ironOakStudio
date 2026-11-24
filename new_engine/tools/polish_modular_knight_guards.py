"""Build isolated directional holds and light reactions on the audited knight.

Run in Blender. Original fifteen actions, all ninety-five rest bones, and all
purchased files are read only. The inspection pass writes source pose metrics;
the final pass writes new derivative actions without changing the old actions.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import retarget_modular_knight as capture
import animate_modular_knight_weapon as weapon
import refine_modular_knight_grip as grip

PACK = TOOLS.parent / "assets/purchased/modular_knights"
OUT = PACK / "derived/polish-v2"
GUARDS = {
    "Left": "Hitreact/MCU_am_LongS_Uni_BounceBackSmall_HighL_StanceR.FBX",
    "Right": "Hitreact/MCU_am_LongS_Uni_BounceBackSmall_HighR_StanceR.FBX",
    "High": "Block/MCU_am_LongS_Lv1_StanceR_Idle_Block.FBX",
    "Low": "Hitreact/MCU_am_LongS_Uni_BounceBackSmall_LowL_StanceR.FBX",
    "Thrust": "Hitreact/MCU_am_LongS_Uni_BounceBackSmall_MidR_StanceR.FBX",
}
HITS = {
    "Left": "Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Left_StanceR.FBX",
    "Right": "Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Right_StanceR.FBX",
    "High": "Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Front_Mid_StanceR.FBX",
    "Low": "Hitreact/MCU_am_LongS_Uni_Hitreact_Light_FrontRight_Low_StanceR.FBX",
    "Thrust": "Hitreact/MCU_am_LongS_Uni_Hitreact_Light_Front_Mid_StanceR.FBX",
}


def signature(action):
    return hashlib.sha256(repr(sorted((curve.data_path, curve.array_index,
        [(tuple(key.co), tuple(key.handle_left), tuple(key.handle_right), key.interpolation)
         for key in curve.keyframe_points]) for curve in action.fcurves)).encode()).hexdigest()


def world(rig, name):
    return rig.matrix_world @ rig.pose.bones[name].matrix


def pose(rig, action, frame):
    rig.animation_data.action = action
    capture._set_frame(frame)
    return {bone.name: bone.matrix_basis.copy().decompose() for bone in rig.pose.bones}


def metric(rig):
    transform = world(rig, weapon.WEAPON)
    base, tip = (transform @ Vector((0, 0, z)) for z in (10.54, 107.9572))
    direction = (tip - base).normalized()
    point = lambda p: [float(p.x), float(p.z), float(-p.y)]
    shoulder, elbow, wrist = (world(rig, name).translation for name in ("upperarm_l", "lowerarm_l", "hand_l"))
    length = (elbow - shoulder).length + (wrist - elbow).length
    return dict(blade_base_actor_xyz_m=point(base), blade_tip_actor_xyz_m=point(tip),
        blade_unit_actor_xyz=point(direction),
        left_arm_length_m=length, left_wrist_shoulder_distance_m=(wrist - shoulder).length,
        left_reach_fraction=(wrist - shoulder).length / length,
        left_palm_world_m=point(weapon.palm(rig)))


def apply_pose(rig, values):
    for name, value in values.items():
        bone = rig.pose.bones[name]
        bone.rotation_mode = "QUATERNION"
        bone.location, bone.rotation_quaternion, bone.scale = value
    bpy.context.view_layer.update()


def arm_solve(rig, side, goal, orientation):
    """Unstretched two-segment solve, using the captured elbow as its pole."""
    names = [base + "_" + side for base in ("upperarm", "lowerarm", "hand")]
    upper, lower, hand = (rig.pose.bones[name] for name in names)
    before = {name: world(rig, name) for name in names}
    a, b, c = (before[name].translation for name in names)
    first, second = (b - a).length, (c - b).length
    distance = (goal - a).length
    if not abs(first - second) + .0001 < distance < first + second - .0001:
        raise ValueError(f"Unreachable {side} held-guard arm goal: {distance} versus reach {first + second}")
    axis = (goal - a).normalized()
    pole = b - a - axis * (b - a).dot(axis)
    if pole.length < .00001:
        raise ValueError("Held-guard elbow pole is singular")
    along = (first * first + distance * distance - second * second) / (2 * distance)
    elbow = a + axis * along + pole.normalized() * math.sqrt(max(0., first * first - along * along))
    rotation = (b - a).normalized().rotation_difference((elbow - a).normalized())
    inverse = rig.matrix_world.inverted()
    upper.matrix = inverse @ Matrix.LocRotScale(a, rotation @ before[names[0]].to_quaternion(), before[names[0]].to_scale())
    bpy.context.view_layer.update()
    lower_world, hand_world = world(rig, names[1]), world(rig, names[2])
    rotation = (hand_world.translation - lower_world.translation).normalized().rotation_difference((goal - lower_world.translation).normalized())
    lower.matrix = inverse @ Matrix.LocRotScale(lower_world.translation, rotation @ lower_world.to_quaternion(), lower_world.to_scale())
    bpy.context.view_layer.update()
    hand.matrix = inverse @ Matrix.LocRotScale(goal, orientation, before[names[2]].to_scale())
    bpy.context.view_layer.update()
    after = {name: world(rig, name) for name in names}
    lengths = [(after[names[1]].translation - after[names[0]].translation).length,
               (after[names[2]].translation - after[names[1]].translation).length]
    return dict(wrist_error_m=(after[names[2]].translation - goal).length,
        segment_length_error_m=max(abs(lengths[0] - first), abs(lengths[1] - second)),
        reach_fraction=distance / (first + second))


def key_pose(rig, action, frame, values, previous):
    # Updating a partially keyed active action can immediately overwrite the
    # intended sampled channels with that action's previous held frame. Detach
    # during pose assignment, then insert without a dependency-graph resample.
    rig.animation_data.action = None
    apply_pose(rig, values)
    rig.animation_data.action = action
    for bone in rig.pose.bones:
        q = bone.rotation_quaternion.copy()
        if bone.name in previous and previous[bone.name].dot(q) < 0:
            q.negate()
        bone.rotation_quaternion = q
        previous[bone.name] = q.copy()
        for prop in ("location", "rotation_quaternion", "scale"):
            bone.keyframe_insert(prop, frame=frame, group=bone.name)
    capture._set_frame(frame)


def finish(args):
    inspection_path = args.output_dir / "guard_source_inspection.json"
    report = json.loads(inspection_path.read_text())
    bpy.ops.wm.open_mainfile(filepath=report["inspection_blend"])
    rig = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
    socket = json.loads((PACK / "derived/weapon_socket_fit.json").read_text())
    idle = bpy.data.actions["Idle"]
    idle_zero = pose(rig, idle, 1.)
    original_rest = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
    original_ids = {name: signature(bpy.data.actions[name]) for name in report["existing_actions"]}
    # The high-block capture supplies the original finger curl, natural arm
    # bend and weight-bearing stance. Its source-referenced palm correction is
    # performed before rotating/placing the two-hand hold as a rigid unit.
    source_hold = bpy.data.actions["CaptureGuard_High"]
    held_refined, held_grip_report = grip.refine(rig, source_hold, "CapturedBlock", socket, audit_only=False)
    held = pose(rig, held_refined, 1.)
    # Keep the source's captured stance but remove only its planar staging
    # offset; this is a held pose around the authoritative movement capsule.
    apply_pose(rig, held)
    pelvis = rig.pose.bones["pelvis"]
    current_pelvis = world(rig, "pelvis")
    pose(rig, idle, 1.)
    idle_pelvis = world(rig, "pelvis")
    apply_pose(rig, held)
    wanted = current_pelvis.copy()
    wanted.translation.x, wanted.translation.y = idle_pelvis.translation.x, idle_pelvis.translation.y
    pelvis.matrix = rig.matrix_world.inverted() @ wanted
    bpy.context.view_layer.update()
    held = {bone.name: bone.matrix_basis.copy().decompose() for bone in rig.pose.bones}
    # Actor coordinates: +X anatomical left, +Y up, +Z forward. Targets are
    # genuine authored directional adaptations, not mislabeled recoil clips.
    targets = {
        "Left": ((.10, 1.06, .24), (.64, .76, .10)),
        "Right": ((-.10, 1.06, .24), (-.64, .76, .10)),
        "High": ((-.08, 1.47, .24), (.98, .08, .12)),
        "Low": ((-.06, 1.12, .26), (.98, .06, .12)),
        "Thrust": ((-.10, 1.18, .28), (-.05, .97, .24)),
    }
    convert = lambda value: Vector((value[0], -value[2], value[1]))
    guards = []
    created = []
    for label, (target, direction) in targets.items():
        rig.animation_data.action = None
        apply_pose(rig, held)
        before = {name: world(rig, name) for name in ("hand_l", "hand_r", weapon.WEAPON)}
        old_weapon = before[weapon.WEAPON]
        old_base = old_weapon @ Vector((0, 0, 10.54))
        old_axis = old_weapon.to_3x3() @ Vector((0, 0, 1))
        rotation = old_axis.normalized().rotation_difference(convert(direction).normalized())
        origin = convert(target)
        delta = Matrix.Translation(origin) @ rotation.to_matrix().to_4x4() @ Matrix.Translation(-old_base)
        solves = {}
        for side in ("r", "l"):
            desired = delta @ before["hand_" + side]
            print("HELD_GUARD_SOLVE", label, side, list(desired.translation), flush=True)
            solves[side] = arm_solve(rig, side, desired.translation, desired.to_quaternion())
        hold_values = {bone.name: bone.matrix_basis.copy().decompose() for bone in rig.pose.bones}
        actual_weapon = world(rig, weapon.WEAPON)
        expected_weapon = delta @ old_weapon
        socket_error = max(abs(actual_weapon[i][j] - expected_weapon[i][j]) for i in range(4) for j in range(4))
        if socket_error > .00001:
            raise ValueError("Authored hold altered rigid source weapon attachment")
        action = bpy.data.actions.new("Guard_" + label)
        action.use_fake_user = True
        created.append(action)
        previous = {}
        first = last = None
        max_foot = max_palm = 0.
        feet = {name: world(rig, name).translation.copy() for name in ("foot_l", "foot_r")}
        palm_relative = actual_weapon.inverted() @ weapon.palm(rig)
        for index in range(91):
            frame = 1. + index
            breathing = pose(rig, idle, frame)
            values = dict(hold_values)
            # Both arms inherit one measured spine-parent breathing delta;
            # the two hands and weapon remain rigidly aligned, feet stationary.
            p, q, s = values["spine_01"]
            breathe_delta = idle_zero["spine_01"][1].inverted() @ breathing["spine_01"][1]
            values["spine_01"] = (p, q @ breathe_delta, s)
            key_pose(rig, action, frame, values, previous)
            max_foot = max([max_foot] + [(world(rig, name).translation - feet[name]).length for name in feet])
            max_palm = max(max_palm, (weapon.palm(rig) - world(rig, weapon.WEAPON) @ palm_relative).length)
            current = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
            if first is None:
                first = current
            last = current
        seam = max(abs(first[name][i][j] - last[name][i][j]) for name in first for i in range(4) for j in range(4))
        metadata = dict(name=action.name, semantic=action.name, loop=True, duration_seconds=3., output_fps=30,
            source=GUARDS["High"], source_frame=1., authored_directional_adaptation=True,
            hold_method="Captured block stance and fingers; unstretched two-bone arm placement; captured common-spine breathing",
            target_blade_base_actor_xyz_m=target, target_blade_unit_actor_xyz=Vector(direction).normalized()[:],
            foot_motion_max_m=max_foot, palm_relative_error_max_m=max_palm, loop_local_matrix_seam_max=seam,
            original_source_modified=False)
        action["valwood_clip"] = json.dumps(metadata, sort_keys=True)
        guards.append(dict(metadata=metadata, arm_solves=solves, rigid_weapon_matrix_error=socket_error, **metric(rig)))
    # Reactions retain the actual capture throughout a short, readable impact
    # and recovery. Smooth endpoints and planar staging removal avoid pops.
    hits = []
    durations = dict(Left=.50, Right=.50, High=.60, Low=.60, Thrust=.60)
    for label in HITS:
        raw = bpy.data.actions["CaptureHit_" + label]
        refined, grip_report = grip.refine(rig, raw, "CapturedHit" + label, socket, audit_only=False)
        lo, hi = refined.frame_range
        captured_first = pose(rig, refined, lo)
        apply_pose(rig, captured_first)
        planar_origin = world(rig, "pelvis").translation.copy()
        pose(rig, idle, 1.)
        origin = world(rig, "pelvis").translation.copy()
        action = bpy.data.actions.new("Hit_" + label)
        action.use_fake_user = True
        created.append(action)
        duration = durations[label]
        count = round(duration * 30)
        previous = {}
        for index in range(count + 1):
            t = index / count
            capture_phase = .12 + .78 * t
            values = pose(rig, refined, lo + (hi - lo) * capture_phase)
            current = world(rig, "pelvis")
            wanted = current.copy()
            # Gameplay owns collision-resolved displacement. Retain vertical
            # captured compression and all joint rotations; do not animate the
            # skinned body metres away from its unchanged movement capsule.
            wanted.translation.x, wanted.translation.y = origin.x, origin.y
            rig.pose.bones["pelvis"].matrix = rig.matrix_world.inverted() @ wanted
            bpy.context.view_layer.update()
            values["pelvis"] = rig.pose.bones["pelvis"].matrix_basis.decompose()
            weight = min(grip.smooth(t / .16), grip.smooth((1. - t) / .25))
            mixed = {name: (idle_zero[name][0].lerp(value[0], weight),
                idle_zero[name][1].slerp(value[1], weight), idle_zero[name][2].lerp(value[2], weight))
                for name, value in values.items()}
            key_pose(rig, action, 1. + index, mixed, previous)
        metadata = dict(name=action.name, semantic=action.name, loop=False, duration_seconds=duration, output_fps=30,
            source=HITS[label], source_phase_range=[.12, .90],
            source_sha256=json.loads(raw["valwood_clip"])["source_sha256"],
            endpoint_method="Smooth capture/neutral impact envelope; captured rotations and vertical compression; planar pelvis travel owned by gameplay",
            source_capture_grip_refinement=grip_report, original_source_modified=False)
        action["valwood_clip"] = json.dumps(metadata, sort_keys=True)
        hits.append(metadata)
    for action in created:
        changing = 0
        for curve in action.fcurves:
            for key in curve.keyframe_points:
                key.interpolation = "LINEAR"
            values = [float(key.co.y) for key in curve.keyframe_points]
            if max(values) - min(values) > .00001:
                changing += 1
        if action.name.startswith("Hit_") and changing < 20:
            raise ValueError("Reaction has no captured motion: " + action.name)
    # Inspection actions are temporary, never exported as alternate runtime
    # clips. Removing them also makes the exact twenty-five clip contract clear.
    rig.animation_data.action = None
    for action in list(bpy.data.actions):
        if action.name not in original_ids and action not in created:
            bpy.data.actions.remove(action, do_unlink=True)
    for name, digest in original_ids.items():
        if signature(bpy.data.actions[name]) != digest:
            raise ValueError("Original action altered: " + name)
    rest_error = max(abs(original_rest[name][i][j] - rig.data.bones[name].matrix_local[i][j])
        for name in original_rest for i in range(4) for j in range(4))
    if rest_error > .0000001 or any(row["metadata"]["loop_local_matrix_seam_max"] > .00001 for row in guards):
        raise ValueError("Rest matrix or held loop seam check failed")
    apply_pose(rig, {bone.name: Matrix.Identity(4).decompose() for bone in rig.pose.bones})
    bpy.context.scene.frame_set(1)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1
    output = args.output_dir / "knight_rig_polish_v2.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(output))
    final = dict(schema="valwood.modular-knight-polish-v2", input=str(args.input), input_sha256=report["input_sha256"],
        output=str(output), bones=len(rig.data.bones), clips=len(bpy.data.actions),
        original_actions_sha256=original_ids, original_actions_preserved=True, rest_matrices_max_error=rest_error,
        held_base_capture_grip=held_grip_report, guards=guards, hits=hits,
        original_sources_modified=False, collision_or_gameplay_acceptance=False)
    (args.output_dir / "animation_polish_v2_report.json").write_text(json.dumps(final, indent=2, allow_nan=False) + "\n")
    print("KNIGHT_POLISH_V2_DONE " + str(output), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=PACK / "derived/knight_rig_weapon.blend")
    parser.add_argument("--output-dir", type=Path, default=OUT)
    parser.add_argument("--inspect-only", action="store_true")
    parser.add_argument("--finish", action="store_true", help="Use the completed capture inspection derivative")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if args.finish:
        finish(args)
        return
    source_hash = hashlib.sha256(args.input.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(args.input))
    rigs = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(rigs) != 1 or len(rigs[0].data.bones) != 95:
        raise ValueError("Expected the audited ninety-five-bone armature-only derivative")
    rig = rigs[0]
    originals = {action.name: signature(action) for action in bpy.data.actions if "valwood_clip" in action}
    if len(originals) != 15:
        raise ValueError("Expected exactly the original fifteen actions")
    old_rest = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
    socket = json.loads((PACK / "derived/weapon_socket_fit.json").read_text())
    additions = {"CaptureGuard_" + label: dict(file=path, loop=False) for label, path in GUARDS.items()}
    additions.update({"CaptureHit_" + label: dict(file=path, loop=False) for label, path in HITS.items()})
    capture.CLIPS.update(additions)
    actions, retarget_report = capture.bake_actions(rig, clips=list(additions), fps=30)
    attachment = []
    inspection = []
    for label, action in actions.items():
        attachment.append(weapon.bake_clip(rig, action, socket))
        metadata = json.loads(action["valwood_clip"])
        lo, hi = action.frame_range
        samples = []
        for phase in (0., .08, .12, .18, .24, .30, .40, .5, .65, .8, 1.):
            frame = lo + phase * (hi - lo)
            pose(rig, action, frame)
            samples.append(dict(phase=phase, output_frame=frame,
                source_frame=metadata["source_frame_range"][0] + phase * (metadata["source_frame_range"][1] - metadata["source_frame_range"][0]),
                **metric(rig)))
        inspection.append(dict(name=label, metadata=metadata, samples=samples))
    report = dict(schema="valwood.modular-knight-directional-holds.v2", input=str(args.input), input_sha256=source_hash,
        original_sources_modified=False, source_pose_inspection=inspection, new_capture_retarget=retarget_report,
        new_capture_weapon=attachment, existing_actions={name: dict(sha256=digest,
            unchanged=signature(bpy.data.actions[name]) == digest) for name, digest in originals.items()})
    report["rest_matrices_max_error"] = max(abs(old_rest[name][i][j] - rig.data.bones[name].matrix_local[i][j])
        for name in old_rest for i in range(4) for j in range(4))
    if not all(row["unchanged"] for row in report["existing_actions"].values()) or report["rest_matrices_max_error"] > 1.e-7:
        raise ValueError("Original actions or rig rest changed")
    rig.animation_data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1
    bpy.context.scene.frame_set(1)
    output = args.output_dir / "capture_inspection.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(output))
    report["inspection_blend"] = str(output)
    (args.output_dir / "guard_source_inspection.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print("GUARD_SOURCE_INSPECTION_DONE " + str(args.output_dir), flush=True)


if __name__ == "__main__":
    main()
