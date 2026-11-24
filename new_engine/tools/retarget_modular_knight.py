"""Bake owned MoCap Central clips onto the purchased Modular Knights rig.

Run inside Blender. Source FBX files, target rest bones and meshes are never
saved or edited. bake_actions returns (actions_by_name, JSON-safe_report).
The caller owns geometry/material selection, GLB export and in-game acceptance.
"""
from pathlib import Path
import hashlib
import json
import math

import bpy
from mathutils import Matrix, Quaternion, Vector

ENGINE = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ENGINE / "assets/purchased/longsword_mocap/source/Animations/Unity_Skel"

# Explicitly audited target -> owned source semantics. The target floor 'root'
# has no source peer; it stays at rest. The source anatomical root is pelvis.
TARGET_TO_SOURCE = {
    name: name for name in (
        "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head", "jaw",
        *(
            base + "_" + side
            for side in ("l", "r")
            for base in (
                "clavicle", "upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball",
                "upperarm_twist_01", "lowerarm_twist_01", "thigh_twist_01", "calf_twist_01",
                *(digit + "_%02d" % number for digit in ("thumb", "index", "middle", "ring", "pinky")
                  for number in (1, 2, 3)),
            )
        ),
    )
}

SEMANTIC_CHILD = {
    "pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "spine_03",
    "spine_03": "neck_01", "neck_01": "head",
}
for _side in ("l", "r"):
    for _a, _b in (("clavicle", "upperarm"), ("upperarm", "lowerarm"),
                   ("lowerarm", "hand"), ("hand", "middle_01"),
                   ("thigh", "calf"), ("calf", "foot"), ("foot", "ball")):
        SEMANTIC_CHILD[_a + "_" + _side] = _b + "_" + _side
    for _digit in ("thumb", "index", "middle", "ring", "pinky"):
        for _number in (1, 2):
            SEMANTIC_CHILD[f"{_digit}_{_number:02}_{_side}"] = f"{_digit}_{_number+1:02}_{_side}"

# Frames are source Blender frames at30fps. Final duplicate held frames are
# excluded. Phase markers are measured left-support/right-toe-swing-apex cues.
WALKS = {
    "Fwd": (35, 32), "Bwd": (32, 6), "Left": (33, 4), "Right": (33, 31),
    "FwdLeft": (32, 3), "BwdLeft": (33, 4), "BwdRight": (31, 1), "FwdRight": (30, 28),
}
CLIPS = {
    "Idle": dict(file="Idle/MCU_am_LongS_Lv1_StanceR_Idle_01.FBX", loop=True),
    **{
        "Walk_" + direction: dict(
            file=f"Walk/MCU_am_LongS_Lv1_StanceR_Walk_{direction}_NoRM.FBX",
            start=1.0, end=1.0 + period, phase_frame=float(phase), loop=True, gait_cycles=1,
        ) for direction, (period, phase) in WALKS.items()
    },
    # Keep both measured captured cycles. Cutting this run to21frames would
    # require7.18cm/9.58degree seam corrections instead of0.0075cm/0.0123degree.
    "Run": dict(file="Run/MCU_am_LongS_Run_Loop_NoRM.FBX", start=1.0, end=43.0,
                phase_frame=19.0, loop=True, gait_cycles=2),
    "Attack_Left": dict(file="Attack/MCU_am_LongS_Lv1_StanceR_Attack01.FBX", loop=False, contact_frame=53),
    "Attack_Right": dict(file="Attack/MCU_am_LongS_Lv1_StanceR_Attack03.FBX", loop=False, contact_frame=50),
    "Attack_High": dict(file="Attack/MCU_am_LongS_StanceRBlock_Attack03.FBX", loop=False, contact_frame=44),
    "Attack_Low": dict(file="Attack/MCU_am_LongS_Lv1_StanceR_Attack11.FBX", loop=False, contact_frame=21),
    "Attack_Thrust": dict(file="Attack/MCU_am_LongS_Lv1_StanceR_Attack13.FBX", loop=False, contact_frame=32),
}


def _set_frame(frame):
    bpy.context.scene.frame_set(math.floor(frame), subframe=frame % 1)
    bpy.context.view_layer.update()


def _body_basis(rest, names):
    up = (rest[names["head"]].translation - rest[names["pelvis"]].translation).normalized()
    left = rest[names["upperarm_l"]].translation - rest[names["upperarm_r"]].translation
    left = (left - up * left.dot(up)).normalized()
    if up.length < .9 or left.length < .9:
        raise ValueError("Cannot establish anatomical body basis from audited joints")
    # Left/up/forward basis: both audited source and target face Blender−Y.
    return Matrix((left, up, left.cross(up))).transposed()


