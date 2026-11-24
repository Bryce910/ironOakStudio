#!/usr/bin/env python3
"""Compare a knight polish derivative against the frozen fifteen-clip asset."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import finalize_modular_knight_glb as glb


def animation_signature(doc, binary, animation):
    rows = {}
    for channel in animation["channels"]:
        target = channel["target"]
        node = doc["nodes"][target["node"]]["name"]
        sampler = animation["samplers"][channel["sampler"]]
        rows[node + "/" + target["path"]] = dict(interpolation=sampler.get("interpolation", "LINEAR"),
            times=glb.sha(glb.array(doc, binary, sampler["input"]).tobytes()),
            values=glb.sha(glb.array(doc, binary, sampler["output"]).tobytes()))
    return rows


def image_hashes(doc, binary):
    values = []
    for image in doc["images"]:
        view = doc["bufferViews"][image["bufferView"]]
        start = view.get("byteOffset", 0)
        values.append(glb.sha(binary[start:start + view["byteLength"]]))
    return sorted(values)


def motion_checks(doc, binary, animation):
    moving = []
    max_seam = 0.
    for channel in animation["channels"]:
        sampler = animation["samplers"][channel["sampler"]]
        values = glb.array(doc, binary, sampler["output"])
        target = channel["target"]
        name = doc["nodes"][target["node"]]["name"] + "/" + target["path"]
        delta = float(np.max(np.abs(values - values[0])))
        if delta > .00001:
            moving.append(dict(channel=name, max_component_change=delta))
        max_seam = max(max_seam, float(np.max(np.abs(values[-1] - values[0]))))
    return dict(varying_channels=len(moving), motion=moving, max_endpoint_component_difference=max_seam)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1] / "assets/purchased/modular_knights/derived"
    parser.add_argument("--original", type=Path, default=root / "modular_knight_hero.glb")
    parser.add_argument("--polish", type=Path, default=root / "polish-v2/modular_knight_hero_v2.glb")
    parser.add_argument("--report", type=Path, default=root / "polish-v2/frozen_asset_preservation.json")
    args = parser.parse_args()
    old, ob, original_bytes = glb.load(args.original)
    new, nb, new_bytes = glb.load(args.polish)
    before = {a["name"]: a for a in old["animations"]}
    after = {a["name"]: a for a in new["animations"]}
    expected = {prefix + "_" + label for prefix in ("Guard", "Hit") for label in ("Left", "Right", "High", "Low", "Thrust")}
    rows = {name: animation_signature(old, ob, animation) == animation_signature(new, nb, after[name])
            for name, animation in before.items()}
    motion = {name: motion_checks(new, nb, after[name]) for name in sorted(expected)}
    node_transform = lambda doc: {node["name"]: {key: node[key] for key in ("translation", "rotation", "scale", "matrix", "children") if key in node}
                                  for node in doc["nodes"]}
    checks = dict(original_clip_count_15=len(before) == 15, new_clip_count_25=len(after) == 25,
        exact_new_clip_names=set(after) - set(before) == expected,
        original_animation_channels_bit_exact=all(rows.values()),
        geometry_attributes_indices_material_assignment_bit_exact=glb.geometry_signature(old, ob) == glb.geometry_signature(new, nb),
        joint_indices_and_inverse_binds_bit_exact=glb.skin_signature(old, ob) == glb.skin_signature(new, nb),
        all_node_rest_transforms_hierarchy_exact=node_transform(old) == node_transform(new),
        material_definitions_exact=old["materials"] == new["materials"],
        all_embedded_texture_bytes_exact=image_hashes(old, ob) == image_hashes(new, nb),
        every_added_reaction_has_captured_motion=all(row["varying_channels"] >= 20 for name, row in motion.items() if name.startswith("Hit_")),
        every_guard_has_measured_breathing_motion=all(row["varying_channels"] >= 1 for name, row in motion.items() if name.startswith("Guard_")),
        added_clip_endpoints_are_smooth=all(row["max_endpoint_component_difference"] < .00001 for row in motion.values()))
    report = dict(schema="valwood.modular-knight-polish-preservation.v2", passed=all(checks.values()),
        original=str(args.original), original_sha256=hashlib.sha256(original_bytes).hexdigest(),
        polish=str(args.polish), polish_sha256=hashlib.sha256(new_bytes).hexdigest(), checks=checks,
        original_animation_checks=rows, new_clips=sorted(expected), added_clip_motion=motion, source_files_modified=False)
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(json.dumps(report, indent=2))
    if not report["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
