#!/usr/bin/env python3
"""Stage bounded camp/road dressing changes as exact, reviewable row patches.

Only this unpublished generation's three regular data copies may be changed.
No model, live authoring file, terrain, source key, collider or alias is added.
The parent can merge each approved row after rechecking its original contents.
"""
from pathlib import Path
import collections
import copy
import hashlib
import importlib.util
import json
import math
import struct

import numpy as np

N = Path(__file__).resolve().parents[1]
P = N / "world-project"
ROOT = N.parents[1]
OUT = N / "review/scenery"
FILES = ("scenery/bep_environment_instances.txt", "scenery/demo_polish_instances.txt", "world.studio")
WIDTH, DEPTH = 100800., 100800. * 2818 / 5009
CAMP = np.array([-11988., 5172.])
KINDS = ("opening_verge_grass_long", "opening_verge_grass_broad", "bep_nature_clover", "bep_nature_fern")


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def pose(a):
    return dict(x=(float(a[1]) - .5) * WIDTH, z=(float(a[2]) - .5) * DEPTH,
                yaw_deg=float(a[3]), scale=float(a[4]), pitch_deg=float(a[8]), roll_deg=float(a[9]),
                sx=float(a[10]), sy=float(a[11]), sz=float(a[12]), y=float(a[13]))


def segment_distance(xz, a, b):
    d = b - a
    t = np.clip(np.dot(xz - a, d) / max(float(np.dot(d, d)), 1e-12), 0., 1.)
    return float(np.linalg.norm(xz - a - d * t))


