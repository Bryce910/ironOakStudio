"""Source-referenced attack offhand fitting with a bounded clavicle rotation.

Run inside Blender. The accepted v2 asset and purchased sources are read only.
Only four left-clavicle/arm rotation channels may change on the five attacks.
Clavicle placement and both arm segments retain their original lengths.
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
import refine_modular_knight_grip as grip

PACK = TOOLS.parent / "assets/purchased/modular_knights"
EDITED = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l")
PLAY_WINDOWS = {"Attack_Left": (40., 73.), "Attack_Right": (38., 69.), "Attack_High": (31., 64.),
                "Attack_Low": (13., 40.), "Attack_Thrust": (17., 51.)}


def digest(action, exclude=()):
    rows = []
    for curve in action.fcurves:
        if any(curve.data_path == f'pose.bones["{name}"].rotation_quaternion' for name in exclude):
            continue
        rows.append((curve.data_path, curve.array_index,
            [(tuple(key.co), tuple(key.handle_left), tuple(key.handle_right), key.interpolation)
             for key in curve.keyframe_points]))
    return hashlib.sha256(repr(sorted(rows)).encode()).hexdigest()


def apply(rig, values):
    rig.animation_data.action = None
    for name, value in values.items():
        rig.pose.bones[name].matrix_basis = value
    bpy.context.view_layer.update()


def angle(a, b):
    return math.degrees(2 * math.acos(min(1., abs(a.normalized().dot(b.normalized())))))


def shoulder_fit(rig, goal, max_degrees, reach_fraction):
    """Smallest clavicle swing that puts the wrist inside the arm sphere.

    Shoulder sits on the original clavicle sphere. Only rotate that sphere;
    never change its radius, joint translations, or an arm segment length.
    """
    clavicle = grip.world(rig, "clavicle_l")
    shoulder, elbow, wrist = (grip.world(rig, name).translation for name in ("upperarm_l", "lowerarm_l", "hand_l"))
    arm_length = (elbow - shoulder).length + (wrist - elbow).length
    target_length = arm_length * reach_fraction
    direct = (goal - shoulder).length
    anchor = clavicle.translation
    bone_vector = shoulder - anchor
    radius = bone_vector.length
    target_vector = goal - anchor
    distance = target_vector.length
    rotation = Quaternion((1, 0, 0, 0))
    degrees = 0.
    if direct > target_length:
        if min(radius, distance) < .00001:
            raise ValueError("Degenerate clavicle sphere")
        cosine = (distance * distance + radius * radius - target_length * target_length) / (2 * distance * radius)
        if cosine > 1.:
            raise ValueError("Wrist goal is outside clavicle plus unstretched arm reach")
        cosine = max(-1., cosine)
        original = bone_vector.normalized()
        target = target_vector.normalized()
        current_angle = math.acos(max(-1., min(1., original.dot(target))))
        permitted_angle = math.acos(cosine)
        required = max(0., current_angle - permitted_angle)
        degrees = math.degrees(required)
        if degrees > max_degrees + .00001:
            raise ValueError(f"Clavicle correction {degrees:.4f} exceeds bound {max_degrees}")
        full = original.rotation_difference(target)
        rotation = Quaternion((1, 0, 0, 0)).slerp(full, required / max(.0000001, current_angle))
        new_world = Matrix.LocRotScale(anchor, rotation @ clavicle.to_quaternion(), clavicle.to_scale())
        rig.pose.bones["clavicle_l"].matrix = rig.matrix_world.inverted() @ new_world
        bpy.context.view_layer.update()
    new_shoulder = grip.world(rig, "upperarm_l").translation
    if (goal - new_shoulder).length > target_length + .00001:
        raise ValueError("Clavicle solve did not reach its analytic arm sphere")
    return dict(delta_degrees=degrees, clavicle_length_m=radius,
        shoulder_displacement_m=(new_shoulder - shoulder).length,
        arm_length_m=arm_length, requested_reach_fraction=direct / arm_length,
        solved_reach_fraction=(goal - new_shoulder).length / arm_length)


def refine(rig, action, args):
    metadata = json.loads(action["valwood_clip"])
    path = Path(metadata["source"])
    expected_hash = metadata["source_sha256"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash:
        raise ValueError("Owned source hash changed")
    imported, objects, source_actions = capture._import_source(path)
    source_action = imported.animation_data.action
    derivative = action.copy()
    derivative.name = action.name + "_OffhandV3"
    derivative.use_fake_user = True
    for curve in list(derivative.fcurves):
        if any(curve.data_path == f'pose.bones["{name}"].rotation_quaternion' for name in EDITED):
            derivative.fcurves.remove(curve)
    protected = digest(action, EDITED)
    start, end = metadata["source_frame_range"]
    duration = metadata["duration_seconds"]
    frames = [1. + index * 30. / args.sample_hz for index in range(round(duration * args.sample_hz) + 1)]
    previous = {}
    previous_baseline = {}
    previous_30 = {}
    previous_baseline_30 = {}
    previous_60 = {}
    previous_baseline_60 = {}
    row = dict(name=action.name, samples=len(frames), source=str(path), source_sha256=expected_hash,
        sample_hz=args.sample_hz, edited_rotation_bones=list(EDITED), max_clavicle_degrees=0., max_clavicle_displacement_m=0.,
        max_palm_error_before_m=0., max_palm_error_after_m=0., max_arm_segment_error_m=0.,
        max_clavicle_segment_error_m=0., max_preserved_hand_rotation_error_degrees=0.,
        max_protected_world_error=0., max_solved_reach_fraction=0., max_requested_reach_fraction=0.,
        max_clavicle_quaternion_step_degrees=0., max_elbow_quaternion_step_degrees=0.,
        max_shoulder_quaternion_step_degrees=0., source_release_samples=0,
        source_radial_range_m=[float("inf"), -float("inf")], source_handle_z_range_m=[float("inf"), -float("inf")],
        play_window_source_frames=PLAY_WINDOWS[action.name], interpolation_reference="Actual engine v2 sampled local poses" if args.runtime_poses else "Blender action interpolation",
        steps=dict(), frames=[], runtime_rotations={name: [] for name in EDITED})
    try:
        for frame in frames:
            source_frame = min(end, start + (frame - 1.))
            imported.animation_data.action = source_action
            capture._set_frame(source_frame)
            source_weapon = grip.world(imported, "weapon_r")
            source_local_palm = source_weapon.inverted() @ grip.palm(imported)
            release, radial, handle_z = grip.release_weight(source_local_palm, source_weapon)
            row["source_radial_range_m"] = [min(row["source_radial_range_m"][0], radial), max(row["source_radial_range_m"][1], radial)]
            row["source_handle_z_range_m"] = [min(row["source_handle_z_range_m"][0], handle_z), max(row["source_handle_z_range_m"][1], handle_z)]
            if release < .9999:
                row["source_release_samples"] += 1
            rig.animation_data.action = action
            capture._set_frame(frame)
            if args.runtime_poses:
                reference = args.runtime_poses["clips"][action.name][round((frame - 1.) * args.runtime_poses["hz"] / 30.)]
                runtime_basis = {}
                for bone in rig.data.bones:
                    value = reference[args.runtime_node_indices[bone.name]]
                    local = Matrix.LocRotScale(Vector(value[:3]), Quaternion((value[6], *value[3:6])), Vector(value[7:]))
                    relative_rest = bone.parent.matrix_local.inverted() @ bone.matrix_local if bone.parent else bone.matrix_local
                    runtime_basis[bone.name] = relative_rest.inverted() @ local
                apply(rig, runtime_basis)
            before_basis = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
            before_world = {bone.name: grip.world(rig, bone.name) for bone in rig.pose.bones}
            target_weapon = before_world["valwood_weapon_r"]
            desired_palm = target_weapon @ source_local_palm
            palm_offset = grip.palm(rig) - before_world["hand_l"].translation
            wrist_goal = desired_palm - palm_offset
            row["max_palm_error_before_m"] = max(row["max_palm_error_before_m"], (grip.palm(rig) - desired_palm).length)
            apply(rig, before_basis)
            fit = shoulder_fit(rig, wrist_goal, args.max_clavicle_degrees, args.max_reach_fraction)
            baseline = {bone.name: grip.world(rig, bone.name) for bone in rig.pose.bones}
            # Restore the captured wrist's world orientation after moving the
            # shoulder; finger channels and palm-local source sliding stay real.
            hand = baseline["hand_l"]
            baseline["hand_l"] = Matrix.LocRotScale(hand.translation,
                before_world["hand_l"].to_quaternion(), hand.to_scale())
            solved = grip.solve(rig, wrist_goal, baseline)
            if not solved["applied"]:
                raise ValueError("Unstretched arm fit failed: " + str(solved))
            row["max_clavicle_degrees"] = max(row["max_clavicle_degrees"], fit["delta_degrees"])
            row["max_clavicle_displacement_m"] = max(row["max_clavicle_displacement_m"], fit["shoulder_displacement_m"])
            row["max_solved_reach_fraction"] = max(row["max_solved_reach_fraction"], fit["solved_reach_fraction"])
            row["max_requested_reach_fraction"] = max(row["max_requested_reach_fraction"], fit["requested_reach_fraction"])
            row["max_arm_segment_error_m"] = max(row["max_arm_segment_error_m"], solved["segment_length_error_m"])
            new_clavicle = grip.world(rig, "clavicle_l").translation
            new_shoulder = grip.world(rig, "upperarm_l").translation
            row["max_clavicle_segment_error_m"] = max(row["max_clavicle_segment_error_m"], abs((new_shoulder - new_clavicle).length - fit["clavicle_length_m"]))
            hand_rotation_error = angle(grip.world(rig, "hand_l").to_quaternion(), before_world["hand_l"].to_quaternion())
            row["max_preserved_hand_rotation_error_degrees"] = max(row["max_preserved_hand_rotation_error_degrees"], hand_rotation_error)
            palm_error = (grip.palm(rig) - desired_palm).length
            row["max_palm_error_after_m"] = max(row["max_palm_error_after_m"], palm_error)
            for name, before in before_world.items():
                bone = rig.pose.bones[name]
                if name == "clavicle_l" or any(parent.name == "clavicle_l" for parent in bone.parent_recursive):
                    continue
                current = grip.world(rig, name)
                row["max_protected_world_error"] = max(row["max_protected_world_error"],
                    *(abs(before[i][j] - current[i][j]) for i in range(4) for j in range(4)))
            values = {name: rig.pose.bones[name].rotation_quaternion.copy() for name in EDITED}
            # Internal bones use their ordinary parent-local pose rotation in
            # glTF. This is independently checked against the accepted v2
            # channels before a separate tool appends only these four tracks.
            for name in EDITED:
                bone = rig.pose.bones[name]
                q = (bone.parent.matrix.inverted() @ bone.matrix).to_quaternion().normalized()
                row["runtime_rotations"][name].append([float(q.x), float(q.y), float(q.z), float(q.w)])
            step_row = {}
            for name in EDITED:
                baseline_q = before_basis[name].to_quaternion()
                new_q = values[name]
                if name in previous:
                    old_step = angle(previous_baseline[name], baseline_q)
                    new_step = angle(previous[name], new_q)
                    step_row[name] = dict(baseline_degrees=old_step, refined_degrees=new_step)
                    for suffix, value in ((f"baseline_{args.sample_hz}hz", old_step), (f"refined_{args.sample_hz}hz", new_step)):
                        key = name + "/" + suffix
                        if key not in row["steps"] or value > row["steps"][key]["degrees"]:
                            row["steps"][key] = dict(degrees=value, source_frame=source_frame)
                        if PLAY_WINDOWS[action.name][0] <= source_frame <= PLAY_WINDOWS[action.name][1]:
                            key += "/played_window"
                            if key not in row["steps"] or value > row["steps"][key]["degrees"]:
                                row["steps"][key] = dict(degrees=value, source_frame=source_frame)
                if abs(frame - round(frame)) < .00001:
                    if name in previous_30:
                        for suffix, value in (("baseline_30hz", angle(previous_baseline_30[name], baseline_q)),
                                              ("refined_30hz", angle(previous_30[name], new_q))):
                            key = name + "/" + suffix
                            if key not in row["steps"] or value > row["steps"][key]["degrees"]:
                                row["steps"][key] = dict(degrees=value, source_frame=source_frame)
                    previous_30[name], previous_baseline_30[name] = new_q.copy(), baseline_q.copy()
                if abs(frame * 2. - round(frame * 2.)) < .00001:
                    if name in previous_60:
                        for suffix, value in (("baseline_60hz", angle(previous_baseline_60[name], baseline_q)),
                                              ("refined_60hz", angle(previous_60[name], new_q))):
                            key = name + "/" + suffix
                            if key not in row["steps"] or value > row["steps"][key]["degrees"]:
                                row["steps"][key] = dict(degrees=value, source_frame=source_frame)
                            if PLAY_WINDOWS[action.name][0] <= source_frame <= PLAY_WINDOWS[action.name][1]:
                                key += "/played_window"
                                if key not in row["steps"] or value > row["steps"][key]["degrees"]:
                                    row["steps"][key] = dict(degrees=value, source_frame=source_frame)
                    previous_60[name], previous_baseline_60[name] = new_q.copy(), baseline_q.copy()
                previous_baseline[name] = baseline_q.copy()
            rig.animation_data.action = derivative
            for name, q in values.items():
                bone = rig.pose.bones[name]
                if name in previous:
                    if previous[name].dot(q) < 0:
                        q.negate()
                    step = angle(previous[name], q)
                    stat = {"clavicle_l": "max_clavicle_quaternion_step_degrees", "lowerarm_l": "max_elbow_quaternion_step_degrees", "upperarm_l": "max_shoulder_quaternion_step_degrees"}.get(name)
                    if stat:
                        row[stat] = max(row[stat], step)
                previous[name] = q.copy()
                bone.rotation_quaternion = q
                bone.keyframe_insert("rotation_quaternion", frame=frame, group=name)
            row["frames"].append(dict(output_frame=frame, source_frame=source_frame,
                clavicle_degrees=fit["delta_degrees"], wrist_error_m=solved["wrist_error_m"], palm_error_m=palm_error,
                source_contact_envelope_weight=release, source_radial_m=radial, source_handle_z_m=handle_z))
        for curve in derivative.fcurves:
            if any(curve.data_path == f'pose.bones["{name}"].rotation_quaternion' for name in EDITED):
                for key in curve.keyframe_points:
                    key.interpolation = "LINEAR"
        if digest(derivative, EDITED) != protected or row["max_protected_world_error"] > .00001:
            raise ValueError("Attack protected channels or world transforms changed")
        if max(row["max_palm_error_after_m"], row["max_arm_segment_error_m"], row["max_clavicle_segment_error_m"]) > .00001:
            raise ValueError("Attack palm or segment-length preservation check failed")
        metadata["offhand_v3"] = {key: value for key, value in row.items() if key not in ("frames", "runtime_rotations")}
        metadata["offhand_v3"]["source_intent"] = "Exact captured weapon-relative palm, including source slide/release; never welded to a guessed handle point"
        derivative["valwood_clip"] = json.dumps(metadata, sort_keys=True)
        return derivative, row
    finally:
        capture._remove_source(objects, source_actions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=PACK / "derived/polish-v2/knight_rig_polish_v2.blend")
    parser.add_argument("--output-dir", type=Path, default=PACK / "derived/polish-v3")
    parser.add_argument("--max-clavicle-degrees", type=float, default=20.)
    parser.add_argument("--max-reach-fraction", type=float, default=.99)
    parser.add_argument("--sample-hz", type=int, default=120, choices=(60, 120))
    parser.add_argument("--engine-reference", type=Path, default=PACK / "derived/polish-v3/runtime_v2_pose_reference_120hz.json")
    parser.add_argument("--audit-only", action="store_true")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.runtime_poses = json.loads(args.engine_reference.read_text()) if args.engine_reference else None
    if args.runtime_poses:
        args.runtime_node_indices = {name: index for index, name in enumerate(args.runtime_poses["nodes"])}
    input_hash = hashlib.sha256(args.input.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(args.input))
    rig = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
    original = {action.name: digest(action) for action in bpy.data.actions if "valwood_clip" in action}
    if len(original) != 25 or len(rig.data.bones) != 95:
        raise ValueError("Expected accepted twenty-five-clip ninety-five-bone v2 rig")
    rest = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
    report = dict(schema="valwood.modular-knight-attack-offhand-v3", input=str(args.input), input_sha256=input_hash,
        source_files_modified=False, source_palm_intent="Actual animated source weapon-local palm, not a static weld",
        max_clavicle_degrees=args.max_clavicle_degrees, max_reach_fraction=args.max_reach_fraction,
        edited_rotation_bones=list(EDITED), engine_interpolation_reference=str(args.engine_reference),
        engine_interpolation_reference_sha256=hashlib.sha256(args.engine_reference.read_bytes()).hexdigest(), clips=[])
    replacements = []
    for name in ("Attack_Left", "Attack_Right", "Attack_High", "Attack_Low", "Attack_Thrust"):
        action, row = refine(rig, bpy.data.actions[name], args)
        replacements.append((name, action))
        report["clips"].append(row)
        print("ATTACK_OFFHAND_CLIP " + json.dumps({key: value for key, value in row.items() if key not in ("frames", "runtime_rotations", "steps")}), flush=True)
    for name, digest_before in original.items():
        if digest(bpy.data.actions[name]) != digest_before:
            raise ValueError("Original in-memory action changed: " + name)
    report["original_nonattack_actions_preserved"] = True
    report["original_attack_protected_channels_preserved"] = True
    report["rest_matrix_error_max"] = max(abs(rest[name][i][j] - rig.data.bones[name].matrix_local[i][j]) for name in rest for i in range(4) for j in range(4))
    if report["rest_matrix_error_max"] > .0000001:
        raise ValueError("Original rest bone changed")
    rig.animation_data.action = None
    for name, replacement in replacements:
        bpy.data.actions.remove(bpy.data.actions[name], do_unlink=True)
        replacement.name = name
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.scene.frame_set(1)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.render.fps_base = 1
    if not args.audit_only:
        output = args.output_dir / "knight_rig_polish_v3.blend"
        bpy.ops.wm.save_as_mainfile(filepath=str(output))
        report["output"] = str(output)
    (args.output_dir / "attack_offhand_report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print("ATTACK_OFFHAND_DONE " + str(args.output_dir), flush=True)


if __name__ == "__main__":
    main()
