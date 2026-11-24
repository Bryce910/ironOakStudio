"""Optional captured-reference left-arm grip refinement on a derivative rig.

Blender --background --python-exit-code 1 --python this_file -- --audit-only [--clips Idle ...]
Blender --background --python-exit-code 1 --python this_file -- --locomotion-only
The purchased sources and input .blend are read only. Only the left upperarm,
lowerarm and hand rotation channels are changed in the output .blend. Source
weapon-local palm motion preserves intentional release rather than pinning the
left hand to an arbitrary handle point.
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

PACK = TOOLS.parent / "assets/purchased/modular_knights"
EDITED = ("upperarm_l", "lowerarm_l", "hand_l")


def matrix(rows):
    return Matrix(rows)


def smooth(value):
    value = min(1., max(0., value))
    return value * value * (3. - 2. * value)


def release_weight(local, source_weapon):
    # Actual owned sword handle: roughly -.17m to+.075m in weapon-local Z.
    # The reference is the captured palm, not a wrist that naturally sits away
    # from the sword. Fade only at the source capture's contact envelope.
    scale = source_weapon.to_scale()
    radial = math.hypot(local.x * scale.x, local.y * scale.y)
    z = local.z * scale.z
    amount = min(smooth((.12 - radial) / .04), smooth((z + .23) / .04), smooth((.14 - z) / .04))
    return amount, radial, z


def palm(rig):
    return rig.matrix_world @ ((rig.pose.bones["index_01_l"].matrix.translation
                              + rig.pose.bones["ring_01_l"].matrix.translation) * .5)


def world(rig, name):
    return rig.matrix_world @ rig.pose.bones[name].matrix


def curve_signature(action, exclude=()):
    rows = []
    for curve in action.fcurves:
        if any(curve.data_path == f'pose.bones["{name}"].rotation_quaternion' for name in exclude):
            continue
        rows.append((curve.data_path, curve.array_index,
                     [(tuple(k.co), tuple(k.handle_left), tuple(k.handle_right), k.interpolation)
                      for k in curve.keyframe_points]))
    return hashlib.sha256(repr(sorted(rows)).encode()).hexdigest()


def solve(rig, goal, baseline):
    """Rotate a two-segment arm with its original elbow pole; never stretch."""
    inverse = rig.matrix_world.inverted()
    upper, lower, hand = (rig.pose.bones[name] for name in EDITED)
    a, b, c = (baseline[name].translation for name in EDITED)
    first, second = (b - a).length, (c - b).length
    delta = goal - a
    distance = delta.length
    if min(first, second, distance) < 1.e-8:
        return dict(applied=False, reason="degenerate chain")
    if distance >= first + second - .0001 or distance <= abs(first - second) + .0001:
        return dict(applied=False, reason="goal outside unstretched chain reach")
    axis = delta / distance
    pole = b - a - axis * (b - a).dot(axis)
    if pole.length < .00001:
        return dict(applied=False, reason="original elbow pole is singular")
    pole.normalize()
    along = (first * first + distance * distance - second * second) / (2 * distance)
    height = math.sqrt(max(0., first * first - along * along))
    elbow = a + axis * along + pole * height
    rotation = (b - a).normalized().rotation_difference((elbow - a).normalized())
    upper.matrix = inverse @ Matrix.LocRotScale(a, rotation @ baseline["upperarm_l"].to_quaternion(), baseline["upperarm_l"].to_scale())
    bpy.context.view_layer.update()
    current_lower, current_hand = world(rig, "lowerarm_l"), world(rig, "hand_l")
    rotation = (current_hand.translation - current_lower.translation).normalized().rotation_difference((goal - current_lower.translation).normalized())
    lower.matrix = inverse @ Matrix.LocRotScale(current_lower.translation, rotation @ current_lower.to_quaternion(), current_lower.to_scale())
    bpy.context.view_layer.update()
    hand.matrix = inverse @ Matrix.LocRotScale(goal, baseline["hand_l"].to_quaternion(), baseline["hand_l"].to_scale())
    bpy.context.view_layer.update()
    after = {name: world(rig, name) for name in EDITED}
    first_after = (after["lowerarm_l"].translation - after["upperarm_l"].translation).length
    second_after = (after["hand_l"].translation - after["lowerarm_l"].translation).length
    return dict(applied=True, segment_length_error_m=max(abs(first_after - first), abs(second_after - second)),
                wrist_error_m=(after["hand_l"].translation - goal).length,
                hand_world_rotation_error_degrees=math.degrees(after["hand_l"].to_quaternion().rotation_difference(baseline["hand_l"].to_quaternion()).angle))


def refine(rig, source_action, label, socket, *, audit_only=False, max_correction=.12):
    metadata = json.loads(source_action["valwood_clip"])
    imported, added_objects, added_actions = capture._import_source(Path(metadata["source"]))
    required = ("weapon_r", "hand_l", "index_01_l", "ring_01_l")
    if any(name not in imported.pose.bones for name in required):
        raise ValueError("Captured reference lacks palm/weapon joints")
    derivative = source_action.copy() if not audit_only else None
    if derivative:
        derivative.name = label + "_GripRefined"
        derivative.use_fake_user = True
    protected_before = curve_signature(source_action, EDITED)
    frames = sorted({float(key.co.x) for curve in source_action.fcurves for key in curve.keyframe_points})
    start, end = metadata["source_frame_range"]
    period = end - start
    source_capture_action = imported.animation_data.action
    correction = None
    if metadata["loop"] and metadata["seam_frames"]:
        first, last = capture._sample_source(imported, start), capture._sample_source(imported, end)
        correction = {name: (first[name][0] - last[name][0], last[name][1].inverted() @ first[name][1], first[name][2] - last[name][2]) for name in first}
    # Recreate the actual per-file hand bind compensation used by the retarget
    # baker; an Idle-only attachment can hide a different attack-file rest basis.
    target_rest, source_rest = capture._rest(rig), capture._rest(imported)
    identity = {name: name for name in capture.TARGET_TO_SOURCE.values()}
    reverse = {source: target for target, source in capture.TARGET_TO_SOURCE.items()}
    conversion = capture._body_basis(target_rest, reverse) @ capture._body_basis(source_rest, identity).transposed()
    original = conversion.to_quaternion() @ source_rest["hand_r"].to_quaternion()
    a = conversion @ (source_rest["middle_01_r"].translation - source_rest["hand_r"].translation)
    b = target_rest["middle_01_r"].translation - target_rest["hand_r"].translation
    compensation = (a.normalized().rotation_difference(b.normalized()) @ original).inverted() @ target_rest["hand_r"].to_quaternion()
    source_rest_socket = source_rest["hand_r"].inverted() @ source_rest["weapon_r"]
    report = dict(name=label, samples=len(frames), applied=0, source_release=0, excessive_correction=0,
                  unreachable=0, max_requested_wrist_correction_m=0., max_applied_wrist_correction_m=0.,
                  max_palm_error_before_m=0., max_palm_error_after_m=0., max_segment_length_error_m=0.,
                  max_goal_outside_arm_reach_m=0., target_arm_length_range_m=[float("inf"), -float("inf")],
                  source_arm_length_range_m=[float("inf"), -float("inf")],
                  max_weapon_relative_shoulder_difference_m=0., max_anatomical_palm_offset_difference_m=0.,
                  max_source_hand_relative_weapon_rotation_drift_degrees=0., max_source_hand_relative_weapon_translation_drift_m=0.,
                  max_within_clip_socket_rotation_drift_degrees=0., max_within_clip_socket_translation_drift_m=0.,
                  target_right_hand_bind_compensation=list(compensation),
                  target_hand_bind_vs_idle_degrees=math.degrees(compensation.rotation_difference(Quaternion(socket["target_hand_bind_compensation_quaternion"])).angle),
                  source_rest_socket_vs_idle_degrees=math.degrees(source_rest_socket.to_quaternion().rotation_difference(matrix(socket["source_hand_local_socket"]).to_quaternion()).angle),
                  max_hand_world_rotation_error_degrees=0., max_protected_world_error=0.,
                  max_protected_world_error_bone=None,
                  source_radial_range_m=[float("inf"), -float("inf")], source_handle_z_range_m=[float("inf"), -float("inf")],
                  source_release_frames=[], skipped_frames=[])
    previous_quaternions = {}
    first_relative_weapon = None
    try:
        for frame in frames:
            seconds = (frame - 1.) / metadata["output_fps"]
            source_frame = start + ((seconds * 30. + metadata["source_phase_frame"] - start) % period) if metadata["loop"] else min(end, start + seconds * 30.)
            imported.animation_data.action = source_capture_action
            capture._source_pose(imported, source_frame, start, end, correction, min(metadata["seam_frames"], period))
            source_weapon = world(imported, "weapon_r")
            source_hand = world(imported, "hand_r")
            relative_weapon = source_hand.inverted() @ source_weapon
            if first_relative_weapon is None: first_relative_weapon = relative_weapon.copy()
            reference_socket = matrix(socket["source_hand_local_socket"])
            report["max_source_hand_relative_weapon_rotation_drift_degrees"] = max(report["max_source_hand_relative_weapon_rotation_drift_degrees"],
                math.degrees(relative_weapon.to_quaternion().rotation_difference(reference_socket.to_quaternion()).angle))
            report["max_source_hand_relative_weapon_translation_drift_m"] = max(report["max_source_hand_relative_weapon_translation_drift_m"],
                source_hand.to_3x3().to_scale().length / math.sqrt(3) * (relative_weapon.translation - reference_socket.translation).length)
            report["max_within_clip_socket_rotation_drift_degrees"] = max(report["max_within_clip_socket_rotation_drift_degrees"],
                math.degrees(relative_weapon.to_quaternion().rotation_difference(first_relative_weapon.to_quaternion()).angle))
            report["max_within_clip_socket_translation_drift_m"] = max(report["max_within_clip_socket_translation_drift_m"],
                source_hand.to_3x3().to_scale().length / math.sqrt(3) * (relative_weapon.translation - first_relative_weapon.translation).length)
            source_arm = sum((world(imported, b).translation - world(imported, a).translation).length
                             for a, b in (("upperarm_l", "lowerarm_l"), ("lowerarm_l", "hand_l")))
            source_palm_local = source_weapon.inverted() @ palm(imported)
            source_shoulder_local = source_weapon.inverted() @ world(imported, "upperarm_l").translation
            source_wrist_local = source_weapon.inverted() @ world(imported, "hand_l").translation
            amount, radial, z = release_weight(source_palm_local, source_weapon)
            report["source_radial_range_m"] = [min(report["source_radial_range_m"][0], radial), max(report["source_radial_range_m"][1], radial)]
            report["source_handle_z_range_m"] = [min(report["source_handle_z_range_m"][0], z), max(report["source_handle_z_range_m"][1], z)]
            rig.animation_data.action = source_action
            capture._set_frame(frame)
            baseline_basis = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
            baseline_world = {bone.name: world(rig, bone.name) for bone in rig.pose.bones}
            current_palm = palm(rig)
            target_weapon = (baseline_world["valwood_weapon_r"] if "valwood_weapon_r" in baseline_world
                             else baseline_world[socket["target_bone"]] @ matrix(socket["target_hand_local_socket"]))
            desired_palm = target_weapon @ source_palm_local
            offset = current_palm - baseline_world["hand_l"].translation
            desired_wrist = desired_palm - offset
            report["max_weapon_relative_shoulder_difference_m"] = max(report["max_weapon_relative_shoulder_difference_m"],
                (baseline_world["upperarm_l"].translation - target_weapon @ source_shoulder_local).length)
            report["max_anatomical_palm_offset_difference_m"] = max(report["max_anatomical_palm_offset_difference_m"],
                (desired_wrist - target_weapon @ source_wrist_local).length)
            target_arm = sum((baseline_world[b].translation - baseline_world[a].translation).length
                             for a, b in (("upperarm_l", "lowerarm_l"), ("lowerarm_l", "hand_l")))
            reach_excess = max(0., (desired_wrist - baseline_world["upperarm_l"].translation).length - target_arm)
            report["max_goal_outside_arm_reach_m"] = max(report["max_goal_outside_arm_reach_m"], reach_excess)
            report["target_arm_length_range_m"] = [min(report["target_arm_length_range_m"][0], target_arm), max(report["target_arm_length_range_m"][1], target_arm)]
            report["source_arm_length_range_m"] = [min(report["source_arm_length_range_m"][0], source_arm), max(report["source_arm_length_range_m"][1], source_arm)]
            requested = (desired_wrist - baseline_world["hand_l"].translation).length
            report["max_requested_wrist_correction_m"] = max(report["max_requested_wrist_correction_m"], requested)
            report["max_palm_error_before_m"] = max(report["max_palm_error_before_m"], (current_palm - desired_palm).length)
            reason = None
            if amount < .0001:
                reason = "authored source release"
                report["source_release"] += 1
                report["source_release_frames"].append(frame)
            elif requested > max_correction:
                reason = "correction exceeds safety envelope"
                report["excessive_correction"] += 1
            if audit_only:
                continue
            rig.animation_data.action = derivative
            for bone in rig.pose.bones:
                bone.matrix_basis = baseline_basis[bone.name]
            bpy.context.view_layer.update()
            if reason is None:
                goal = baseline_world["hand_l"].translation.lerp(desired_wrist, amount)
                result = solve(rig, goal, baseline_world)
                if result["applied"]:
                    report["applied"] += 1
                    report["max_applied_wrist_correction_m"] = max(report["max_applied_wrist_correction_m"], (goal - baseline_world["hand_l"].translation).length)
                    report["max_segment_length_error_m"] = max(report["max_segment_length_error_m"], result["segment_length_error_m"])
                    report["max_hand_world_rotation_error_degrees"] = max(report["max_hand_world_rotation_error_degrees"], result["hand_world_rotation_error_degrees"])
                    if result["segment_length_error_m"] > .00001 or result["wrist_error_m"] > .00001 or result["hand_world_rotation_error_degrees"] > .1:
                        raise ValueError("Two-bone solve failed preservation tolerances: " + str(result))
                else:
                    reason = result["reason"]
                    report["unreachable"] += 1
            if reason:
                report["skipped_frames"].append(dict(frame=frame, reason=reason))
            for name in EDITED:
                bone = rig.pose.bones[name]
                bone.rotation_mode = "QUATERNION"
                old = previous_quaternions.get(name)
                if old is not None and old.dot(bone.rotation_quaternion) < 0: bone.rotation_quaternion.negate()
                previous_quaternions[name] = bone.rotation_quaternion.copy()
                bone.keyframe_insert("rotation_quaternion", frame=frame, group=name)
            # Channels other than these three rotations may not change. In
            # particular do not key hand translation to disguise arm stretch.
            for name, before in baseline_world.items():
                bone = rig.pose.bones[name]
                if name == "upperarm_l" or any(parent.name == "upperarm_l" for parent in bone.parent_recursive):
                    continue
                after = world(rig, name)
                error = max(abs(before[i][j] - after[i][j]) for i in range(4) for j in range(4))
                if error > report["max_protected_world_error"]:
                    report["max_protected_world_error"] = error
                    report["max_protected_world_error_bone"] = name
            report["max_palm_error_after_m"] = max(report["max_palm_error_after_m"], (palm(rig) - desired_palm).length)
        if derivative:
            for curve in derivative.fcurves:
                if any(curve.data_path == f'pose.bones["{name}"].rotation_quaternion' for name in EDITED):
                    for key in curve.keyframe_points: key.interpolation = "LINEAR"
            if curve_signature(derivative, EDITED) != protected_before:
                raise ValueError("A protected action channel changed")
            if report["max_protected_world_error"] > .00001:
                raise ValueError("Right arm, torso, or lower-body world pose changed: " + str(report))
            metadata["foot_or_grip_ik_applied"] = True
            metadata["left_grip_refinement"] = report
            derivative["valwood_clip"] = json.dumps(metadata, sort_keys=True)
        return derivative, report
    finally:
        capture._remove_source(added_objects, added_actions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=PACK / "derived/knight_rig_actions.blend")
    parser.add_argument("--output", type=Path, default=PACK / "derived/knight_rig_grip_locomotion.blend")
    parser.add_argument("--report", type=Path, default=PACK / "derived/grip_locomotion_report.json")
    parser.add_argument("--socket", type=Path, default=PACK / "derived/weapon_socket_fit.json")
    parser.add_argument("--clips", nargs="+")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--audit-only", action="store_true", help="Measure captured-reference mismatch; never save a modified rig")
    mode.add_argument("--locomotion-only", action="store_true", help="Refine Idle/eight walks; preserve Run releases and all attack actions unchanged")
    parser.add_argument("--max-correction", type=float, default=.12)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    for output in ((args.report,) if args.audit_only else (args.output, args.report)):
        if args.input.resolve() == output.resolve() or (PACK / "source").resolve() in output.resolve().parents:
            raise ValueError("Outputs must be new derivatives outside purchased source")
    if not math.isfinite(args.max_correction) or not 0 < args.max_correction <= .12:
        raise ValueError("Maximum correction must be finite and in (0,.12] metres")
    input_hash = hashlib.sha256(args.input.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(args.input))
    rigs = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    if len(rigs) != 1: raise ValueError("Expected one target rig")
    rig = rigs[0]
    for name in (*EDITED, "index_01_l", "ring_01_l", "hand_r"):
        if name not in rig.pose.bones: raise ValueError("Missing target grip bone " + name)
    actions = {action.name: action for action in bpy.data.actions if "valwood_clip" in action}
    labels = args.clips or sorted(actions)
    if args.locomotion_only:
        if args.clips and any(name != "Idle" and name != "Run" and not name.startswith("Walk_") for name in args.clips):
            raise ValueError("Attack correction is not released; use --audit-only for attack analysis")
        labels = [name for name in labels if name in ("Idle", "Run") or name.startswith("Walk_")]
    if any(name not in actions for name in labels): raise ValueError("Requested action is not in input rig")
    socket = json.loads(args.socket.read_text())
    report = dict(schema="valwood.modular-knight-grip-refinement.v1", input=str(args.input), input_sha256=input_hash,
                  source_files_modified=False, original_rig_modified=False, audit_only=args.audit_only,
                  method="Captured index/ring palm centre in source weapon-local coordinates; original elbow-pole two-bone rotations; left world hand orientation retained",
                  max_allowed_correction_m=args.max_correction, clips=[])
    report["mode"] = "locomotion-only" if args.locomotion_only else "audit-only"
    report["target_weapon_reference"] = "animated valwood_weapon_r" if "valwood_weapon_r" in rig.pose.bones else "fixed measured Idle hand socket"
    report["unmodified_attack_clips"] = sorted(name for name in actions if name.startswith("Attack_"))
    report["unresolved_attack_risks"] = ["Captured weapon-local palm targets can exceed the target left arm's unstretched reach or the12cm correction envelope.",
        "No partially solved or frame-skipped attack correction is saved; original attack actions are retained exactly."]
    for label in labels:
        release_only = args.locomotion_only and label == "Run"
        derivative, row = refine(rig, actions[label], label, socket, audit_only=args.audit_only or release_only, max_correction=args.max_correction)
        if args.locomotion_only and not release_only and (row["excessive_correction"] or row["unreachable"]):
            raise ValueError("Locomotion clip requires unsafe partial correction: " + label)
        row["retained_original"] = derivative is None
        if release_only: row["preservation_reason"] = "Authored one-handed running release; entire original action retained"
        report["clips"].append(row)
        if derivative:
            rig.animation_data.action = None
            bpy.data.actions.remove(actions[label], do_unlink=True)
            derivative.name = label
        print("GRIP_CLIP " + json.dumps(row), flush=True)
    if hashlib.sha256(args.input.read_bytes()).hexdigest() != input_hash: raise ValueError("Input rig file unexpectedly changed")
    if not args.audit_only:
        rig.animation_data.action = None
        for bone in rig.pose.bones: bone.matrix_basis = Matrix.Identity(4)
        bpy.context.scene.frame_set(1)
        bpy.context.scene.render.fps = 30
        bpy.context.scene.render.fps_base = 1
        args.output.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(args.output))
        report["output"] = str(args.output)
        report["output_bytes"] = args.output.stat().st_size
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("GRIP_REFINEMENT_DONE " + json.dumps(dict(audit_only=args.audit_only, clips=len(report["clips"]), report=str(args.report))))


if __name__ == "__main__":
    main()
