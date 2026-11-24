"""Export independent owned-source palm references and verify bone convention.

Blender --background --python-exit-code 1 --python this_file -- [options]
The actual engine checker consumes the 120Hz source palm references after GLB
export, including the midpoints between every new 60Hz left-arm key.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
import numpy as np

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import retarget_modular_knight as capture
import refine_modular_knight_grip as grip
import finalize_modular_knight_glb as glb

PACK = TOOLS.parent / "assets/purchased/modular_knights"
EDITED = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l")
WINDOWS = {"Attack_Left": (40., 73.), "Attack_Right": (38., 69.), "Attack_High": (31., 64.),
           "Attack_Low": (13., 40.), "Attack_Thrust": (17., 51.)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=PACK / "derived/polish-v3")
    parser.add_argument("--hz", type=int, default=240, choices=(120, 240))
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    bpy.ops.wm.open_mainfile(filepath=str(PACK / "derived/polish-v2/knight_rig_polish_v2.blend"))
    rig = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
    doc, binary, original_bytes = glb.load(PACK / "derived/polish-v2/modular_knight_hero_v2.glb")
    rows = []
    reports = []
    max_convention_error = 0.
    for name, window in WINDOWS.items():
        action = bpy.data.actions[name]
        metadata = json.loads(action["valwood_clip"])
        animation = next(a for a in doc["animations"] if a["name"] == name)
        channels = {}
        for node in EDITED:
            channel = next(c for c in animation["channels"] if doc["nodes"][c["target"]["node"]]["name"] == node and c["target"]["path"] == "rotation")
            sampler = animation["samplers"][channel["sampler"]]
            channels[node] = (glb.array(doc, binary, sampler["input"]).reshape(-1), glb.array(doc, binary, sampler["output"]))
        rig.animation_data.action = action
        for frame in range(1, round(metadata["duration_seconds"] * 30) + 2):
            capture._set_frame(frame)
            seconds = (frame - 1.) / 30.
            for node, (times, values) in channels.items():
                bone = rig.pose.bones[node]
                q = (bone.parent.matrix.inverted() @ bone.matrix).to_quaternion().normalized()
                actual = np.asarray((q.x, q.y, q.z, q.w))
                index = int(np.argmin(np.abs(times - seconds)))
                if abs(times[index] - seconds) > .00001:
                    raise ValueError("Expected an authored original frame for the rotation convention check")
                expected = values[index]
                if float(np.dot(actual, expected)) < 0:
                    actual *= -1
                max_convention_error = max(max_convention_error, float(np.max(np.abs(actual - expected))))
        source, objects, source_actions = capture._import_source(Path(metadata["source"]))
        source_action = source.animation_data.action
        start, end = metadata["source_frame_range"]
        count = round(metadata["duration_seconds"] * args.hz)
        try:
            for index in range(count + 1):
                seconds = index / args.hz
                frame = min(end, start + seconds * 30.)
                source.animation_data.action = source_action
                capture._set_frame(frame)
                source_weapon = grip.world(source, "weapon_r")
                local = source_weapon.inverted() @ grip.palm(source)
                played = int(window[0] <= frame <= window[1])
                rows.append(f"{name} {seconds:.10f} {local.x:.10f} {local.y:.10f} {local.z:.10f} {played}\n")
        finally:
            capture._remove_source(objects, source_actions)
        reports.append(dict(name=name, source=metadata["source"], source_sha256=metadata["source_sha256"], samples=count + 1, hz=args.hz))
    if max_convention_error > .000002:
        raise ValueError("Parent-local quaternion convention did not match original GLB")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    path = args.output_dir / f"source_palm_reference_{args.hz}hz.txt"
    path.write_text("".join(rows))
    report = dict(schema="valwood.modular-knight-independent-grip-reference.v3", passed=True,
        original_glb_sha256=hashlib.sha256(original_bytes).hexdigest(), source_files_modified=False,
        original_local_joint_rotation_convention_max_component_error=max_convention_error,
        clip_references=reports, source_reference_file=str(path), source_reference_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (args.output_dir / "source_reference_report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
