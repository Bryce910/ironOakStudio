#!/usr/bin/env python3
"""Compose bounded, actually grounded scenery along the complete opening road.

Unpublished candidate only. Reuses owned resident assets and textures, keeps
terrain, water, tree collisions and all critical paths unchanged. Paint/ribbon
budgets are already full, so existing local litter records are recomposed.
"""
from pathlib import Path
import collections
import ctypes
import hashlib
import importlib.util
import json
import math
import shlex
import struct
import sys

import numpy as np

N = Path(__file__).resolve().parents[1]
P = N / "world-project"
ROOT = N.parents[1]
OUT = N / "review/scenery"
WIDTH, DEPTH = 100800., 100800. * 2818. / 5009.
CAMP = np.array([-11988., 5172.])
KINDS = ("opening_verge_grass_long", "opening_verge_grass_broad", "bep_nature_clover",
         "bep_nature_fern", "bep_nature_bush_a", "bep_nature_bush_b",
         "opening_road_roots", "opening_road_stone")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def segment_distance(point, a, b):
    direction = b - a
    t = np.clip(np.dot(point - a, direction) / max(float(np.dot(direction, direction)), 1e-12), 0., 1.)
    return float(np.linalg.norm(point - a - direction * t))


def read_pose(q):
    return dict(x=(float(q[1]) - .5) * WIDTH, z=(float(q[2]) - .5) * DEPTH,
                yaw_deg=float(q[3]), scale=float(q[4]), pitch_deg=float(q[8]),
                roll_deg=float(q[9]), sx=float(q[10]), sy=float(q[11]), sz=float(q[12]), y=float(q[13]))


