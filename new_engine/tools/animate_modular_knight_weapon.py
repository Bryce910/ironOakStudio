"""Restore owned capture's animated weapon attachment on a derivative joint.

Blender --background --python-exit-code 1 --python this_file -- [options]
Input locomotion/body curves and purchased sources stay unchanged. A single
child joint adds the captured hand-relative weapon motion omitted by a fixed
Idle socket. This does not move either arm to hide a weapon attachment error.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import retarget_modular_knight as capture

PACK = TOOLS.parent / "assets/purchased/modular_knights"
WEAPON = "valwood_weapon_r"


def world(rig, name):
    return rig.matrix_world @ rig.pose.bones[name].matrix


def palm(rig):
    return rig.matrix_world @ ((rig.pose.bones["index_01_l"].matrix.translation
                               + rig.pose.bones["ring_01_l"].matrix.translation) * .5)


def rotation_error(a, b):
    return math.degrees(2 * math.acos(min(1., abs(a.normalized().dot(b.normalized())))))


def curve_signature(action):
    rows = []
    for curve in action.fcurves:
        if curve.data_path.startswith(f'pose.bones["{WEAPON}"]'): continue
        rows.append((curve.data_path, curve.array_index,
                     [(tuple(k.co), tuple(k.handle_left), tuple(k.handle_right), k.interpolation) for k in curve.keyframe_points]))
    return hashlib.sha256(repr(sorted(rows)).encode()).hexdigest()


def compensation(rig, source):
    target_rest, source_rest = capture._rest(rig), capture._rest(source)
    reverse = {source: target for target, source in capture.TARGET_TO_SOURCE.items()}
    identity = {name: name for name in capture.TARGET_TO_SOURCE.values()}
    conversion = capture._body_basis(target_rest, reverse) @ capture._body_basis(source_rest, identity).transposed()
    original = conversion.to_quaternion() @ source_rest["hand_r"].to_quaternion()
    a = conversion @ (source_rest["middle_01_r"].translation - source_rest["hand_r"].translation)
    b = target_rest["middle_01_r"].translation - target_rest["hand_r"].translation
    return (a.normalized().rotation_difference(b.normalized()) @ original).inverted() @ target_rest["hand_r"].to_quaternion()


def append_joint(rig, socket):
    if WEAPON in rig.data.bones: raise ValueError("Input already contains the derived weapon joint")
    rig.animation_data.action = None
    for bone in rig.pose.bones: bone.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    try:
        bone = rig.data.edit_bones.new(WEAPON)
        bone.parent = rig.data.edit_bones[socket["target_bone"]]
        bone.use_connect = False
        bone.use_deform = True
        bone.matrix = bone.parent.matrix @ Matrix(socket["target_hand_local_socket"])
        bone.length = 8.0  # Display only, in the existing centimetre rig units.
    finally:
        bpy.ops.object.mode_set(mode="OBJECT")
    rig.pose.bones[WEAPON].rotation_mode = "QUATERNION"


def bake_clip(rig, action, socket):
    metadata = json.loads(action["valwood_clip"])
    frames = sorted({float(key.co.x) for curve in action.fcurves for key in curve.keyframe_points})
    body_before = curve_signature(action)
    source_path = Path(metadata["source"])
    source_hash = hashlib.sha256(source_path.read_bytes()).hexdigest()
    source, added_objects, added_actions = capture._import_source(source_path)
    source_action = source.animation_data.action
    bind = compensation(rig, source)
    start, end = metadata["source_frame_range"]
    period = end - start
    correction = None
    if metadata["loop"] and metadata["seam_frames"]:
        first, last = capture._sample_source(source, start), capture._sample_source(source, end)
        correction = {name: (first[name][0] - last[name][0], last[name][1].inverted() @ first[name][1], first[name][2] - last[name][2]) for name in first}
    hand = rig.data.bones[socket["target_bone"]]
    rest_attach = hand.matrix_local.inverted() @ rig.data.bones[WEAPON].matrix_local
    row = dict(name=action.name, samples=len(frames), source_sha256=source_hash, source_hash_matches_retarget=source_hash == metadata["source_sha256"],
               max_weapon_world_matrix_error=0., max_left_palm_error_m=0., max_left_wrist_correction_m=0.,
               max_left_goal_outside_arm_reach_m=0., unreachable_left_goal_samples=0,
               max_source_hand_relative_rotation_drift_degrees=0., max_source_hand_relative_translation_drift_m=0.,
               weapon_local_loop_seam_translation_m=None, weapon_local_loop_seam_rotation_degrees=None,
               weapon_tip_loop_seam_m=None, existing_body_curves_unchanged=False)
    first_relative = None
    first_basis = last_basis = first_tip = last_tip = None
    previous_quaternion = None
    try:
        for frame in frames:
            seconds = (frame - 1.) / metadata["output_fps"]
            source_frame = start + ((seconds * 30. + metadata["source_phase_frame"] - start) % period) if metadata["loop"] else min(end, start + seconds * 30.)
            source.animation_data.action = source_action
            capture._source_pose(source, source_frame, start, end, correction, min(metadata["seam_frames"], period))
            source_hand, source_weapon = world(source, "hand_r"), world(source, "weapon_r")
            relative = source_hand.inverted() @ source_weapon
            source_palm_local = source_weapon.inverted() @ palm(source)
            if first_relative is None: first_relative = relative.copy()
            row["max_source_hand_relative_rotation_drift_degrees"] = max(row["max_source_hand_relative_rotation_drift_degrees"], rotation_error(relative.to_quaternion(), first_relative.to_quaternion()))
            row["max_source_hand_relative_translation_drift_m"] = max(row["max_source_hand_relative_translation_drift_m"],
                (source_hand.to_3x3() @ (relative.translation - first_relative.translation)).length)
            desired_attach = Matrix.LocRotScale(bind.inverted() @ relative.translation * socket["anatomical_palm_scale"],
                                               bind.inverted() @ relative.to_quaternion(), relative.to_scale())
            rig.animation_data.action = action
            capture._set_frame(frame)
            bone = rig.pose.bones[WEAPON]
            basis = rest_attach.inverted() @ desired_attach
            position, rotation, scaling = basis.decompose()
            rotation.normalize()
            if previous_quaternion is not None and previous_quaternion.dot(rotation) < 0: rotation.negate()
            previous_quaternion = rotation.copy()
            bone.location, bone.rotation_quaternion, bone.scale = position, rotation, scaling
            bpy.context.view_layer.update()
            expected_world = world(rig, socket["target_bone"]) @ desired_attach
            actual_world = world(rig, WEAPON)
            row["max_weapon_world_matrix_error"] = max(row["max_weapon_world_matrix_error"],
                max(abs(expected_world[i][j] - actual_world[i][j]) for i in range(4) for j in range(4)))
            desired_palm = actual_world @ source_palm_local
            wrist = world(rig, "hand_l").translation
            wrist_goal = desired_palm - (palm(rig) - wrist)
            row["max_left_palm_error_m"] = max(row["max_left_palm_error_m"], (palm(rig) - desired_palm).length)
            row["max_left_wrist_correction_m"] = max(row["max_left_wrist_correction_m"], (wrist_goal - wrist).length)
            shoulder = world(rig, "upperarm_l").translation
            elbow = world(rig, "lowerarm_l").translation
            arm_length = (elbow - shoulder).length + (wrist - elbow).length
            excess = max(0., (wrist_goal - shoulder).length - arm_length)
            row["max_left_goal_outside_arm_reach_m"] = max(row["max_left_goal_outside_arm_reach_m"], excess)
            if excess > .0001: row["unreachable_left_goal_samples"] += 1
            for property_name in ("location", "rotation_quaternion", "scale"):
                bone.keyframe_insert(property_name, frame=frame, group=WEAPON)
            # One metre along the source weapon's +Z blade direction, expressed
            # in existing rig centimetres. It detects attachment-loop discontinuity.
            tip = actual_world @ Vector((0, 0, 100))
            if first_basis is None: first_basis, first_tip = basis.copy(), tip.copy()
            last_basis, last_tip = basis.copy(), tip.copy()
        for curve in action.fcurves:
            if curve.data_path.startswith(f'pose.bones["{WEAPON}"]'):
                for key in curve.keyframe_points: key.interpolation = "LINEAR"
        if metadata["loop"]:
            row["weapon_local_loop_seam_translation_m"] = ((first_basis.translation - last_basis.translation) * .01).length
            row["weapon_local_loop_seam_rotation_degrees"] = rotation_error(first_basis.to_quaternion(), last_basis.to_quaternion())
            row["weapon_tip_loop_seam_m"] = (first_tip - last_tip).length
        row["existing_body_curves_unchanged"] = body_before == curve_signature(action)
        if not row["existing_body_curves_unchanged"] or not row["source_hash_matches_retarget"] or row["max_weapon_world_matrix_error"] > .00001:
            raise ValueError("Weapon export preservation/fit check failed: " + str(row))
        if metadata["loop"] and (row["weapon_local_loop_seam_translation_m"] > .00001 or row["weapon_local_loop_seam_rotation_degrees"] > .1):
            raise ValueError("Captured weapon local loop does not close: " + str(row))
        metadata["animated_weapon_joint"] = WEAPON
        metadata["animated_weapon_source"] = "Actual sampled source hand_r^-1*weapon_r; per-file bind compensation and existing palm ratio"
        action["valwood_clip"] = json.dumps(metadata, sort_keys=True)
        return row
    finally:
        capture._remove_source(added_objects, added_actions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=PACK / "derived/knight_rig_grip_locomotion.blend")
    parser.add_argument("--output", type=Path, default=PACK / "derived/knight_rig_weapon.blend")
    parser.add_argument("--report", type=Path, default=PACK / "derived/weapon_animation_report.json")
    parser.add_argument("--socket", type=Path, default=PACK / "derived/weapon_socket_fit.json")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    for output in (args.output, args.report):
        if output.resolve() == args.input.resolve() or (PACK / "source").resolve() in output.resolve().parents:
            raise ValueError("Outputs must be new derivatives outside purchased source")
    input_hash = hashlib.sha256(args.input.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(args.input))
    rigs = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(rigs) != 1: raise ValueError("Expected one target rig")
    rig = rigs[0]
    actions = sorted([action for action in bpy.data.actions if "valwood_clip" in action], key=lambda a: a.name)
    if len(actions) != 15: raise ValueError("Expected all15 audited body actions")
    old_rest = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
    socket = json.loads(args.socket.read_text())
    append_joint(rig, socket)
    rest_error = max(abs(old_rest[name][i][j] - rig.data.bones[name].matrix_local[i][j]) for name in old_rest for i in range(4) for j in range(4))
    if rest_error > .0001: raise ValueError("Adding weapon joint altered existing bone rest matrices")
    report = dict(schema="valwood.modular-knight-animated-weapon.v1", input=str(args.input), input_sha256=input_hash,
                  original_sources_modified=False, existing_rest_max_error_rig_units=rest_error,
                  added_joint=WEAPON, added_joint_parent=socket["target_bone"], total_bones=len(rig.data.bones),
                  source_weapon_axes=dict(blade="+Z", crossguard="+Y", thickness="+X"),
                  body_curves_modified=False, attack_grip_ik_applied=False, clips=[])
    if len(rig.data.bones) > 128: raise ValueError("Derived rig exceeds runtime joint budget")
    for action in actions:
        row = bake_clip(rig, action, socket)
        report["clips"].append(row)
        print("ANIMATED_WEAPON_CLIP " + json.dumps(row), flush=True)
    rig.animation_data.action = None
    for bone in rig.pose.bones: bone.matrix_basis = Matrix.Identity(4)
    bpy.context.scene.frame_set(1)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1
    if hashlib.sha256(args.input.read_bytes()).hexdigest() != input_hash: raise ValueError("Input rig file unexpectedly changed")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(args.output))
    report["output"] = str(args.output)
    report["output_bytes"] = args.output.stat().st_size
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("ANIMATED_WEAPON_DONE " + json.dumps(dict(joints=report["total_bones"], clips=len(report["clips"]), output=str(args.output))))


if __name__ == "__main__":
    main()