def _leg_length(rest, names):
    lengths = []
    for side in ("l", "r"):
        hip, knee, ankle = [rest[names[n + "_" + side]].translation for n in ("thigh", "calf", "foot")]
        lengths.append((hip - knee).length + (knee - ankle).length)
    result = sum(lengths) / len(lengths)
    if not math.isfinite(result) or result <= 0:
        raise ValueError("Invalid mapped rest leg length")
    return result


def _rest(armature):
    return {bone.name: armature.matrix_world @ bone.matrix_local for bone in armature.data.bones}


def _basis_pose(armature):
    return {b.name: b.matrix_basis.copy().decompose() for b in armature.pose.bones}


def _same_pose(a, b):
    # Exact duplicate final source poses were verified at half-frame resolution.
    # Here decompose can introduce tiny quaternion roundoff; compare matrices.
    for name in a:
        first, second = Matrix.LocRotScale(*a[name]), Matrix.LocRotScale(*b[name])
        if any(abs(first[i][j] - second[i][j]) > 1.e-7 for i in range(4) for j in range(4)):
            return False
    return True


def _sample_source(source, frame):
    _set_frame(frame)
    return _basis_pose(source)


def _source_pose(source, frame, start, end, correction, window):
    pose = _sample_source(source, frame)
    if correction and window > 0 and frame > end - window:
        amount = min(1., max(0., (frame - end + window) / window))
        weight = amount * amount * (3 - 2 * amount)
        for name, (position, rotation, scale) in pose.items():
            dp, dq, ds = correction[name]
            pose[name] = (position + dp * weight,
                          rotation @ Quaternion((1, 0, 0, 0)).slerp(dq, weight),
                          scale + ds * weight)
        for name, transform in pose.items():
            source.pose.bones[name].matrix_basis = Matrix.LocRotScale(*transform)
        bpy.context.view_layer.update()
    return pose


def _import_source(path):
    before_objects = set(bpy.data.objects)
    before_actions = set(bpy.data.actions)
    try:
        bpy.ops.import_scene.fbx(filepath=str(path), use_anim=True, ignore_leaf_bones=False)
        added_objects = set(bpy.data.objects) - before_objects
        rigs = [o for o in added_objects if o.type == "ARMATURE"]
        if len(rigs) != 1 or not rigs[0].animation_data or not rigs[0].animation_data.action:
            raise ValueError("Expected one animated source armature: " + str(path))
        return rigs[0], added_objects, set(bpy.data.actions) - before_actions
    except Exception:
        _remove_source(set(bpy.data.objects) - before_objects, set(bpy.data.actions) - before_actions)
        raise


def _remove_source(objects, actions):
    for obj in objects:
        if obj.name in bpy.data.objects:
            bpy.data.objects.remove(obj, do_unlink=True)
    for action in actions:
        if action.name in bpy.data.actions:
            bpy.data.actions.remove(action, do_unlink=True)


