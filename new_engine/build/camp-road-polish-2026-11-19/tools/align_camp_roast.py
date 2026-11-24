#!/usr/bin/env python3
"""Seat the existing roast on its existing spit; private data transform only."""
from pathlib import Path
import argparse
import hashlib
import json
import struct

import numpy as np

N = Path(__file__).resolve().parents[1]
P = N / "world-project"
OUT = N / "review/scenery"
EXPECTED = "fba2e95bd48dd7135daec55ce85d53dfcd5a7683511811eb3ac2d26aa716d22a"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def glb(path):
    raw = path.read_bytes()
    size = struct.unpack_from("<I", raw, 12)[0]
    doc, data = json.loads(raw[20:20 + size]), raw[28 + size:]
    assert all(not any(key in n for key in ("matrix", "translation", "rotation", "scale", "skin")) for n in doc["nodes"])

    def accessor(index):
        a = doc["accessors"][index]
        view = doc["bufferViews"][a["bufferView"]]
        dtype = {5126: "<f4", 5125: "<u4", 5123: "<u2"}[a["componentType"]]
        width = {"SCALAR": 1, "VEC3": 3}[a["type"]]
        size = np.dtype(dtype).itemsize
        return np.ndarray((a["count"], width), dtype=dtype, buffer=data,
            offset=view.get("byteOffset", 0) + a.get("byteOffset", 0),
            strides=(view.get("byteStride", size * width), size)).astype(float)
    return doc, accessor


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply-after-journey", action="store_true")
    args = parser.parse_args()
    assert N.name == "camp-road-polish-2026-11-19" and not (N / "PUBLISHED.json").exists()
    path = P / "scenery/starting_area_instances.txt"
    assert not path.is_symlink() and path.resolve().is_relative_to(N)
    assert sha(path) == EXPECTED, "Reviewed placement source changed; do not stack or overwrite"
    studio = (P / "world.studio").read_text().splitlines()
    assets = {q[1]: q[2] for line in studio if (q := line.split()) and q[0] == "asset"}
    rows = path.read_text().splitlines()
    matches = {alias: [i for i, line in enumerate(rows) if line.startswith(alias + " ")]
               for alias in ("nm_city_region_camp", "nm_city_region_roast")}
    assert all(len(ids) == 1 for ids in matches.values())
    camp = rows[matches["nm_city_region_camp"][0]].split()
    index = matches["nm_city_region_roast"][0]
    roast = rows[index].split()
    assert camp[1:3] == roast[1:3] and camp[3:13] == roast[3:13]
    assert camp[3:13] == ["0", "1", "0", "0", "0", "0", "0", "1", "1", "1"]
    camp_path = P / "models" / (assets["nm_city_region_camp"] + ".glb")
    roast_path = P / "models" / (assets["nm_city_region_roast"] + ".glb")
    cd, ca = glb(camp_path)
    rd, ra = glb(roast_path)
    cp = next(p for m in cd["meshes"] for p in m["primitives"]
              if cd["materials"][p["material"]].get("name") == "Charcoal and dry splits")
    cv = ca(cp["attributes"]["POSITION"])
    # The only high component of this material is the existing horizontal spit.
    rod = cv[cv[:, 1] > 1.1]
    assert len(rod) == 72 and len(np.unique(rod, axis=0)) == 24
    assert np.allclose(np.ptp(rod, axis=0), [2.29, .05, .05], atol=1e-6)
    rp = rd["meshes"][0]["primitives"][0]
    rv, ri = ra(rp["attributes"]["POSITION"]), ra(rp["indices"]).astype(int).reshape(-1, 3)
    assert len(rd["meshes"]) == 1 and len(rd["meshes"][0]["primitives"]) == 1
    assert rv[:, 1].min() == 0
    assert min(ca(p["attributes"]["POSITION"])[:, 1].min() for m in cd["meshes"] for p in m["primitives"]) == 0
    body_center = (rv.min(0) + rv.max(0)) * .5
    shaft_center = (rod.min(0) + rod.max(0)) * .5
    target = float(camp[13]) + shaft_center[1] - body_center[1]
    delta = target - float(roast[13])
    assert .17 < delta < .20 and abs(shaft_center[2] - body_center[2]) < 1e-7
    # Intersect the actual roast surface with the actual shaft axis, proving
    # this moves the bar through the body rather than merely matching AABBs.
    tri = rv[ri]
    e1, e2 = tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    start, direction = np.array([-.4, body_center[1], 0.]), np.array([.8, 0., 0.])
    h = np.cross(direction, e2)
    determinant = np.einsum("ij,ij->i", e1, h)
    inverse = np.divide(1., determinant, out=np.zeros_like(determinant), where=np.abs(determinant) > 1e-10)
    offset = start - tri[:, 0]
    u = inverse * np.einsum("ij,ij->i", offset, h)
    q = np.cross(offset, e1)
    v = inverse * (q @ direction)
    t = inverse * np.einsum("ij,ij->i", e2, q)
    hits = (np.abs(determinant) > 1e-10) & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0) & (t < 1)
    crossings = np.sort((start + t[hits, None] * direction)[:, 0])
    assert len(crossings) >= 2 and crossings[0] < -.04 and crossings[-1] > .04
    before = rows[index]
    roast[13] = f"{target:.12f}"
    rows[index] = " ".join(roast)
    after = ("\n".join(rows) + "\n").encode()
    report = dict(schema="valwood.camp-roast-placement.v1", passed=True, applied=False, gpu_used=False,
        tool_sha256=sha(Path(__file__)), file="scenery/starting_area_instances.txt", line=index + 1,
        before=before, after=rows[index], before_sha256=EXPECTED, after_sha256=hashlib.sha256(after).hexdigest(),
        changed_fields=[13], vertical_raise_m=delta, shaft_center_world_y=float(camp[13]) + shaft_center[1],
        central_axis_actual_triangle_crossings_local_x=crossings.tolist(),
        source_camp_glb=str(camp_path.resolve()), source_camp_sha256=sha(camp_path),
        source_roast_glb=str(roast_path.resolve()), source_roast_sha256=sha(roast_path),
        geometry_materials_collision_and_all_other_rows_preserved=True,
        native_visual_retest_required=True)
    if args.apply_after_journey:
        execution = json.loads((N / "review/journey-before-roast-placement/EXECUTION.json").read_text())
        assert execution["passed"] and execution["exit_code"] == 0 and not execution["timed_out"]
        assert not (OUT / "ROAST_PLACEMENT.json").exists()
        (OUT / "starting_area_instances.txt.before").write_bytes(path.read_bytes())
        path.write_bytes(after)
        assert sha(path) == report["after_sha256"]
        assert sha(camp_path) == report["source_camp_sha256"] and sha(roast_path) == report["source_roast_sha256"]
        report["applied"] = True
        patch_path = OUT / "ROW_PATCH.json"
        patch = json.loads(patch_path.read_text())
        assert not any(f["path"] == report["file"] for f in patch["files"])
        patch["files"].append(dict(path=report["file"], before_sha256=EXPECTED, after_sha256=report["after_sha256"]))
        patch["dependent_instance_changes"] = [report]
        patch_path.write_text(json.dumps(patch, indent=2) + "\n")
        (OUT / "ROAST_PLACEMENT.json").write_text(json.dumps(report, indent=2) + "\n")
        (OUT / "ROAST_PLACEMENT_PUBLICATION.json").write_text(json.dumps(dict(relative=report["file"],
            candidate=str(path), before_sha256=EXPECTED, after_sha256=report["after_sha256"]), indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