def main():
    assert N.name == "camp-road-polish-2026-11-19" and not (N / "PUBLISHED.json").exists()
    assert not (OUT / "ROW_PATCH.json").exists(), "Do not stack this deterministic pass"
    files = ("world.studio", "scenery/demo_polish_instances.txt")
    assert all((P / name).is_file() and not (P / name).is_symlink() for name in files)
    before = {name: (P / name).read_bytes() for name in files}
    studio = before["world.studio"].decode().splitlines()
    assert not any(line.startswith(("override ", "group ")) for line in studio)
    aliases = {shlex.split(line)[1]: shlex.split(line)[2] for line in studio if line.startswith("asset ")}
    source_tool = ROOT / "docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py"
    spec = importlib.util.spec_from_file_location("camp_road_grounding", source_tool)
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

    geometry, budgets = {}, {}
    for alias in KINDS:
        path = P / "models" / (aliases[alias] + ".glb")
        geometry[alias] = mesh.geometry(path)
        raw = path.read_bytes()
        length = struct.unpack_from("<I", raw, 12)[0]
        doc = json.loads(raw[20:20 + length])
        budgets[alias] = dict(path=str(path), source_sha256=sha(raw), bytes=len(raw),
            vertices=sum(doc["accessors"][p["attributes"]["POSITION"]]["count"] for m in doc["meshes"] for p in m["primitives"]),
            triangles=sum(doc["accessors"][p["indices"]]["count"] // 3 for m in doc["meshes"] for p in m["primitives"]))
    route_file = P / "opening/first-journey.routes.json"
    route_data = json.loads(route_file.read_text())["paths"]
    routes = []
    for points in route_data.values():
        points = np.array(points)[:, [0, 2]]
        routes.extend(zip(points[:-1], points[1:]))
    traffic = []
    for line in (P / "opening/road-traffic.settings").read_text().splitlines():
        q = line.split()
        if q and q[0] == "point":
            traffic.append(np.array([float(q[1]), float(q[3])]))
    routes.extend(zip(traffic[:-1], traffic[1:]))
    actors = [np.array([-11990.15, 5171.35]), np.array([-11985.8, 5171.35]), np.array([-11987.9, 5168.8])]
    protected = [(CAMP, 5.8), (np.array([-11933., 5312.]), 9.),
                 (np.array([-11930.5, 5320.]), 4.6), (np.array([-11968., 5582.]), 5.5)]
    trees, occupied = [], []
    original_scenery = {}
    for path in sorted((P / "scenery").glob("*.txt")):
        original_scenery[str(path.relative_to(P))] = sha(path.read_bytes())
        for line in path.read_text().splitlines():
            q = line.split()
            if len(q) < 14 or q[0].startswith("#"):
                continue
            try:
                pose = read_pose(q)
            except ValueError:
                continue
            point = np.array([pose["x"], pose["z"]])
            if -12060 < point[0] < -11870 and 5130 < point[1] < 5620:
                occupied.append((q[0], point))
                if q[0] in ("nm_city_region_oak", "nm_city_region_beech"):
                    trees.append(point)

    rng = np.random.default_rng(20261119)
    nodes = np.array([[-11988., 5172.], [-11943., 5252.], [-11928., 5342.],
                      [-11948., 5432.], [-11968., 5522.], [-11968., 5582.]])
    lengths = np.linalg.norm(np.diff(nodes, axis=0), axis=1)
    cumulative = np.concatenate([[0.], np.cumsum(lengths)])

    def along(distance):
        index = min(len(lengths) - 1, int(np.searchsorted(cumulative, distance, side="right") - 1))
        direction = (nodes[index + 1] - nodes[index]) / lengths[index]
        return nodes[index] + direction * (distance - cumulative[index]), np.array([direction[1], -direction[0]])

    clusters = []
    # Compose an asymmetric camp rim, keeping the view toward Rowan and the
    # fire readable. Taller plants sit behind low, broken clover/grass edges.
    for i, (x, z, radius) in enumerate([(-11999.2, 5165.2, 2.6), (-11998.3, 5179.5, 2.7),
        (-11990.8, 5188.6, 2.5), (-11978.3, 5183.7, 2.7), (-11976.1, 5170.9, 2.7),
        (-11982.1, 5161.1, 2.6), (-11994.4, 5159.7, 2.5), (-12003.4, 5173.0, 2.4)]):
        clusters.append(dict(name=f"camp_rim_{i}", point=np.array([x, z]), radius=radius, shade=True, count=28))
    # Deliberately uneven intervals and sides avoid the appearance of repeated
    # border planting. Where possible each island belongs to an existing tree.
    for i, distance in enumerate([30, 49, 71, 96, 120, 147, 177, 205, 234, 263, 290, 316, 341, 364, 386, 405]):
        centre, right = along(distance)
        side = -1 if i % 3 in (0, 1) else 1
        desired = centre + right * side * (7.0 + (i % 3) * 1.1)
        candidates = [p for p in trees if np.linalg.norm(p - desired) < 8 and
                      min(segment_distance(p, a, b) for a, b in routes) > 6.0]
        shaded = bool(candidates) and distance < 355
        if shaded:
            tree = min(candidates, key=lambda p: np.linalg.norm(p - desired))
            desired = tree + right * side * 1.1
        clusters.append(dict(name=f"road_{i:02d}", point=desired, radius=2.5 if shaded else 2.0,
                             shade=shaded, count=24 if distance < 355 else 16))
    # Openings on the sunny side are left intentional; these smaller islands
    # are offset from the main groups rather than mirrored across the road.
    for i, distance in enumerate([59, 132, 218, 303, 353]):
        centre, right = along(distance)
        clusters.append(dict(name=f"sunlit_pocket_{i}", point=centre + right * (6.4 if i % 2 else -7.1),
                             radius=1.9, shade=False, count=16))

    placements, skips = [], collections.Counter()
    new_roots = []

    def add(alias, target, label, shade, index, kind="plant"):
        target = np.asarray(target, dtype=float)
        lane = min(segment_distance(target, a, b) for a, b in routes)
        if any(np.linalg.norm(target - centre) < radius for centre, radius in protected):
            skips["protected_interaction_radius"] += 1
            return False
        if min(np.linalg.norm(target - a) for a in actors) < 3.5:
            skips["actor_clearance"] += 1
            return False
        if any(np.linalg.norm(target - p) < (.12 if kind != "plant" else .18 if "grass" in alias else .4) for _, p in new_roots):
            skips["cluster_spacing"] += 1
            return False
        # Avoid inserting plant bodies into existing trunks or their blockers.
        if kind == "plant" and trees and min(np.linalg.norm(target - p) for p in trees) < .65:
            skips["tree_root_clearance"] += 1
            return False
        pose = dict(x=float(target[0]), z=float(target[1]), yaw_deg=float(rng.uniform(-180, 180)),
                    pitch_deg=0., roll_deg=0., scale=1., sx=1., sy=1., sz=1.)
        if alias == "opening_verge_grass_long":
            pose.update(scale=float(rng.uniform(1.1, 1.7)), sx=float(rng.uniform(.88, 1.16)),
                        sy=float(rng.uniform(1.15, 1.55)), sz=float(rng.uniform(.85, 1.14)))
        elif alias == "opening_verge_grass_broad":
            pose.update(scale=float(rng.uniform(1.05, 1.50)), sy=float(rng.uniform(.92, 1.30)))
        elif alias == "bep_nature_clover":
            pose.update(scale=float(rng.uniform(.20, .34)), sy=float(rng.uniform(.65, .9)))
        elif alias == "bep_nature_fern":
            pose.update(scale=float(rng.uniform(.29, .46)), sy=float(rng.uniform(.88, 1.12)))
        elif alias.startswith("bep_nature_bush"):
            pose.update(scale=.15, sx=float(rng.uniform(.55, .78)), sy=float(rng.uniform(.40, .58)), sz=float(rng.uniform(.55, .78)))
        elif alias == "opening_road_roots":
            pose.update(scale=float(rng.uniform(.22, .37)), sy=float(rng.uniform(.36, .53)))
        else:
            pose.update(scale=float(rng.uniform(.15, .23)), sx=float(rng.uniform(.65, .96)),
                        sy=float(rng.uniform(.40, .62)), sz=float(rng.uniform(.7, 1.0)))
        mesh.align_plant(pose, ground)
        embed = .014 if "grass" in alias else .022 if "clover" in alias else .035 if kind == "plant" else .06
        visible_height = mesh.support(pose, geometry[alias], ground, embed)
        if abs(pose["pitch_deg"]) > 22 or abs(pose["roll_deg"]) > 22:
            skips["steep_grade"] += 1
            return False
        points = geometry[alias] * np.array([pose["sx"], pose["sy"], pose["sz"]]) * pose["scale"]
        points = points @ mesh.rotation(pose["yaw_deg"], pose["pitch_deg"], pose["roll_deg"]).T
        radius = float(np.linalg.norm(points[:, [0, 2]], axis=1).max())
        clearance = lane - radius
        if clearance < (3.2 if kind == "plant" else 3.8):
            skips["actual_mesh_route_clearance"] += 1
            return False
        q = [alias, f'{pose["x"] / WIDTH + .5:.12f}', f'{pose["z"] / DEPTH + .5:.12f}',
             f'{pose["yaw_deg"]:.8f}', f'{pose["scale"]:.8f}', "0", "0", "0",
             f'{pose["pitch_deg"]:.8f}', f'{pose["roll_deg"]:.8f}', f'{pose["sx"]:.8f}',
             f'{pose["sy"]:.8f}', f'{pose["sz"]:.8f}', f'{pose["y"]:.9f}']
        if kind == "plant":
            q += ["wind", f'{rng.uniform(.82, 1.18):.5f}', f'{rng.uniform(.80, 1.12):.5f}']
        text = " ".join(q)
        assert len(text) <= 320 and len(" ".join(q[:14])) <= 256
        serialized = read_pose(q)
        # Basal support is reviewed after decimal serialization, against the
        # actual visible source vertices rather than a guessed pivot sphere.
        transformed = geometry[alias] * np.array([serialized["sx"], serialized["sy"], serialized["sz"]]) * serialized["scale"]
        transformed = transformed @ mesh.rotation(serialized["yaw_deg"], serialized["pitch_deg"], serialized["roll_deg"]).T
        low = transformed[:, 1].min()
        basal = transformed[transformed[:, 1] <= low + max(.02, visible_height * .025)]
        basal = basal[np.linspace(0, len(basal) - 1, min(64, len(basal)), dtype=int)]
        gaps = [serialized["y"] + p[1] - ground(serialized["x"] + p[0], serialized["z"] + p[2]) for p in basal]
        if float(np.quantile(gaps, .9)) > (.065 if kind == "plant" else .12):
            skips["basal_float_guard"] += 1
            return False
        placements.append(dict(alias=alias, cluster=label, shade=shade, row=text,
            x=serialized["x"], z=serialized["z"], height_m=visible_height, actual_mesh_radius_m=radius,
            route_clearance_m=clearance, basal_gap_p10_m=float(np.quantile(gaps, .1)),
            basal_gap_p90_m=float(np.quantile(gaps, .9)), embed_m=embed, kind=kind))
        new_roots.append((alias, target))
        return True

    for cluster in clusters:
        planted = 0
        for k in range(cluster["count"] * 3):
            if planted >= cluster["count"]:
                break
            angle = k * 2.399963229728653 + rng.uniform(-.22, .22)
            distance = cluster["radius"] * math.sqrt(rng.uniform(.025, 1.0))
            target = cluster["point"] + np.array([math.cos(angle) * distance, math.sin(angle) * distance * .72])
            if cluster["shade"]:
                species = ("opening_verge_grass_long", "opening_verge_grass_broad", "bep_nature_fern",
                           "opening_verge_grass_broad", "bep_nature_clover", "opening_verge_grass_long")
            else:
                species = ("opening_verge_grass_long", "opening_verge_grass_long", "opening_verge_grass_broad",
                           "bep_nature_clover", "opening_verge_grass_broad")
            alias = species[planted % len(species)]
            planted += int(add(alias, target, cluster["name"], cluster["shade"], k))
        cluster["planted"] = planted
        # A single low woody silhouette anchors selected shade islands. No
        # collidable full-size bushes or trees are inserted on the road.
        if cluster["shade"] and planted >= 10:
            add("bep_nature_bush_a" if len(cluster["name"]) % 2 else "bep_nature_bush_b",
                cluster["point"] + [-.7, 1.0], cluster["name"], True, 0)
    # Sparse separate root/stone arrangements give the eye something beyond
    # plants, keeping the central road and mounted-traffic lane unobstructed.
    for i, cluster in enumerate(clusters):
        if i % 2 == 0:
            add("opening_road_roots", cluster["point"] + [1.5, -.8], cluster["name"], cluster["shade"], i, "root")
        if i % 3 == 0:
            add("opening_road_stone", cluster["point"] + [-1.6, -.5], cluster["name"], cluster["shade"], i, "stone")
    # Broken edge growth, never a continuous identical hedge or a blanket.
    for i, distance in enumerate(np.arange(22., 399., 6.8)):
        centre, right = along(float(distance))
        target = centre + right * ((-1 if i % 2 else 1) * rng.uniform(4.1, 4.8))
        add("opening_verge_grass_long" if i % 3 else "opening_verge_grass_broad", target, "broken_road_edge", False, i)

    assert 520 <= len(placements) <= 850, len(placements)
    # Recompose existing overbroad, mutually overlapping woodland masks into
    # irregular tree-associated litter pockets distributed along the whole road.
    donors = []
    for index, line in enumerate(studio):
        if not line.startswith("surfaces "):
            continue
        value = json.loads(line[9:])
        q = value.split()
        if q[0] == "paint" and len(q) == 7 and q[5] == "1" and -11970 < float(q[1]) < -11910 and 5285 < float(q[2]) < 5415:
            donors.append(index)
    assert len(donors) == 21, len(donors)
    targets = [c for c in clusters if c["planted"] >= 12 and c["name"] != "broken_road_edge"]
    # Evenly sample the full narrative route; include rear/side camp framing.
    chosen = [targets[i] for i in np.linspace(0, len(targets) - 1, len(donors), dtype=int)]
    studio_changes = []
    for k, (index, cluster) in enumerate(zip(donors, chosen)):
        point = cluster["point"] + np.array([math.sin(k * 1.7) * .6, math.cos(k * 1.1) * .8])
        radius = 3.7 + .38 * (k % 5)
        amount = .68 if cluster["shade"] else .43
        layer = 1 if cluster["shade"] else 7
        updated = "surfaces " + json.dumps(f"paint {point[0]:.7f} {point[1]:.7f} {radius:.3f} {amount:.3f} {layer} 0")
        studio_changes.append(dict(line=index + 1, before=studio[index], after=updated, kind="shade_litter_composition", cluster=cluster["name"]))
        studio[index] = updated
    # Match root-owned new fire material assets, without touching the actual
    # camp geometry, fire/smoke settings or practical-light rows.
    for alias, old, new in (("nm_city_region_camp", "opening/arrival_camp", "opening/camp_hearth_polish_20261119"),
                            ("nm_city_region_roast", "opening/arrival_roast", "opening/camp_roast_polish_20261119")):
        matches = [i for i, line in enumerate(studio) if line.startswith("asset " + alias + " ")]
        assert len(matches) == 1
        index = matches[0]
        q = studio[index].split()
        assert q[2] == old and (P / "models" / (new + ".glb")).exists()
        q[2] = new
        updated = " ".join(q)
        studio_changes.append(dict(line=index + 1, before=studio[index], after=updated, kind="root_owned_fire_material_alias"))
        studio[index] = updated
    # Extend only tiny root/stone detail visibility. The same already-resident
    # single meshes remain shared; all foliage LOD distances stay untouched.
    for alias in ("opening_road_roots", "opening_road_stone"):
        index = next(i for i, line in enumerate(studio) if line.startswith("asset " + alias + " "))
        q = studio[index].split()
        assert q[6] == "30"
        q[6] = "65"
        updated = " ".join(q)
        studio_changes.append(dict(line=index + 1, before=studio[index], after=updated, kind="small_detail_visibility_30_to_65m"))
        studio[index] = updated

    count = collections.Counter(p["alias"] for p in placements)
    total_vertices = sum(budgets[a]["vertices"] * c for a, c in count.items())
    total_triangles = sum(budgets[a]["triangles"] * c for a, c in count.items())
    peak_vertices = peak_triangles = peak_instances = 0
    for distance in np.arange(0, cumulative[-1] + 1, 3.):
        point, _ = along(float(distance))
        active = [p for p in placements if np.linalg.norm(np.array([p["x"], p["z"]]) - point) < (65 if p["kind"] != "plant" else 75)]
        peak_vertices = max(peak_vertices, sum(budgets[p["alias"]]["vertices"] for p in active))
        peak_triangles = max(peak_triangles, sum(budgets[p["alias"]]["triangles"] for p in active))
        peak_instances = max(peak_instances, len(active))
    assert peak_vertices < 250000 and peak_triangles < 240000, (peak_vertices, peak_triangles, peak_instances, dict(count))
    assert all((P / name).read_bytes() == before[name] for name in files), "Concurrent authoring change"
    assert all(sha((P / name).read_bytes()) == digest for name, digest in original_scenery.items())
    OUT.mkdir(parents=True, exist_ok=True)
    reviews = []
    staged_rows = before["scenery/demo_polish_instances.txt"].decode().splitlines()
    start_line = len(staged_rows) + 2
    staged_rows += ["# Camp-road20261119: composed rooted understory; existing route/terrain preserved."] + [p["row"] for p in placements]
    outputs = {"world.studio": ("\n".join(studio) + "\n").encode(),
               "scenery/demo_polish_instances.txt": ("\n".join(staged_rows) + "\n").encode()}
    for name in files:
        (OUT / (Path(name).name + ".before")).write_bytes(before[name])
        (P / name).write_bytes(outputs[name])
        reviews.append(dict(path=name, before_sha256=sha(before[name]), after_sha256=sha(outputs[name])))
    for i, p in enumerate(placements):
        p["file"] = "scenery/demo_polish_instances.txt"
        p["line"] = start_line + i
        p["before"] = None
        p["after"] = p.pop("row")
    report = dict(schema="valwood.camp-road-scenery.v1", staged=True, gpu_used=False, native_visual_review_required=True,
        tool_sha256=sha(Path(__file__).read_bytes()), files=reviews, changed_studio_rows=studio_changes,
        instance_changes=placements, added_instances=len(placements), removed_instances=0,
        species_counts=dict(count), clusters=[dict(name=c["name"], center_xz=c["point"].tolist(), radius=c["radius"], shade=c["shade"], planted=c["planted"]) for c in clusters],
        road_length_m=float(cumulative[-1]), route_minimum_actual_mesh_clearance_m=min(p["route_clearance_m"] for p in placements),
        geometry_assets=budgets, added_instanced_vertices_total=total_vertices, added_instanced_triangles_total=total_triangles,
        conservative_peak_75m_near_vertices=peak_vertices, conservative_peak_75m_near_triangles=peak_triangles, conservative_peak_near_instances=peak_instances,
        added_unique_geometry_bytes=0, added_texture_bytes=0, added_texture_uploads=0, new_draw_aliases=0,
        painting=dict(changed=21, added=0, final_paint_count=256, final_ribbon_count=16, final_layer_count=8),
        skipped_candidates=dict(skips), height_sha256=sha(raw_height), route_sha256=sha(route_file.read_bytes()), grounding_tool_sha256=sha(source_tool.read_bytes()),
        scenery_before_hashes=original_scenery,
        preserved=["all old scenery instance rows", "all apartment rows/assets", "terrain and water", "tree placements/blockers", "quest/traffic/NPC paths", "camp practical light/smoke", "all textures and original meshes", "foliage LOD distances"],
        limitations=["No AAA or performance assertion before native comparison.", "Vegetation uses existing wind/player-reactive shader; no new bending simulation.", "Bounded existing litter paint masks recompose surface detail; no new terrain displacement or wheel-rut geometry."])
    (OUT / "ROW_PATCH.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: report[k] for k in ("added_instances", "species_counts", "route_minimum_actual_mesh_clearance_m", "conservative_peak_75m_near_vertices", "conservative_peak_75m_near_triangles", "painting")}, indent=2))


def reground_final_surface():
    """Y-only correction using the separately audited production CPU sampler.

    Run after main(). The helper compiles the candidate's actual surface field
    and atlas-height implementations; its source and binary are pinned here.
    No runtime code, masks, x/z positions, axes or route clearances change.
    """
    path = OUT / "ROW_PATCH.json"
    report = json.loads(path.read_text())
    assert not (OUT / "FINAL_SURFACE_REGROUND.json").exists()
    helper = N / "review/route-budget/sample-height.so"
    source = helper.with_suffix(".cpp")
    assert helper.is_file() and source.is_file()
    sampler = ctypes.CDLL(str(helper))
    sampler.load.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    sampler.load.restype = ctypes.c_int
    sampler.sample_many.argtypes = [ctypes.c_uint, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_float), ctypes.c_int]
    sampler.sample_many.restype = ctypes.c_int
    sampler.error.restype = ctypes.c_char_p
    height = P / "ground/showcase.height"
    studio = P / "world.studio"
    assert sampler.load(str(height).encode(), str(studio).encode()) == 1, sampler.error()

    def ground(x, z):
        xz = np.ascontiguousarray([[x, z]], dtype=np.float64)
        result = np.empty(1, dtype=np.float32)
        assert sampler.sample_many(1, xz.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                                   result.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), 1) == 1, sampler.error()
        return float(result[0])

    source_tool = ROOT / "docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py"
    spec = importlib.util.spec_from_file_location("camp_road_final_ground", source_tool)
    mesh = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mesh)
    geometry = {a: mesh.geometry(Path(meta["path"])) for a, meta in report["geometry_assets"].items()}
    target = P / "scenery/demo_polish_instances.txt"
    before = target.read_bytes()
    rows = before.decode().splitlines()
    changes = []
    for record in report["instance_changes"]:
        index = record["line"] - 1
        assert rows[index] == record["after"]
        q = rows[index].split()
        pose = read_pose(q)
        prior_y = pose["y"]
        mesh.support(pose, geometry[record["alias"]], ground, record["embed_m"])
        if abs(pose["y"] - prior_y) > .00001:
            updated = q.copy()
            updated[13] = f'{pose["y"]:.9f}'
            text = " ".join(updated)
            assert q[:13] == updated[:13] and q[14:] == updated[14:]
            changes.append(dict(line=index + 1, alias=q[0], before=rows[index], after=text,
                                delta_y_m=float(updated[13]) - prior_y))
            rows[index] = text
            record["after"] = text
            q = updated
        serialized = read_pose(q)
        points = geometry[record["alias"]] * np.array([serialized["sx"], serialized["sy"], serialized["sz"]]) * serialized["scale"]
        points = points @ mesh.rotation(serialized["yaw_deg"], serialized["pitch_deg"], serialized["roll_deg"]).T
        minimum = points[:, 1].min()
        bottom = points[points[:, 1] <= minimum + max(.02, record["height_m"] * .025)]
        bottom = bottom[np.linspace(0, len(bottom) - 1, min(64, len(bottom)), dtype=int)]
        gaps = [serialized["y"] + p[1] - ground(serialized["x"] + p[0], serialized["z"] + p[2]) for p in bottom]
        record["basal_gap_p10_m"] = float(np.quantile(gaps, .1))
        record["basal_gap_p90_m"] = float(np.quantile(gaps, .9))
        assert record["basal_gap_p90_m"] <= (.065 if record["kind"] == "plant" else .12)
    assert target.read_bytes() == before
    after = ("\n".join(rows) + "\n").encode()
    initial = OUT / "ROW_PATCH.initial_raw_ground.json"
    assert not initial.exists()
    initial.write_bytes(path.read_bytes())
    (OUT / "demo_polish_instances.pre-final-ground.before").write_bytes(before)
    target.write_bytes(after)
    for item in report["files"]:
        if item["path"] == "scenery/demo_polish_instances.txt":
            item["after_sha256"] = sha(after)
    receipt = dict(schema="valwood.camp-road-final-surface-ground.v1", passed=True, gpu_used=False,
        method="Existing actual visible-mesh support quantile against raw atlas height plus production terrain_offset; Y-only changes to newly added rows.",
        helper=str(helper), helper_sha256=sha(helper.read_bytes()), helper_source=str(source), helper_source_sha256=sha(source.read_bytes()),
        input_height_sha256=sha(height.read_bytes()), input_world_studio_sha256=sha(studio.read_bytes()),
        scenery_before_sha256=sha(before), scenery_after_sha256=sha(after), changed_instances=len(changes),
        max_absolute_y_correction_m=max(abs(c["delta_y_m"]) for c in changes), changes=changes,
        xz_rotation_scale_wind_geometry_preserved=True,
        worst_final_basal_p90_gap_m=max(r["basal_gap_p90_m"] for r in report["instance_changes"]))
    report["grounding_final_runtime_field"] = {k: v for k, v in receipt.items() if k != "changes"}
    report["tool_sha256"] = sha(Path(__file__).read_bytes())
    path.write_text(json.dumps(report, indent=2) + "\n")
    (OUT / "FINAL_SURFACE_REGROUND.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({k: v for k, v in receipt.items() if k != "changes"}, indent=2))


if __name__ == "__main__":
    if sys.argv[1:] == ["--reground-final-surface"]:
        reground_final_surface()
    else:
        assert not sys.argv[1:]
        main()
