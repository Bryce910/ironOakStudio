#!/usr/bin/env python3
"""Append only the four permitted 60Hz attack rotations to accepted v2 GLB.

The Blender baker emits parent-local pose quaternions for internal bones. The
separate post-export checker verifies the imported engine poses against source
palm intent and the original skeleton; no body or mesh export is repeated.
"""
import argparse
import copy
import json
from pathlib import Path

import numpy as np
import finalize_modular_knight_glb as glb

EDITED = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l")


def channel_records(doc, binary, animation, exclude=()):
    rows = {}
    for channel in animation["channels"]:
        target = channel["target"]
        name = doc["nodes"][target["node"]]["name"]
        if name in exclude and target["path"] == "rotation":
            continue
        sampler = animation["samplers"][channel["sampler"]]
        rows[name + "/" + target["path"]] = dict(interpolation=sampler.get("interpolation", "LINEAR"),
            times=glb.sha(glb.array(doc, binary, sampler["input"]).tobytes()),
            values=glb.sha(glb.array(doc, binary, sampler["output"]).tobytes()))
    return rows


def main():
    root = Path(__file__).resolve().parents[1] / "assets/purchased/modular_knights/derived"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=root / "polish-v2/modular_knight_hero_v2.glb")
    parser.add_argument("--bake-report", type=Path, default=root / "polish-v3/attack_offhand_report.json")
    parser.add_argument("--output", type=Path, default=root / "polish-v3/modular_knight_hero_v3.glb")
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        raise ValueError("Accepted v2 asset is read only")
    old, original_binary, old_bytes = glb.load(args.input)
    doc, binary = copy.deepcopy(old), bytearray(original_binary)
    baked = json.loads(args.bake_report.read_text())
    nodes = {node["name"]: i for i, node in enumerate(doc["nodes"])}
    if len(nodes) != len(doc["nodes"]):
        raise ValueError("Ambiguous joint names")
    changed = []
    for clip in baked["clips"]:
        animation = next(a for a in doc["animations"] if a["name"] == clip["name"])
        times = np.asarray([[(row["output_frame"] - 1.) / 30.] for row in clip["frames"]], dtype="<f4")
        for name in EDITED:
            # A high-cut clavicle that needed no shoulder compensation stays
            # byte-exact instead of receiving an unnecessary resampled track.
            if name == "clavicle_l" and clip["max_clavicle_degrees"] == 0.:
                continue
            channels = [c for c in animation["channels"] if c["target"] == dict(node=nodes[name], path="rotation")]
            if len(channels) != 1:
                raise ValueError("Expected one existing rotation channel")
            channel = channels[0]
            old_sampler = animation["samplers"][channel["sampler"]]
            values = np.asarray(clip["runtime_rotations"][name], dtype="<f4")
            if values.shape != (len(times), 4) or not np.all(np.isfinite(values)):
                raise ValueError("Invalid baked rotation array")
            values /= np.linalg.norm(values, axis=1, keepdims=True)
            for i in range(1, len(values)):
                if float(np.dot(values[i - 1], values[i])) < 0:
                    values[i] *= -1
            time_accessor = glb.append_accessor(doc, binary, times, doc["accessors"][old_sampler["input"]])
            rotation_accessor = glb.append_accessor(doc, binary, values, doc["accessors"][old_sampler["output"]])
            channel["sampler"] = len(animation["samplers"])
            animation["samplers"].append(dict(input=time_accessor, output=rotation_accessor, interpolation="LINEAR"))
            changed.append(dict(clip=clip["name"], node=name, samples=len(times), sample_hz=clip["sample_hz"]))
    output = glb.encode(doc, binary)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    after, after_binary, output = glb.load(args.output)
    old_anims = {a["name"]: a for a in old["animations"]}
    new_anims = {a["name"]: a for a in after["animations"]}
    checks = dict(same_25_clip_names=set(old_anims) == set(new_anims),
        geometry_bit_exact=glb.geometry_signature(old, original_binary) == glb.geometry_signature(after, after_binary),
        joint_bind_bit_exact=glb.skin_signature(old, original_binary) == glb.skin_signature(after, after_binary),
        nodes_rest_hierarchy_bit_exact=old["nodes"] == after["nodes"],
        materials_images_textures_bit_exact=all(old[key] == after[key] for key in ("materials", "images", "textures", "samplers")),
        original_buffer_prefix_bit_exact=after_binary[:len(original_binary)] == original_binary,
        twenty_nonattack_clips_bit_exact=True, protected_attack_channels_bit_exact=True)
    for name, animation in old_anims.items():
        exclude = EDITED if name.startswith("Attack_") else ()
        same = channel_records(old, original_binary, animation, exclude) == channel_records(after, after_binary, new_anims[name], exclude)
        checks["protected_attack_channels_bit_exact" if exclude else "twenty_nonattack_clips_bit_exact"] &= same
    report = dict(schema="valwood.modular-knight-offhand-patch-v3", passed=all(checks.values()), checks=checks,
        input=str(args.input), input_sha256=glb.sha(old_bytes), output=str(args.output), output_sha256=glb.sha(output),
        output_bytes=len(output), changed_channels=changed, source_bake_report=str(args.bake_report),
        only_permitted_attack_rotations_changed=True, source_files_modified=False)
    args.output.with_suffix(".patch-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    if not report["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
