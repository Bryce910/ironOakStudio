#!/usr/bin/env python3
"""Refit existing TV/bill presentation to the corrected private apartment.

Candidate only: source GLBs, live project and published shaders remain immutable.
All geometry changes are translations of the five already-fitted primitives.
"""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct

import numpy as np

N = Path(__file__).resolve().parents[1]
P = N / "world-project"
ROOT = N.parents[2]
ALIGNMENT = ROOT / "docs/apartment-collision/2026-11-18/DEPENDENT_ALIGNMENT_REVIEW.json"
STEM = "home_story_aligned_20261118"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def owned(path: Path) -> None:
    assert not path.is_symlink(), f"Mutable path is a symlink: {path}"
    assert path.resolve().is_relative_to(N), f"Path escapes candidate: {path}"


def read_glb(path: Path):
    raw = path.read_bytes()
    magic, version, size = struct.unpack_from("<III", raw)
    assert magic == 0x46546C67 and version == 2 and size == len(raw)
    length, kind = struct.unpack_from("<II", raw, 12)
    assert kind == 0x4E4F534A
    doc = json.loads(raw[20:20 + length])
    offset = 20 + length
    length, kind = struct.unpack_from("<II", raw, offset)
    assert kind == 0x004E4942 and offset + 8 + length == len(raw)
    return doc, bytearray(raw[offset + 8:])


def access(doc, data, index):
    accessor = doc["accessors"][index]
    view = doc["bufferViews"][accessor["bufferView"]]
    assert not accessor.get("sparse")
    dtype = {5126: "<f4", 5125: "<u4", 5123: "<u2", 5121: "u1"}[accessor["componentType"]]
    width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[accessor["type"]]
    size = np.dtype(dtype).itemsize
    return np.ndarray((accessor["count"], width), dtype=dtype, buffer=data,
                      offset=view.get("byteOffset", 0) + accessor.get("byteOffset", 0),
                      strides=(view.get("byteStride", size * width), size))


def glb_bytes(doc, data):
    encoded = json.dumps(doc, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    data = bytes(data) + b"\0" * (-len(data) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(data)) +
            struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
            struct.pack("<II", len(data), 0x004E4942) + data)


def translated(values, delta):
    out = list(values)
    out[2] += delta
    return out


