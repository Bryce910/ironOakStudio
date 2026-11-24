#!/usr/bin/env python3
"""Re-seat private battlefield blood against the production Studio terrain bake.

Does not run the original scene generator, change record ordinals, or modify any
shared asset. Requires battlefield_world_sample built from this candidate.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import subprocess

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "world-project"
ORIGIN = (-12085.0, 5150.0)
SAMPLER = Path("/tmp/battlefield_world_sample")


def sample(points):
    result = subprocess.run(
        [str(SAMPLER)], cwd=PROJECT,
        input="".join(f"{x:.12f} {z:.12f}\n" for x, z in points),
        text=True, check=True, capture_output=True,
    )
    rows = [[float(v) for v in line.split()]
            for line in result.stdout.splitlines()]
    assert len(rows) == len(points), result.stderr
    return rows


def main():
    model = PROJECT / "models/battlefield/casualty_blood.glb"
    assert model.is_file() and not model.is_symlink()
    blob = model.read_bytes()
    json_length = struct.unpack_from("<I", blob, 12)[0]
    document = json.loads(blob[20:20 + json_length])
    binary = bytearray(blob[28 + json_length:])
    primitives = []
    points = []
    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            assert "indices" not in primitive and primitive.get("mode", 4) == 4
            attrs = {}
            for key in ("POSITION", "NORMAL"):
                accessor = document["accessors"][primitive["attributes"][key]]
                view = document["bufferViews"][accessor["bufferView"]]
                assert accessor["componentType"] == 5126 and accessor["type"] == "VEC3"
                assert "byteStride" not in view
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                attrs[key] = (accessor, np.frombuffer(
                    binary, dtype="<f4", count=accessor["count"] * 3,
                    offset=offset).reshape(-1, 3))
            positions = attrs["POSITION"][1]
            assert len(positions) % 3 == 0
            points.extend((ORIGIN[0] + float(p[0]), ORIGIN[1] + float(p[2]))
                          for p in positions)
            primitives.append(attrs)
    samples = sample(points)
    previous_offsets = []
    corrected_offsets = []
    cursor = 0
    minimum = math.inf
    for attrs in primitives:
        accessor, positions = attrs["POSITION"]
        normals = attrs["NORMAL"][1]
        for p in positions:
            terrain = samples[cursor]
            assert not terrain[3], f"Blood pool is in local water: {terrain}"
            previous_offsets.append(float(p[1]) - terrain[2])
            p[1] = terrain[2] + .020
            corrected_offsets.append(float(p[1]) - terrain[2])
            minimum = min(minimum, float(p[1]))
            cursor += 1
        for first in range(0, len(positions), 3):
            triangle = positions[first:first + 3].astype(np.float64)
            normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
            length = np.linalg.norm(normal)
            assert length > 1e-12
            normal /= length
            assert normal[1] > 0, "Reversed blood pool triangle"
            normals[first:first + 3] = normal
        accessor["min"] = positions.min(0).tolist()
        accessor["max"] = positions.max(0).tolist()
    assert .010 < min(corrected_offsets) <= max(corrected_offsets) < .025
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    binary += b"\0" * (-len(binary) % 4)
    model.write_bytes(
        struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary))
        + struct.pack("<II", len(encoded), 0x4E4F534A) + encoded
        + struct.pack("<II", len(binary), 0x004E4942) + binary)
    records = PROJECT / "scenery/battlefield_instances.txt"
    assert not records.is_symlink()
    lines = records.read_text().splitlines()
    changed = 0
    for index, line in enumerate(lines):
        row = line.split()
        if row and row[0] == "battlefield_casualty_blood":
            assert len(row) == 14
            row[13] = f"{minimum:.9f}"
            lines[index] = " ".join(row)
            changed += 1
    assert changed == 1
    records.write_text("\n".join(lines) + "\n")
    report = {
        "method": "Production Studio saved sculpt + ribbon terrain/water bake; source GLB Y stores absolute ground height; explicit placement pivot restores the normalized model's minimum Y.",
        "vertices": len(points), "wet_vertices": 0,
        "previous_offset_range_m": [min(previous_offsets), max(previous_offsets)],
        "corrected_offset_range_m": [min(corrected_offsets), max(corrected_offsets)],
        "corrected_vertices": sum(abs(a-b) > 1e-6 for a,b in zip(previous_offsets, corrected_offsets)),
        "explicit_pivot_y": minimum,
        "sha256": hashlib.sha256(model.read_bytes()).hexdigest(),
        "bytes": model.stat().st_size,
        "source_record_count": sum(bool(l.strip()) and not l.lstrip().startswith("#") for l in lines),
        "record_ordinals_preserved": True,
    }
    target = ROOT / "review/battlefield_static_ground_audit.json"
    target.write_text(json.dumps(report, indent=2) + "\n")
    original_report = ROOT / "assets/battlefield/SCENE_BUILD.json"
    build = json.loads(original_report.read_text())
    build["static_pool_min"] = minimum
    build["static_ground_audit"] = str(target)
    for asset in build["mesh_assets"]:
        if Path(asset["path"]).name == model.name:
            asset.update(bytes=report["bytes"], sha256=report["sha256"])
    original_report.write_text(json.dumps(build, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