def stage():
    assert N.name == "opening-polish-2026-11-18" and not (N / "PUBLISHED.json").exists()
    assert not OUT.exists(), "Use a fresh report bank; never stack this patch twice"
    assert all((P / name).is_file() and not (P / name).is_symlink() for name in FILES)
    before = {name: (P / name).read_bytes() for name in FILES}
    rows = {name: raw.decode().splitlines() for name, raw in before.items()}
    studio = rows["world.studio"]
    assert not any(line.startswith(("override ", "group ")) for line in studio)
    import shlex
    assets = {shlex.split(line)[1]: shlex.split(line)[2] for line in studio if line.startswith("asset ")}
    source_tool = ROOT / "docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py"
    spec = importlib.util.spec_from_file_location("opening_scenery_mesh", source_tool)
    mesh = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mesh)
    raw_height = (P / "ground/showcase.height").read_bytes()
    magic, nx, nz, xmin, zmin, sx, sz = struct.unpack_from("<8sII4d", raw_height)
    assert magic == b"VWHGT1\0\0" and sx == sz == 1
    heights = np.frombuffer(raw_height, "<f4", offset=48).reshape(nz, nx)

    def ground(x, z):
        ix, iz = (x - xmin) / sx, (z - zmin) / sz
        if not 0 <= ix < nx - 1 or not 0 <= iz < nz - 1:
            return None
        i, j = int(ix), int(iz)
        tx, tz = ix - i, iz - j
        return float((1 - tz) * ((1 - tx) * heights[j, i] + tx * heights[j, i + 1]) +
                     tz * ((1 - tx) * heights[j + 1, i] + tx * heights[j + 1, i + 1]))

    geometry = {kind: mesh.geometry(P / "models" / (assets[kind] + ".glb")) for kind in KINDS}
    route_data = json.loads((P / "opening/first-journey.routes.json").read_text())["paths"]
    routes = []
    for points in route_data.values():
        xz = np.asarray(points, dtype=float)[:, [0, 2]]
        routes.extend(zip(xz[:-1], xz[1:]))
    actors = [np.array([-11990.15, 5171.35]), np.array([-11985.8, 5171.35]), np.array([-11987.9, 5168.8])]
    # Irregular, low growth at the clearing margin and a few road bends. The
    # protected heart of camp, encounter and the broad traffic lane stay open.
    islands = [
        ("camp_west", -11996.6, 5178.6, 1.45),
        ("camp_east", -11977.5, 5174.4, 1.45),
        ("camp_rear", -11996.8, 5164.3, 1.45),
        ("camp_north", -11987.8, 5192.0, 1.55),
        ("road_west_1", -11977.7, 5211.0, 1.55),
        ("road_east_1", -11954.2, 5213.0, 1.55),
        ("road_west_2", -11956.5, 5249.0, 1.55),
        ("road_east_2", -11933.7, 5255.0, 1.55),
    ]
    available = collections.defaultdict(list)
    for name in FILES[:2]:
        for index, line in enumerate(rows[name]):
            a = line.split()
            if len(a) >= 14 and a[0] in KINDS:
                available[a[0]].append((name, index, pose(a)))
    used, edits, island_review = set(), [], []
    for island_index, (label, x, z, radius) in enumerate(islands):
        center = np.array([x, z])
        planted = []
        # Existing masked grass is genuinely root-weighted and interactive.
        # Mixing taller blades with low clover and ferns creates a layered edge.
        species = ("opening_verge_grass_long",) * 5 + ("opening_verge_grass_broad",) * 4 + \
                  ("bep_nature_clover",) * 2 + ("bep_nature_fern",) * 2
        for k, alias in enumerate(species):
            angle = island_index * .73 + k * 2.399963229728653
            radial = radius * math.sqrt((k + .35) / len(species))
            target = center + np.array([math.cos(angle) * radial, math.sin(angle) * radial * .70])
            if np.linalg.norm(target - CAMP) < 6.6:
                continue
            # Actual transformed foliage radius is checked below. A conservative
            # root guard avoids placing even low blades on the walking ribbon.
            lane = min(segment_distance(target, a, b) for a, b in routes)
            if lane < 4.8 or min(np.linalg.norm(target - a) for a in actors) < 3.2:
                continue
            choices = [(np.linalg.norm(np.array([p["x"], p["z"]]) - target), name, index, p)
                       for name, index, p in available[alias] if (name, index) not in used]
            distance, name, index, prior = min(choices, key=lambda item: (item[0], item[1], item[2]))
            assert distance < 45, (label, alias, distance)
            used.add((name, index))
            a = rows[name][index].split()
            v = geometry[alias]
            support = copy.deepcopy(prior)
            mesh.support(support, v, ground)
            prior_embed = support["y"] - prior["y"]
            new = copy.deepcopy(prior)
            new.update(x=float(target[0]), z=float(target[1]), yaw_deg=math.degrees(angle) % 360 - 180,
                       pitch_deg=0., roll_deg=0.)
            if "grass" in alias:
                new.update(scale=.92 + .08 * (k % 3), sx=.92 + .03 * (k % 4),
                           sy=1.05 + .10 * (k % 3), sz=.93 + .04 * (k % 3))
            elif "clover" in alias:
                new.update(scale=.26 + .035 * (k % 2), sx=1.0, sy=.77, sz=1.0)
            else:
                new.update(scale=.34 + .05 * (k % 2), sx=1.0, sy=.90, sz=1.0)
            mesh.align_plant(new, ground)
            # Some inherited donor roots are visibly floating or over-buried.
            # Reground the actual visible basal vertices instead of carrying
            # those errors to a smaller plant at the new site.
            embed = .012 if "grass" in alias else .018 if "clover" in alias else .04
            mesh.support(new, v, ground, embed)
            uniform = max(.15, new["scale"])
            factor = new["scale"] / uniform
            a[1], a[2] = f'{new["x"] / WIDTH + .5:.12g}', f'{new["z"] / DEPTH + .5:.12g}'
            a[3], a[4] = f'{new["yaw_deg"]:.10g}', f'{uniform:.10g}'
            for field, key in ((8, "pitch_deg"), (9, "roll_deg"), (10, "sx"), (11, "sy"), (12, "sz"), (13, "y")):
                a[field] = f'{new[key] * (factor if field in (10, 11, 12) else 1):.12g}'
            updated = " ".join(a)
            # Review the exact decimal representation consumed by production.
            serialized = pose(a)
            points = v * np.array([serialized["sx"], serialized["sy"], serialized["sz"]]) * serialized["scale"]
            points = points @ mesh.rotation(serialized["yaw_deg"], serialized["pitch_deg"], serialized["roll_deg"]).T
            horizontal_radius = float(np.linalg.norm(points[:, [0, 2]], axis=1).max())
            clearance = lane - horizontal_radius
            assert clearance > 3.1, (label, alias, clearance)
            assert .15 <= float(a[4]) <= 4 and all(.1 <= float(a[i]) <= 8 for i in (10, 11, 12))
            assert rows[name][index].split()[14:] == a[14:]
            old = rows[name][index]
            rows[name][index] = updated
            review = dict(file=name, line=index + 1, before=old, after=updated, alias=alias,
                          island=label, moved_m=float(distance), prior_basal_embedding_m=prior_embed,
                          staged_basal_embedding_m=embed,
                          actual_mesh_horizontal_radius_m=horizontal_radius,
                          actual_mesh_route_clearance_m=clearance,
                          visible_height_m=float(points[:, 1].max() - points[:, 1].min()))
            edits.append(review)
            planted.append(review)
        assert len(planted) >= 10, (label, len(planted))
        island_review.append(dict(name=label, center_xz=[x, z], plants=len(planted)))

    # Only existing camp-local surface records change. Layers/images/paint,
    # terrain grades, water ribbons and the other regions retain their budgets.
    replacements = {
        'paint -11988 5170 18 0.64 7 0': 'paint -11988 5170 16.5 0.76 7 0',
        'paint -11988 5172 5.15 0.90 5 0': 'paint -11988 5172 4.8 0.88 5 0',
        'paint -11988 5177 3.4 0.82 5 0': 'paint -11988 5177 2.8 0.78 5 0',
        'paint -11991 5170 3.5 0.8 5 0': 'paint -11991 5170 2.7 0.72 5 0',
        'paint -12005.51015 5175.378758 5.27871050 0.91 7 0':
        'paint -11996.6 5178.6 4.8 0.94 7 0',
        'paint -11992.63265 5189.709302 6.70410400 0.91 7 0':
        'paint -11987.8 5192 5.2 0.94 7 0',
        'paint -11972.6661 5182.817468 6.79928200 0.91 7 0':
        'paint -11977.5 5174.4 4.8 0.94 7 0',
        'paint -12003.97674 5161.539023 7.56960900 0.91 7 0':
        'paint -11996.8 5164.3 4.8 0.94 7 0',
    }
    studio_edits = []
    for index, line in enumerate(studio):
        if not line.startswith("surfaces "):
            continue
        value = json.loads(line[9:])
        if value in replacements:
            updated = "surfaces " + json.dumps(replacements[value])
            studio_edits.append(dict(file="world.studio", line=index + 1, before=line, after=updated,
                                     kind="camp_material"))
            studio[index] = updated
    assert len(studio_edits) == len(replacements), len(studio_edits)
    assert all((P / name).read_bytes() == before[name] for name in FILES), "Concurrent candidate edit"
    OUT.mkdir(parents=True)
    file_review = []
    for name in FILES:
        original = before[name]
        updated = ("\n".join(rows[name]) + "\n").encode()
        (OUT / (Path(name).name + ".before")).write_bytes(original)
        (P / name).write_bytes(updated)
        file_review.append(dict(path=name, before_sha256=sha(original), after_sha256=sha(updated)))
    report = dict(schema="valwood.opening-scenery-row-patch.v1", staged=True, passed=False,
                  native_visual_review_required=True, gpu_used=False, files=file_review,
                  instance_changes=edits, studio_changes=studio_edits, islands=island_review,
                  changed_rows=len(edits) + len(studio_edits), added_instances=0, removed_instances=0,
                  source_aliases_and_row_order_unchanged=True, wind_metadata_unchanged=True,
                  colliders_and_tree_instances_unchanged=True, terrain_and_ribbons_unchanged=True,
                  model_geometry_and_texture_files_changed=0,
                  height_sha256=sha(raw_height), geometry_tool_sha256=sha(source_tool.read_bytes()),
                  publish_policy="Recheck original live contents, dry-run each exact before/after row, backup then merge approved rows only; abort on any conflict.",
                  limitations=["No AAA or performance claim before integrated native review.",
                               "No physical plant bending change; retained root-weighted grass uses the existing interaction shader."])
    (OUT / "ROW_PATCH.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(changed_instances=len(edits), changed_material_rows=len(studio_edits), files=file_review), indent=2))


if __name__ == "__main__":
    stage()