def main():
    assert N.name == "apartment-collision-2026-11-18"
    for container in (N, P, P / "models", P / "models/opening", P / "opening",
                      P / "scenery", N / "shared", N / "shared/shaders",
                      N / "assets", N / "assets/apartment-aligned", N / "tools", N / "review"):
        owned(container)
    report_path = N / "review/DEPENDENT_REFIT.json"
    assert not report_path.exists(), "This immutable refit has already run"
    review = json.loads(ALIGNMENT.read_text())
    manifest_path = Path(review["source_manifest"])
    assert sha(manifest_path) == review["source_manifest_sha256"]
    manifest = json.loads(manifest_path.read_text())
    scenery_path = P / "scenery/authored_instances.txt"
    scenery_sha = sha(scenery_path)
    rows = {q[0]: q for line in scenery_path.read_text().splitlines() if (q := line.split())}
    # The root already corrected these 593 rows. Assert every original native
    # origin is now represented exactly, and never write the scenery file.
    depth = 100800. * 2818. / 5009.
    for record in manifest["records"]:
        q = rows[record["alias"]]
        origin = record["origin_native"]
        world = [(float(q[1]) - .5) * 100800., float(q[-1]), (float(q[2]) - .5) * depth]
        assert np.max(np.abs(np.array(world) - [-3500 + origin[0], 600 + origin[1], -11000 + origin[2]])) < 1e-8

    rels = ("world.studio", "opening.config", "opening.scene", "opening/cinematic.settings", "opening/cinematic.json")
    original = {rel: (P / rel).read_bytes() for rel in rels}
    for rel in rels:
        owned(P / rel)
    shader_path = N / "shared/shaders/world_foliage.fs.glsl"
    owned(shader_path)
    original_shader = shader_path.read_bytes()
    assert sha(shader_path) == review["shader_sha256"]
    tv_delta = review["overlay"]["television_delta_z"]
    bill_delta = review["overlay"]["bill_delta_z"]
    files = {}
    sources = []
    assets = []
    links = []
    for suffix in ("", "_mid"):
        source = P / f"models/opening/home_story_tvfit_20261118{suffix}.glb"
        doc, binary = read_glb(source)
        doc_before, binary_before = copy.deepcopy(doc), bytes(binary)
        changes = {}
        for mesh in doc["meshes"]:
            for primitive in mesh["primitives"]:
                name = doc["materials"][primitive["material"]].get("name", "")
                shift = review["overlay"]["primitive_changes"].get(name)
                if shift is None:
                    continue
                index = primitive["attributes"]["POSITION"]
                assert index not in changes, "Shared positions require explicit review"
                position = access(doc, binary, index)
                assert position.shape[1] == 3 and position.dtype == np.dtype("<f4")
                before = position.copy()
                position[:] = position.astype(np.float64) + np.array(shift)
                assert np.max(np.abs(position.astype(np.float64) - before - np.array(shift))) < 2e-6
                doc["accessors"][index]["min"] = position.min(0).tolist()
                doc["accessors"][index]["max"] = position.max(0).tolist()
                changes[index] = {"material": name, "translation": shift, "vertices": len(position)}
        assert len(changes) == 5
        # Reconstruct the exact allowed binary and JSON modifications. No UV,
        # normals, materials, embedded images or unrelated geometry may change.
        expected_binary = bytearray(binary_before)
        for index in changes:
            access(doc_before, expected_binary, index)[:] = access(doc, binary, index)
            doc_before["accessors"][index]["min"] = doc["accessors"][index]["min"]
            doc_before["accessors"][index]["max"] = doc["accessors"][index]["max"]
        assert expected_binary == binary and doc_before == doc
        destination = N / f"assets/apartment-aligned/{STEM}{suffix}.glb"
        owned(destination.parent)
        assert not destination.exists() and not destination.is_symlink()
        encoded = glb_bytes(doc, binary)
        assets.append((destination, encoded))
        link = P / f"models/opening/{STEM}{suffix}.glb"
        assert not link.exists() and not link.is_symlink()
        links.append((link, destination))
        sources.append({"source": str(source.resolve()), "source_link": str(source), "source_sha256": sha(source),
                        "candidate_asset": str(destination), "candidate_link": str(link),
                        "after_sha256": hashlib.sha256(encoded).hexdigest(), "changed_accessors": changes,
                        "all_other_binary_and_json_preserved": True})

    studio = original["world.studio"].decode().splitlines()
    old_asset = "asset nm_opening_reference_home_story opening/home_story_tvfit_20261118 2 0 0 240 75 1"
    assert studio.count(old_asset) == 1
    studio[studio.index(old_asset)] = f"asset nm_opening_reference_home_story opening/{STEM} 2 0 0 240 75 1"
    changed_studio_lines = [studio.index(f"asset nm_opening_reference_home_story opening/{STEM} 2 0 0 240 75 1") + 1]
    for pane in review["daylight_lights_and_panes"]:
        line = pane["world_studio_line"] - 1
        tokens = studio[line].split()
        assert tokens[0] == "light" and abs(float(tokens[3]) - pane["light_z_before"]) < 1e-9
        tokens[3] = f'{pane["light_z_after"]:.12f}'
        studio[line] = " ".join(tokens)
        changed_studio_lines.append(line + 1)
    assert sum(a != b for a, b in zip(studio, original["world.studio"].decode().splitlines())) == 8
    files["world.studio"] = ("\n".join(studio) + "\n").encode()

    config = original["opening.config"].decode().splitlines()
    changed_config = []
    for index, line in enumerate(config):
        tokens = line.split()
        if not tokens:
            continue
        if tokens[0] == "house_focus":
            tokens[1:4] = [f"{v:.12f}" for v in review["overlay"]["house_focus_after"]]
        elif tokens[0] == "house_exit":
            tokens[1:4] = [f"{v:.12f}" for v in review["overlay"]["house_exit_after"]]
        elif tokens[0] == "transport_rail":
            for column in (3, 6, 9, 12):
                tokens[column] = f"{float(tokens[column]) + tv_delta:.12f}"
        else:
            continue
        config[index] = " ".join(tokens)
        changed_config.append(tokens[0])
    assert sorted(changed_config) == ["house_exit", "house_focus", "transport_rail"]
    files["opening.config"] = ("\n".join(config) + "\n").encode()

    settings = original["opening/cinematic.settings"].decode().splitlines()
    shot_indices = [i for i, line in enumerate(settings) if line.startswith("shot ")]
    assert len(shot_indices) == 4
    for index, delta in ((1, bill_delta), (2, tv_delta)):
        line = shot_indices[index]
        tokens = settings[line].split()
        for column in (4, 7, 10, 13):
            tokens[column] = f"{float(tokens[column]) + delta:.12f}"
        settings[line] = " ".join(tokens)
    assert sum(a != b for a, b in zip(settings, original["opening/cinematic.settings"].decode().splitlines())) == 2
    files["opening/cinematic.settings"] = ("\n".join(settings) + "\n").encode()

    cinematic = json.loads(original["opening/cinematic.json"])
    assert cinematic["shots"][1]["name"] == "still-behind" and cinematic["shots"][2]["name"] == "one-quiet-night"
    for index, delta in ((1, bill_delta), (2, tv_delta)):
        for key in ("eye_from", "eye_to", "target_from", "target_to"):
            cinematic["shots"][index][key] = translated(cinematic["shots"][index][key], delta)
    files["opening/cinematic.json"] = (json.dumps(cinematic, indent=2) + "\n").encode()
    scene = json.loads(original["opening.scene"])
    scene["home"]["inspect"]["position"] = review["overlay"]["house_focus_after"]
    scene["home"]["console"]["position"] = review["overlay"]["house_exit_after"]
    files["opening.scene"] = (json.dumps(scene, indent=2) + "\n").encode()

    shader = original_shader.decode()
    start = shader.index("bool referenceDaylightPane(vec3 p){")
    end = shader.index("\nfloat referenceRoomWeight", start)
    block = shader[start:end]
    matches = list(re.finditer(r"vec3\(([^)]+)\)", block))
    assert len(matches) == 14
    replaced = block
    for index in reversed(range(len(matches))):
        match = matches[index]
        values = [float(v) for v in match[1].split(",")]
        pane = review["daylight_lights_and_panes"][index // 2]
        expected = pane["shader_pane_min_before" if index % 2 == 0 else "shader_pane_max_before"]
        assert values == expected
        values[2] += pane["delta_z"]
        replacement = "vec3(" + ",".join(f"{v:.9f}" for v in values) + ")"
        replaced = replaced[:match.start()] + replacement + replaced[match.end():]
    shader = shader[:start] + replaced + shader[end:]
    for name, old_z in (("LED min", -11003.111), ("LED max", -11003.089),
                       ("TV min", -11004.121), ("TV max", -11003.375)):
        old = f",{old_z:.9f})"
        new = f",{old_z + tv_delta:.9f})"
        assert shader.count(old) == 1, name
        shader = shader.replace(old, new, 1)
    # Reversing exactly18 vector literals must recover every original byte.
    original_vectors = re.findall(r"vec3\([^)]+\)", original_shader.decode())
    new_vectors = re.findall(r"vec3\([^)]+\)", shader)
    assert len(original_vectors) == len(new_vectors)
    substitutions = [(a, b) for a, b in zip(original_vectors, new_vectors) if a != b]
    assert len(substitutions) == 18
    reversed_shader = shader
    for before, after in substitutions:
        assert reversed_shader.count(after) == 1
        reversed_shader = reversed_shader.replace(after, before, 1)
    assert reversed_shader.encode() == original_shader

    backup = N / "review/dependent-refit-before"
    assert not backup.exists()
    backup.mkdir()
    publications = []
    for rel, data in files.items():
        target = P / rel
        dest = backup / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(original[rel])
        target.write_bytes(data)
        publications.append({"relative": rel, "candidate": str(target), "before_sha256": hashlib.sha256(original[rel]).hexdigest(), "after_sha256": sha(target)})
    (backup / "world_foliage.fs.glsl").write_bytes(original_shader)
    shader_path.write_text(shader)
    for path, data in assets:
        path.write_bytes(data)
    for link, target in links:
        link.symlink_to(target)
        publications.append({"relative": str(link.relative_to(P)), "candidate": str(link), "asset": str(target), "after_sha256": sha(link), "new_unique_asset": True})
    assert sha(scenery_path) == scenery_sha
    for source in sources:
        assert sha(Path(source["source"])) == source["source_sha256"]
    report = {"schema": "valwood.apartment-dependent-refit.v1", "passed": True,
              "candidate": str(N), "live_modified": False, "gpu_launched": False,
              "tool": str(Path(__file__).resolve()), "tool_sha256": sha(Path(__file__)),
              "alignment_review": str(ALIGNMENT), "alignment_review_sha256": sha(ALIGNMENT),
              "corrected_scenery_untouched_sha256": scenery_sha,
              "tv_delta_z": tv_delta, "bill_delta_z": bill_delta,
              "geometry": sources, "world_studio_changed_lines": sorted(changed_studio_lines),
              "world_studio_other_lines_preserved": True,
              "shader": {"candidate": str(shader_path), "before_sha256": hashlib.sha256(original_shader).hexdigest(),
                         "after_sha256": sha(shader_path), "changed_domains": 9, "changed_vector_literals": 18,
                         "all_other_bytes_preserved": True},
              "publication_files": publications,
              "preserved": ["physics", "enclosure", "room controls", "practical lights", "probes", "spawn", "waypoints", "cinematic shots0/3", "all shader logic"],
              "validation_scope": "CPU asset translation/byte preservation and document consistency only; native visual and capsule checks remain required."}
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    (N / "review/DEPENDENT_REFIT_PUBLICATION.json").write_text(json.dumps({"project_files": publications, "shared_shader": report["shader"]}, indent=2) + "\n")
    print(json.dumps({"passed": True, "receipt": str(report_path), "new_stem": STEM, "project_files": len(publications), "shader_domains": 9}, indent=2))


if __name__ == "__main__":
    main()