def bake_actions(target, *, source_root=SOURCE_ROOT, clips=None, target_map=None,
                 fps=30, seam_frames=4.0, align_gait_phase=True, progress=None):
    """Create true skeletal actions on an imported Modular Knights armature.

    ``clips`` is a sequence of CLIPS names (default all15). ``target_map`` maps
    actual target bone names to audited source semantics, default identity map
    above. ``fps`` is the Blender/output sample rate; source timing remains30Hz.
    All target rest transforms stay unchanged. No foot/grip IK, per-frame floor
    normalization or source horizontal sway stripping is applied.

    Returns (dict[str,bpy.types.Action], JSON-safe metadata). Target's previous
    active action/pose, NLA mute states, scene time and FPS are restored. New
    actions use fake users, so callers can explicitly choose/export them.
    Mapped bones/helpers use quaternion mode for the new skeletal actions.
    Set scene.render.fps=fps and fps_base=1 before their playback/export.
    """
    if target.type != "ARMATURE":
        raise ValueError("target must be a Blender armature object")
    if not isinstance(fps, int) or not 1 <= fps <= 120:
        raise ValueError("fps must be an integer from1to120")
    if not math.isfinite(seam_frames) or seam_frames < 0 or seam_frames > 8:
        raise ValueError("seam_frames must be finite and in0..8")
    chosen = list(CLIPS) if clips is None else list(clips)
    if not chosen or len(chosen) != len(set(chosen)) or any(name not in CLIPS for name in chosen):
        raise ValueError("clips must contain distinct known CLIPS names")
    mapping = dict(TARGET_TO_SOURCE if target_map is None else target_map)
    required = ("pelvis", "head", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r")
    reverse = {value: key for key, value in mapping.items()}
    if len(reverse) != len(mapping) or any(name not in reverse for name in required):
        raise ValueError("Target mapping must be one-to-one and include anatomical root/body/legs")
    missing = sorted(set(mapping) - set(target.data.bones.keys()))
    if missing:
        raise ValueError("Target mapping references absent bones: " + ", ".join(missing))
    source_root = Path(source_root)
    for name in chosen:
        if not (source_root / CLIPS[name]["file"]).is_file():
            raise FileNotFoundError(source_root / CLIPS[name]["file"])

    scene = bpy.context.scene
    previous_frame, previous_subframe = scene.frame_current, scene.frame_subframe
    previous_fps, previous_base = scene.render.fps, scene.render.fps_base
    target.animation_data_create()
    previous_action = target.animation_data.action
    previous_pose = {b.name: b.matrix_basis.copy() for b in target.pose.bones}
    track_mutes = [(track, track.mute) for track in target.animation_data.nla_tracks]
    target.animation_data.action = None
    for track, _ in track_mutes:
        track.mute = True
    for bone in target.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    target_rest = _rest(target)
    target_frame = _body_basis(target_rest, reverse)
    target_order = sorted(target.pose.bones, key=lambda b: len(b.parent_recursive))
    target_world = target.matrix_world.copy()
    target_inverse = target_world.inverted()
    target_inverse_rotation = target_world.to_quaternion().inverted()
    rest_relative = {
        bone.name: (bone.parent.matrix_local.inverted() @ bone.matrix_local if bone.parent else bone.matrix_local.copy())
        for bone in target.data.bones
    }
    source_identity = {semantic: semantic for semantic in mapping.values()}
    actions, reports = {}, []
    try:
        for label in chosen:
            spec = CLIPS[label]
            path = source_root / spec["file"]
            source, added_objects, source_actions = _import_source(path)
            action = None
            try:
                missing_source = sorted(set(mapping.values()) - set(source.data.bones.keys()))
                if missing_source:
                    raise ValueError("Source lacks mapped bones: " + ", ".join(missing_source))
                source_action = source.animation_data.action
                source_rest = _rest(source)
                conversion = target_frame @ _body_basis(source_rest, source_identity).transposed()
                conversion_q = conversion.to_quaternion()
                scale = _leg_length(target_rest, reverse) / _leg_length(source_rest, source_identity)
                offsets = {}
                for target_name, source_name in mapping.items():
                    original = conversion_q @ source_rest[source_name].to_quaternion()
                    child = SEMANTIC_CHILD.get(source_name)
                    swing = Quaternion((1, 0, 0, 0))
                    if child in reverse and child in source_rest:
                        a = conversion @ (source_rest[child].translation - source_rest[source_name].translation)
                        b = target_rest[reverse[child]].translation - target_rest[target_name].translation
                        if a.length > 1.e-8 and b.length > 1.e-8:
                            swing = a.normalized().rotation_difference(b.normalized())
                    offsets[target_name] = (swing @ original).inverted() @ target_rest[target_name].to_quaternion()
                lo, hi = map(float, source_action.frame_range)
                start, end = float(spec.get("start", lo)), float(spec.get("end", hi))
                trimmed_hold = False
                if "end" not in spec and spec["loop"] and end - start > 1:
                    if _same_pose(_sample_source(source, end - 1), _sample_source(source, end)):
                        end -= 1
                        trimmed_hold = True
                if start < lo or end > hi or end <= start:
                    raise ValueError("Authored clip range outside source action")
                first = _sample_source(source, start)
                last = _sample_source(source, end)
                correction = None
                if spec["loop"] and seam_frames:
                    correction = {
                        name: (first[name][0] - last[name][0], last[name][1].inverted() @ first[name][1], first[name][2] - last[name][2])
                        for name in first
                    }
                period, duration = end - start, (end - start) / 30.
                phase_frame = float(spec.get("phase_frame", start)) if align_gait_phase else start
                phase_offset = phase_frame - start
                if phase_offset < 0 or phase_offset >= period:
                    raise ValueError("Gait phase marker outside source period")
                action = bpy.data.actions.new(label)
                action.use_fake_user = True
                target.animation_data.action = action
                previous_rotations = {}
                samples = max(1, int(math.ceil(duration * fps)))
                for index in range(samples + 1):
                    seconds = min(duration, index / fps)
                    output_frame = 1. + seconds * fps
                    # Do not phase-wrap a nonlooping attack's final pose.
                    source_frame = start + ((seconds * 30. + phase_offset) % period) if spec["loop"] else start + seconds * 30.
                    source.animation_data.action = source_action
                    _source_pose(source, source_frame, start, end, correction, min(seam_frames, period))
                    source_poses = {name: source.matrix_world @ source.pose.bones[name].matrix for name in mapping.values()}
                    computed = {}
                    for bone in target_order:
                        base = (computed[bone.parent.name] @ rest_relative[bone.name] if bone.parent else rest_relative[bone.name].copy())
                        if bone.name not in mapping:
                            computed[bone.name] = base
                            bone.matrix_basis = Matrix.Identity(4)
                            continue
                        semantic = mapping[bone.name]
                        rotation = target_inverse_rotation @ conversion_q @ source_poses[semantic].to_quaternion() @ offsets[bone.name]
                        position = base.translation.copy()
                        if semantic == "pelvis":
                            delta = conversion @ (source_poses[semantic].translation - source_rest[semantic].translation) * scale
                            position = target_inverse @ (target_rest[bone.name].translation + delta)
                        desired = Matrix.LocRotScale(position, rotation.normalized(), base.to_scale())
                        local = base.inverted() @ desired
                        position, rotation, scaling = local.decompose()
                        rotation.normalize()
                        old = previous_rotations.get(bone.name)
                        if old is not None and old.dot(rotation) < 0:
                            rotation.negate()
                        previous_rotations[bone.name] = rotation.copy()
                        bone.rotation_mode = "QUATERNION"
                        bone.location, bone.rotation_quaternion, bone.scale = position, rotation, scaling
                        computed[bone.name] = desired
                        bone.keyframe_insert("rotation_quaternion", frame=output_frame, group=bone.name)
                        if semantic == "pelvis":
                            bone.keyframe_insert("location", frame=output_frame, group=bone.name)
                    if index == 0:
                        # The purchased FBX arrives with a100x pose scale on
                        # root and some IK helpers. Each derivative action must
                        # explicitly override those imported pose properties,
                        # while preserving the source's actual rest/bind data.
                        for bone in target_order:
                            bone.rotation_mode = "QUATERNION"
                            bone.keyframe_insert("scale", frame=output_frame, group=bone.name)
                            if mapping.get(bone.name) != "pelvis":
                                bone.keyframe_insert("location", frame=output_frame, group=bone.name)
                            if bone.name not in mapping:
                                bone.keyframe_insert("rotation_quaternion", frame=output_frame, group=bone.name)
                    if progress and (index == 0 or index == samples or index % fps == 0):
                        progress(label, index, samples)
                for curve in action.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = "LINEAR"
                metadata = dict(name=action.name, semantic=label, source=str(path), source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                source_frame_range=[start, end], source_fps=30, output_fps=fps,
                                duration_seconds=duration, sample_count=samples + 1, loop=spec["loop"],
                                gait_cycles=spec.get("gait_cycles", 0), source_phase_frame=phase_frame,
                                seam_frames=seam_frames if spec["loop"] else 0, trimmed_duplicate_hold=trimmed_hold,
                                leg_translation_scale=scale, source_modified=False, target_rest_modified=False,
                                foot_or_grip_ik_applied=False, pelvis_xyz_preserved=True)
                metadata["horizontal_capture_travel_retained"] = not spec["loop"]
                if "contact_frame" in spec:
                    metadata["contact_source_frame"] = spec["contact_frame"]
                    metadata["contact_seconds"] = (spec["contact_frame"] - start) / 30.
                action["valwood_clip"] = json.dumps(metadata, sort_keys=True)
                actions[label] = action
                reports.append(metadata)
                if progress:
                    progress(label, samples, samples)
            except Exception:
                if action is not None:
                    target.animation_data.action = None
                    bpy.data.actions.remove(action)
                raise
            finally:
                _remove_source(added_objects, source_actions)
    except Exception:
        target.animation_data.action = None
        for action in actions.values():
            bpy.data.actions.remove(action, do_unlink=True)
        raise
    finally:
        target.animation_data.action = previous_action
        for track, muted in track_mutes:
            track.mute = muted
        for bone in target.pose.bones:
            bone.matrix_basis = previous_pose[bone.name]
        scene.render.fps, scene.render.fps_base = previous_fps, previous_base
        scene.frame_set(previous_frame, subframe=previous_subframe)
        bpy.context.view_layer.update()
    return actions, dict(schema="valwood.modular-knight-retarget.v1", target=target.name,
                         target_to_source=mapping, sample_fps=fps, clips=reports,
                         method="World joint rotations with semantic-child bind compensation; proportional pelvis XYZ; dense local quaternion actions",
                         source_files_modified=False, target_rest_modified=False,
                         visual_quality_evaluated=False,
                         limitations=["No in-game motion, grip, foot-contact, clipping, collision or combat acceptance is implied.",
                                      "Run retains two gait cycles; playback must preserve cycle parity when blending."])
