#!/usr/bin/env python3
"""CPU-only acceptance for the authored castle demo and optional native reports.

Does not launch the engine, rebuild assets, or infer visual quality from counts.
The closed demonstration route is checked against committed model bounds and
the actual generated collision boxes before its native walking test.
"""
from collections import Counter
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

from build_starting_area import rectangle_distance, segment_distance

ROOT = Path(__file__).resolve().parents[1]
SCENERY_FILES = ("authored_instances.txt", "starting_area_instances.txt",
                 "rainforest_instances.txt", "castle_village_instances.txt")
ROUTE_ID = "valdoa_castle_district_demo_return"
HERO_ROUTE_ID = "valdoa_market_hero_return"
EXPANSION_ROUTE_ID = "valdoa_castle_training_return"
FARMS_ROUTE_ID = "valdoa_castle_farms_return"
EXPANSION_ALIASES = {"castle_monument", "castle_training_dummy", "castle_archery_target",
                     "castle_weapon_rack", "castle_yard_bench", "castle_training_fence"}
COMPOUND_PIECES = EXPANSION_ALIASES | {"castle_drawbridge"}
WALK_SPEED = 3.5
EXPECTED_POPULATION = {"residents": 128, "knights": 16, "archers": 16, "traders": 20, "travelers": 76}
EXPECTED_PETS = {"pets": 12, "cats": 6, "dogs": 6}
SCENE_CAP = 8192
MODEL_CAP = 4096
TREES = {"pine", "fir", "broadleaf", "pine_young"}
NON_SOLID = {"starting_platform", "starting_bucket", "starting_lantern", "starting_axe",
             "starting_celandine", "starting_wildflower", "fern0", "fern1", "fern2",
             "village_banner", "purchased_prop_flag_a", "purchased_prop_flag_b",
             "purchased_prop_rope", "purchased_prop_lamp"}
NON_SOLID |= {"village_crop_corn", "village_crop_wheat"}
NON_SOLID |= {"village_toolkit_" + name for name in
              ("axe", "chisel", "claw_hammer", "crowbar", "firepoker", "hatchet", "hoe", "machete", "pickaxe", "pitchfork", "scythe", "shovel")}
CIRCLES = {"starting_tower", "starting_barrel", "starting_stump", "starting_firepit"}
VILLAGE_PIECES = {"village_house_a", "village_house_b", "village_inn", "village_smithy",
                  "village_stall_red", "village_stall_blue", "village_stall_gold", "village_keep"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path):
    return json.loads(Path(path).read_text())


def read_records(path):
    result = []
    require(Path(path).stat().st_size <= 256 * 1024, f"Runtime source exceeds256KiB: {path}")
    for line_number, line in enumerate(Path(path).read_text().splitlines(), 1):
        require(len(line) <= 256, f"{path}:{line_number}: runtime line length exceeded")
        body, _, label = line.partition("#")
        fields = body.split()
        if not fields:
            continue
        require(len(fields) in (6, 7), f"{path}:{line_number}: invalid record")
        values = [float(value) for value in fields[1:]]
        require(all(math.isfinite(value) for value in values), f"{path}:{line_number}: nonfinite placement")
        result.append((fields[0], values, label.strip()))
    return result


def runtime_alias_rules(path):
    """Read the fixed renderer alias data without compiling or executing it.

    Keep this conservative parser explicit: an unrecognised initializer is
    rejected instead of guessing what the renderer will accept. Bounds are
    taken from the actual source, so a supported floor placement cannot pass
    this report while still being rejected by the current runtime contract.
    """
    rules = {}
    def source_float(value):
        return struct.unpack("f", struct.pack("f", float(value.removesuffix("f"))))[0]
    for name, _, fields in re.findall(r'\{"([a-z0-9_]+)",\s*"([a-z0-9_]+)",([^}]+)\}', Path(path).read_text()):
        values = [value.strip() for value in fields.split(",")]
        require(len(values) >= 7 and values[0] in ("true", "false"), f"Unknown alias initializer: {name}")
        rules[name] = {"max_burial": source_float(values[1]),
                       "max_elevation": source_float(values[8]) if len(values) > 8 else 0.,
                       "max_scale": 2.5 if name == "broadleaf" else (2. if values[0] == "true" else 4.)}
    require(rules, "Runtime alias bounds are unavailable")
    return rules


def validate_runtime_record(alias, values, rules, label):
    require(alias in rules, f"Unknown runtime model alias: {alias}")
    u, v, yaw, scale, burial = values[:5]
    elevation = values[5] if len(values) == 6 else 0.
    bounds = rules[alias]
    require(0 <= u <= 1 and 0 <= v <= 1 and -360 <= yaw <= 360 and .15 <= scale <= bounds["max_scale"] and
            0 <= burial <= bounds["max_burial"] and 0 <= elevation <= bounds["max_elevation"],
            f"Runtime placement bounds reject {label or alias}: scale={scale}, burial={burial}, elevation={elevation}")


def collision_boxes(path):
    """Parse the generated data include, never execute asset/source code."""
    result = {}
    for alias, numbers in re.findall(r'\{"([a-z0-9_]+)",([^{}]+)\}', Path(path).read_text()):
        values = [float(value.strip().removesuffix("f")) for value in numbers.split(",")]
        require(len(values) == 6 and all(math.isfinite(value) for value in values), "Invalid collision include")
        result.setdefault(alias, []).append(values)
    return result


def frontage_report(metadata, placements, districts, dressing_labels, root=ROOT):
    """Check the frozen frontage recipe, actual instances and house ownership."""
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Frontage metadata is missing")
    require(sha256(Path(root) / metadata["recipe_file"]) == metadata["recipe_sha256"],
            "Frontage recipe changed since generation")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 126 and all(label in placements for label in labels),
            "Frontage instances are missing/duplicated")
    require(set(labels) == {label for label in placements if label.startswith("demo_frontage_")},
            "Frontage metadata omits actual scene instances")
    owners = {label: district["id"] for district in districts if district["id"] != "retained_merchants_lane"
              for label in district["house_labels"]}
    homes = metadata["homes"]
    require(len(homes) == 42 and {home["owner"] for home in homes} == set(owners) and len(owners) == 42,
            "Frontage ownership does not cover all42 new homes")
    assigned = []
    for home in homes:
        require(home["street"] == owners[home["owner"]] and len(home["labels"]) == 3,
                f"Invalid frontage street/group: {home['owner']}")
        require(all(label in placements and placements[label].get("frontage_owner") == home["owner"]
                    for label in home["labels"]), f"Stale frontage owner: {home['owner']}")
        assigned.extend(home["labels"])
    require(len(assigned) == len(set(assigned)) and set(assigned) == set(labels), "Frontage groups do not uniquely own all instances")
    ground = metadata["ground_labels"]
    supports = metadata["surface_supports"]
    supported = [proof["label"] for proof in supports]
    require(len(ground) == len(set(ground)) == 112 and len(supported) == len(set(supported)) == 14 and
            not set(ground).intersection(supported) and set(ground) | set(supported) == set(labels),
            "Frontage ground/support ownership differs")
    for proof in supports:
        samples = proof["actual_surface_samples_m"]
        child = placements[proof["label"]]
        require(proof["parent"] in placements and child.get("surface_parent") == proof["parent"] and
                child["frontage_owner"] == placements[proof["parent"]].get("frontage_owner"),
                f"Frontage support parent differs: {proof['label']}")
        require(len(samples) == 5 and all(math.isfinite(value) for value in samples) and max(samples) - min(samples) < .025,
                f"Frontage real-mesh support proof is invalid: {proof['label']}")
    require(set(metadata["preserved_dressing_labels"]) == set(dressing_labels) and
            not set(labels).intersection(dressing_labels), "Frontages changed the preserved furnishing layer")
    baseline = metadata["existing_records_preserved"]
    require(isinstance(baseline, int) and baseline >= 0 and
            list(placements)[baseline:baseline + len(labels)] == labels and
            metadata["new_model_families"] == metadata["new_texture_policies"] == 0,
            "Frontage baseline or model/image residency differs")
    return {"finished_homes": len(homes), "instances": len(labels), "ground_objects": len(ground),
            "supported_products": len(supports), "real_support_samples": 5 * len(supports),
            "recipe_hash_checked": True, "new_model_families": 0, "new_texture_policies": 0}


def market_report(metadata, placements, models, root=ROOT):
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Detailed market metadata is missing")
    require(sha256(Path(root) / metadata["recipe_file"]) == metadata["recipe_sha256"], "Market recipe changed since generation")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 142 and all(label in placements for label in labels) and
            set(labels) == {label for label in placements if label.startswith("lively_market_")},
            "Market instances are missing/duplicated")
    baseline = metadata["existing_records_preserved"]
    require(list(placements)[baseline:baseline + len(labels)] == labels, "Market append-only baseline differs")
    groups = metadata["groups"]
    owned = [label for group in groups for label in group["labels"]]
    require(len(groups) == len({group["id"] for group in groups}) == 14 and
            len(owned) == len(set(owned)) and set(owned) == set(labels), "Market group ownership differs")
    supports = metadata["surface_supports"]
    supported = [proof["label"] for proof in supports]
    ground = metadata["ground_labels"]
    require(len(supported) == len(set(supported)) == 86 and len(ground) == len(set(ground)) == 56 and
            not set(ground).intersection(supported) and set(ground) | set(supported) == set(labels),
            "Market support ownership differs")
    for proof in supports:
        child = placements[proof["label"]]
        samples = proof["actual_surface_samples_m"]
        require(proof["parent"] in placements and child.get("surface_parent") == proof["parent"] and
                len(samples) == 5 and all(math.isfinite(v) for v in samples) and max(samples) - min(samples) < .025,
                f"Market actual surface support is invalid: {proof['label']}")
    aliases = {placements[label]["alias"] for label in labels}
    require(set(metadata["source_model_sha256"]) == aliases and
            all(metadata["source_model_sha256"][alias] == models[alias]["sha256"] for alias in aliases),
            "Market source model hashes differ")
    require(len(metadata["trader_stations_xz"]) == 10 and len(metadata["pet_stations"]) == len(metadata["pet_routes"]) == 6 and
            metadata["new_model_families"] == metadata["new_texture_policies"] == 0,
            "Market crowd/pet reservations or asset residency differs")
    return {"instances": len(labels), "authored_groups": len(groups), "counters": 8, "covered_stalls": 2,
            "loaded_handcarts": 2, "banners": 4, "supported_goods": len(supports), "real_support_samples": 5 * len(supports),
            "source_hashes_checked": True, "reserved_traders": 10, "reserved_pet_pads": 6, "reserved_pet_loops": 6}


def pets_report(metadata, root=ROOT, *, hash_models=False):
    root = Path(root)
    require(metadata.get("schema_version") == 1 and metadata.get("pet_count") == 12 and
            metadata.get("cats") == metadata.get("dogs") == 6 and metadata.get("pose_count") == 32 and
            metadata.get("blend_count") == 38, "Pet asset inventory differs")
    require(sha256(root / metadata["provenance_file"]) == metadata["provenance_sha256"], "Pet provenance changed since cooking")
    poses = metadata["poses"]
    expected = {f"village_pet_{species}_{mode}_{frame:02d}.glb" for species in ("cat", "dog")
                for mode, count in (("walk", 8), ("idle", 4), ("rest", 4)) for frame in range(count)}
    require(len(poses) == 32 and {pose["file"] for pose in poses} == expected, "Pet pose files are missing/duplicated")
    for pose in poses:
        path = root / "projects/valwood_world/models" / pose["file"]
        require(path.is_file() and path.stat().st_size == pose["bytes"] and 1000 <= pose["triangles"] <= 9000 and
                pose["primitive_count"] == 1, f"Pet pose file or geometry budget differs: {pose['file']}")
        if hash_models:
            require(sha256(path) == pose["sha256"], f"Pet model changed since cooking: {pose['file']}")
    budget = metadata["budget"]
    require(budget["unique_texture_policies"] == 3 and budget["estimated_texture_rgba_mips_mib"] <= 12 and
            budget["geometry_bytes"] < 30_000_000, "Pet image/geometry residency exceeds budget")
    return {"pets": 12, "cats": 6, "dogs": 6, "pose_assets": 32, "blend_batches": 38,
            "provenance_hash_checked": True, "model_hashes_checked": hash_models, **budget}


def generated_route(root, name):
    source = (Path(root) / "engine/world_castle_demo.inc").read_text()
    match = re.search(r'static constexpr double\s+' + re.escape(name) + r'\[\]\[2\]\s*=\s*\{(.*?)\};', source, re.S)
    require(match is not None, f"Generated route is missing: {name}")
    return [[float(x), float(z)] for x, z in re.findall(r'\{\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\}', match.group(1))]


def paving_include_report(squares, source):
    """Match effective paving against generated C data without executing it."""
    match = re.search(r'static const VillageRect\s+village_rects\[\]\s*=\s*\{(.*?)\};', source, re.S)
    require(match is not None, "Generated runtime paving squares are missing")
    rows = []
    for body in re.findall(r'\{([^{}]+)\}', match.group(1)):
        values = [float(value.strip().removesuffix("f")) for value in body.split(",")]
        require(len(values) == 8 and all(math.isfinite(v) for v in values), "Invalid generated runtime paving square")
        rows.append(values)
    require(len(rows) >= len(squares), "Generated runtime paving square count differs")
    for square, actual in zip(squares, rows):
        expected = [*square["center"], *square["half"], 1, 0, square["paving"], 1]
        require(all(abs(a - b) <= 1e-8 * max(1, abs(b)) for a, b in zip(actual, expected)),
                f"Generated runtime paving square differs: {square['id']}")
    require(all(row[6] == 0 for row in rows[len(squares):]), "Generated runtime has an unclaimed paving square")
    return len(squares)


def hero_apron_report(metadata, placements, models, roads, squares, layout_path):
    """Independently measure both source canopies and their rendered aprons."""
    aprons = metadata.get("ground_aprons", [])
    owners = {"hero_market_baker_apron": "hero_market_baker_canopy",
              "hero_market_dairy_apron": "hero_market_dairy_canopy"}
    require(len(aprons) == 2 and {row["id"] for row in aprons} == set(owners), "Hero measured aprons are missing/duplicated")
    by_id = {row["id"]: row for row in squares}
    require(len(by_id) == len(squares) and {key for key in by_id if key.startswith("hero_market_")} == set(owners),
            "Hero effective apron squares are missing/duplicated")
    cross = [road for road in roads if road["id"] == "cross_55"]
    require(len(cross) == 1 and cross[0]["a"][1] == cross[0]["b"][1] == 55, "Hero apron road connection differs")
    road = cross[0]
    minimum_overlap = math.inf
    for apron in aprons:
        require(apron["parent"] == owners[apron["id"]] and apron["parent"] in placements, "Hero apron parent differs")
        parent = placements[apron["parent"]]
        require(parent["alias"] == "purchased_shop_d" and parent["scale"] == 1, "Hero apron full-size source canopy differs")
        low, high = models[parent["alias"]]["bounds_y_up"]
        c, s, scale = math.cos(parent["yaw"]), math.sin(parent["yaw"]), parent["scale"]
        points = [(parent["x"] + (c * x + s * z) * scale, parent["z"] + (-s * x + c * z) * scale)
                  for x in (low[0], high[0]) for z in (low[2], high[2])]
        measured = [min(p[0] for p in points), min(p[1] for p in points), max(p[0] for p in points), max(p[1] for p in points)]
        expected = [measured[0] - .10, measured[1] - .10, measured[2] + .10, measured[3] + .90]
        require(apron["side_extension_m"] == apron["back_extension_m"] == .10 and apron["street_extension_m"] == .90,
                "Hero measured apron margins differ")
        for key, target in (("source_world_bounds_xz", measured), ("bounds_xz", expected)):
            require(len(apron[key]) == 4 and all(math.isfinite(a) and abs(a - b) < 2e-6 for a, b in zip(apron[key], target)),
                    f"Hero measured apron source bounds differ: {apron['id']}")
        square = by_id[apron["id"]]
        actual = [square["center"][0] - square["half"][0], square["center"][1] - square["half"][1],
                  square["center"][0] + square["half"][0], square["center"][1] + square["half"][1]]
        require(all(math.isfinite(a) and abs(a - b) < 2e-6 for a, b in zip(actual, expected)),
                f"Hero effective paving square does not cover measured apron: {apron['id']}")
        require(square["paving"] == apron["paving"] == .83, "Hero apron paving material differs")
        overlap = expected[3] - (road["a"][1] - road["half_width"])
        require(overlap > .40 and min(road["a"][0], road["b"][0]) < expected[0] < expected[2] < max(road["a"][0], road["b"][0]),
                "Hero measured apron fails to overlap existing road")
        minimum_overlap = min(minimum_overlap, overlap)
    runtime_squares = paving_include_report(squares, Path(layout_path).read_text())
    return {"measured_aprons": 2, "minimum_road_overlap_m": minimum_overlap, "runtime_paving_squares_verified": runtime_squares}


def hero_report(metadata, grove, placements, models, pieces, root=ROOT, *, roads, squares):
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Hero street metadata is missing")
    require(sha256(Path(root) / metadata["recipe_file"]) == metadata["recipe_sha256"], "Hero street recipe changed")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 135 and set(labels) == {label for label in placements if label.startswith("hero_market_")},
            "Hero street instances are missing/duplicated")
    start = metadata["existing_records_preserved"]
    require(list(placements)[start:start + len(labels)] == labels, "Hero street append-only baseline differs")
    lines, groups = metadata["cloth_lines"], metadata["groups"]
    cloth = [label for line in lines for label in line["rope_labels"] + line["flag_labels"]]
    grouped = [label for group in groups for label in group["labels"]]
    require(len(lines) == 3 and len(groups) == 7 and len(cloth + grouped) == len(set(cloth + grouped)) == len(labels) and
            set(cloth + grouped) == set(labels), "Hero street composition ownership differs")
    supports = metadata["surface_supports"]
    ground = metadata["ground_labels"]
    require(len(supports) == 26 and len(ground) == 20 and
            set(ground) | {p["label"] for p in supports} | set(cloth) == set(labels), "Hero street support inventory differs")
    for proof in supports:
        samples = proof["actual_surface_samples_m"]
        require(placements[proof["label"]].get("surface_parent") == proof["parent"] and proof["parent"] in placements and
                len(samples) == 5 and all(math.isfinite(v) for v in samples) and max(samples) - min(samples) < .025,
                f"Hero source-surface support differs: {proof['label']}")
    aliases = {placements[label]["alias"] for label in labels}
    require(set(metadata["source_model_sha256"]) == aliases and
            all(metadata["source_model_sha256"][alias] == models[alias]["sha256"] for alias in aliases), "Hero source model hashes differ")
    canopies = [placements[r["label"]] for r in metadata["source_envelopes"]]
    require(len(canopies) == 2 and all(r["alias"] == "purchased_shop_d" and r["scale"] == 1 for r in canopies),
            "Full-size source infill canopies are missing")
    aprons = hero_apron_report(metadata, placements, models, roads, squares, Path(root) / "engine/world_village_layout.inc")
    path = metadata["walking_route_xz"]
    require(path == generated_route(root, "castle_market_hero_route") and len(path) == 3 and path[0] == path[-1],
            "Hero runtime walking route differs")
    clearance = route_clearance(list(placements.values()), models, path, pieces)
    length = sum(math.dist(a, b) for a, b in zip(path, path[1:]))
    require(abs(length - 88) < .001 and metadata["clear_walking_width_m"] == 3, "Hero walking route/width differs")
    require({v["id"] for v in metadata["views"]} == {"castle-hero-entry", "castle-hero-mid", "castle-hero-reverse"}, "Hero inspection views are missing")
    require(isinstance(grove, dict) and sha256(Path(root) / grove["recipe_file"]) == grove["recipe_sha256"], "Hero grove recipe changed")
    grove_labels = grove["labels"]
    require(len(grove_labels) == len(set(grove_labels)) == 36 and all(label in placements for label in grove_labels) and
            sum(placements[label]["alias"] == "broadleaf" for label in grove_labels) == 12 and
            grove["global_density_changed"] is False and grove["new_texture_policies"] == 0, "Hero grove budget differs")
    return {"instances": len(labels), "full_size_canopies": 2, "cloth_lines": 3, "supported_goods_and_stacks": len(supports),
            "real_support_samples": 5 * len(supports), "hooks": 6, "workstations": 4,
            "grove_trees": 12, "grove_understory": 24, **aprons,
            "route": {"id": HERO_ROUTE_ID, "local_xz": path, "length_m": length,
                      "walking_seconds": length / WALK_SPEED, "frames_with_warmup": math.ceil(length / WALK_SPEED * 60) + 300, **clearance}}


def route_clearance(placements, models, route, pieces, radius=.37):
    """Conservative full-segment capsule sweeps in each model's rotated frame.

    Named village pieces use the same collision boxes as runtime, including
    gate openings. All other solids use runtime bounding boxes/circles. This
    is a geometric check, not a substitute for the native terrain/motion test.
    """
    minimum, nearest, checks = math.inf, None, 0
    for item in placements:
        alias = item["alias"]
        if alias in NON_SOLID or alias.startswith("purchased_goods_"):
            continue
        low, high = models[alias]["bounds_y_up"]
        scale, yaw = item["scale"], item["yaw"]
        base = item.get("elevation", 0) - item["burial"]
        # All demo walk stations are on the 24m terrace. Ignore only solids
        # demonstrably above the full standing capsule, never low cargo.
        if base + low[1] * scale > 1.85 or base + high[1] * scale < .05:
            continue
        cosine, sine = math.cos(yaw), math.sin(yaw)
        def local(point):
            dx, dz = point[0] - item["x"], point[1] - item["z"]
            return [cosine * dx - sine * dz, sine * dx + cosine * dz]
        boxes = pieces.get(alias) if alias in VILLAGE_PIECES | COMPOUND_PIECES else None
        if alias == "starting_gate":
            boxes = [[low[0], -3.50133 / 2, low[2], high[2], 0, high[1]],
                     [3.50133 / 2, high[0], low[2], high[2], 0, high[1]],
                     [-3.50133 / 2, 3.50133 / 2, low[2], high[2], 4.2, high[1]]]
        if alias in VILLAGE_PIECES | COMPOUND_PIECES:
            require(bool(boxes), f"Missing actual collision pieces: {alias}")
        for start, finish in zip(route, route[1:]):
            a, b = local(start), local(finish)
            if alias in CIRCLES or alias in TREES:
                trunk_radius = {"pine": .38, "fir": .42, "broadleaf": .20, "pine_young": .08}
                extent = trunk_radius.get(alias, max(abs(low[0]), abs(high[0]), abs(low[2]), abs(high[2])))
                margin = segment_distance([0, 0], a, b) - extent * scale - radius
                checks += 1
            else:
                candidates = boxes or [[low[0], high[0], low[2], high[2], low[1], high[1]]]
                margins = []
                for xmin, xmax, zmin, zmax, ymin, ymax in candidates:
                    if base + ymin * scale > 1.85 or base + ymax * scale < .05:
                        continue
                    margins.append(rectangle_distance(a, b, [xmin * scale, zmin * scale],
                                                      [xmax * scale, zmax * scale]) - radius)
                    checks += 1
                if not margins:
                    continue
                margin = min(margins)
            if margin < minimum:
                minimum, nearest = margin, item["label"]
    require(checks > 0 and math.isfinite(minimum), "No route geometry was checked")
    require(minimum > .15, f"Demo route obstructed by {nearest}: margin {minimum:.6f}m")
    return {"solid_segment_checks": checks, "minimum_capsule_margin_m": minimum,
            "nearest_solid": nearest, "capsule_radius_m": radius}


def same_numeric_rows(actual, expected):
    return len(actual) == len(expected) and all(len(a) == len(b) and all(
        math.isfinite(x) and abs(x - y) <= 1e-8 * max(1, abs(y)) for x, y in zip(a, b))
        for a, b in zip(actual, expected))


def drawbridge_collision_boxes(path):
    source = Path(path).read_text()
    match = re.search(r'castle_drawbridge_collision_boxes\[\]\[6\]\s*=\s*\{(.*?)\};', source, re.S)
    require(match is not None, "Drawbridge compound collision source is missing")
    result = [[float(value.strip().removesuffix("f")) for value in body.split(",")]
              for body in re.findall(r'\{([^{}]+)\}', match.group(1))]
    require(len(result) == 2 and all(len(box) == 6 and all(math.isfinite(v) for v in box) for box in result),
            "Drawbridge compound collision source is invalid")
    return result


def training_report(root=ROOT):
    root = Path(root)
    path = root / "projects/valwood_world/models/castle_training_manifest.json"
    data = load_json(path)
    require(data.get("schema_version") == 1 and data.get("pose_count") == 24 and
            data.get("instances") == 7 and data.get("trainees") == 6 and data.get("instructors") == 1 and
            data.get("source_samples") == 20 and data.get("authored_recovery_samples") == 4,
            "Castle training pose/actor inventory differs")
    require(data.get("source_action") == "Attack-two handed2" and data.get("source_frames") == [33., 113.] and
            data.get("source_scripts_executed") is False and data.get("source_file_modified") is False and
            sha256(root / data["source"]) == data["source_sha256"], "Castle training source/action differs")
    require(data.get("new_image_texture_policies") == 0 and data.get("unique_images") == 6 and
            data.get("all_adjacent_pose_topology_material_exact") is True and
            0 < data.get("geometry_bytes", 0) < 16 * 1048576, "Castle training residency/interpolation budget differs")
    assets = data["assets"]
    require(len(assets) == 24 and [row["file"] for row in assets] ==
            [f"castle_training_knight_drill_{i:02}.glb" for i in range(24)], "Castle training pose inventory is duplicated")
    for row in assets:
        model = root / "projects/valwood_world/models" / row["file"]
        require(model.stat().st_size == row["bytes"] and sha256(model) == row["sha256"], "Castle training actual pose changed")
    header = (root / "engine/world_castle_training.h").read_text()
    for key, count in (("actor_count", 7), ("pose_count", 28), ("blend_count", 28), ("drill_pose_count", 24)):
        require(re.search(r'world_castle_training_' + key + r'\s*=\s*' + str(count) + r'\s*;', header),
                "Castle training CPU/runtime counts differ")
    block = header.split("world_castle_training_stations[]={", 1)[1].split("};", 1)[0]
    stations = []
    for body in re.findall(r'\{([^{}]+)\}', block):
        fields = [value.strip() for value in body.split(",")]
        require(len(fields) == 6, "Castle training runtime station initializer differs")
        stations.append({"position_xz": [float(fields[0]), float(fields[1])],
                         "yaw": float(fields[2].removesuffix("f")), "cycle_seconds": float(fields[3]),
                         "instructor": fields[5] == "true"})
    expected = {(-47., -73.), (-51., -73.), (-47., -84.), (-51., -84.), (-55., -72.), (-55., -83.), (-42.5, -68.)}
    require(len(stations) == 7 and {tuple(row["position_xz"]) for row in stations} == expected and
            sum(row["instructor"] for row in stations) == 1, "Castle training reserved runtime stations differ")
    return {"actors": 7, "trainees": 6, "instructors": 1, "drill_poses": 24,
            "resident_idle_poses": 4, "pose_assets": 28, "blend_batches": 28,
            "geometry_bytes": data["geometry_bytes"], "new_texture_policies": 0,
            "source_sha256": data["source_sha256"], "manifest_sha256": sha256(path),
            "stations": stations, "pose_hashes_checked": True}


def expansion_report(metadata, creek, placements, models, pieces, root=ROOT, *, roads, squares, views):
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Castle expansion metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata["recipe_sha256"], "Castle expansion recipe changed")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 63 and set(labels) ==
            {label for label in placements if label.startswith("castle_expansion_")}, "Castle expansion instances are missing/duplicated")
    start = metadata["existing_records_preserved"]
    require(start == 2321 and list(placements)[start:start + 63] == labels, "Castle expansion append-only baseline differs")
    baseline = json.dumps(list(placements.values())[:start], sort_keys=True, separators=(",", ":")).encode()
    require(hashlib.sha256(baseline).hexdigest() == metadata.get("baseline_records_sha256"),
            "Castle expansion canonical existing prefix differs")
    require(set(metadata["models"]) == EXPANSION_ALIASES and metadata["new_model_families"] == 6 and
            metadata["new_gpu_image_bytes"] == 0, "Castle expansion model/image budget differs")
    inventory_path = root / metadata["asset_manifest"]
    inventory = load_json(inventory_path)
    require(inventory["recipe_sha256"] == sha256(root / "tools/build_castle_expansion_assets.py") and
            inventory["new_gpu_image_bytes"] == inventory["image_policy_delta"] == 0,
            "Castle expansion asset recipe/image policy differs")
    for path, digest in inventory["source_files_sha256"].items():
        require(sha256(root / path) == digest, "Castle expansion resident source changed")
    require({row["asset"] for row in inventory["assets"]} == EXPANSION_ALIASES and len(inventory["assets"]) == 6,
            "Castle expansion asset families differ")
    for row in inventory["assets"]:
        alias = row["asset"]
        require(alias in models and metadata["models"][alias] == alias and models[alias]["file"] == alias + ".glb" and
                models[alias]["sha256"] == row["sha256"] == sha256(root / "projects/valwood_world/models" / models[alias]["file"]) and
                models[alias]["bounds_y_up"] == row["bounds"], "Castle expansion actual model/source bounds changed")
        require(same_numeric_rows(pieces.get(alias, []), row["collision_boxes"]), "Castle expansion source collision bounds differ")
    statue = placements.get(metadata["statue_label"], {})
    require(statue.get("alias") == "castle_monument" and statue.get("x") == 47 and statue.get("z") == -78 and
            statue.get("scale") == 1 and abs(statue.get("yaw", 0) + math.pi / 2) < 1e-8 and
            models["castle_monument"]["bounds_y_up"][1][1] > 6.9, "Castle Defender monument is missing or reduced")
    by_square = {row["id"]: row for row in squares}
    ground = metadata["ground_squares"]
    require(len(ground) == 3 and {row["id"] for row in ground} ==
            {"castle_training_court", "castle_defender_court", "castle_armory_terrace"} and
            all(by_square.get(row["id"]) == row for row in ground), "Castle expansion effective paving differs")
    by_road = {row["id"]: row for row in roads}
    authored_roads = metadata["ground_roads"]
    require(len(authored_roads) == 3 and {row["id"] for row in authored_roads} ==
            {"castle_training_walk", "castle_monument_north_walk", "castle_monument_walk"} and
            all(by_road.get(row["id"]) == row for row in authored_roads), "Castle expansion effective pedestrian roads differ")
    actual_views = {row["id"]: row for row in views}
    require(len(metadata["views"]) == 3 and {row["id"] for row in metadata["views"]} ==
            {"castle-training", "castle-statue", "castle-armory"} and
            all(actual_views.get(row["id"]) == row for row in metadata["views"]), "Castle expansion inspection views differ")
    require(isinstance(creek, dict) and creek.get("schema_version") == 1 and
            sha256(root / creek["recipe_file"]) == creek["recipe_sha256"], "Castle creek recipe changed")
    for file_key, hash_key in (("terrain_helper_file", "terrain_helper_sha256"),
                               ("provenance_file", "provenance_sha256"), ("collision_file", "collision_sha256")):
        require(file_key in creek and hash_key in creek and sha256(root / creek[file_key]) == creek[hash_key],
                "Castle creek helper/provenance/collision source changed")
    bridge = placements.get("castle_creek_drawbridge", {})
    require(creek["labels"] == ["castle_creek_drawbridge"] and creek["existing_records_preserved"] == start + 63 and
            list(placements)[start + 63:start + 64] == ["castle_creek_drawbridge"] and bridge.get("alias") == "castle_drawbridge" and
            bridge.get("x") == 0 and bridge.get("z") == 225 and bridge.get("yaw") == 0 and bridge.get("scale") == 1 and
            bridge.get("burial") == .7 and bridge.get("elevation") == 0, "Castle drawbridge placement/prefix differs")
    provenance = load_json(root / creek["provenance_file"])
    require(provenance["new_gpu_image_keys"] == 0 and provenance["bed_separate_from_walk_floor"] is True and
            provenance["deck_height_m"] == creek["deck_surface_y"] == 24.02 and creek["water_surface_y"] == 22.9 and
            provenance["alias"]["collision"] == "Drawbridge" and len(provenance["assets"]) == 2,
            "Castle drawbridge water/floor/collision policy differs")
    for row in provenance["assets"] + provenance["source_materials"]:
        require(sha256(root / row["path"]) == row["sha256"], "Castle drawbridge model/material source changed")
    require(models["castle_drawbridge"]["sha256"] == provenance["assets"][0]["sha256"] and
            same_numeric_rows(pieces.get("castle_drawbridge", []), provenance["collision_boxes"]), "Castle drawbridge runtime collision/model differs")
    path = metadata["route_xz"]
    require(isinstance(path, list) and len(path) >= 10 and path[0] == path[-1] and
            all(len(point) == 2 and all(math.isfinite(v) for v in point) for point in path) and
            all(math.dist(a, b) > .001 for a, b in zip(path, path[1:])), "Castle expansion walking route is invalid")
    require(path == generated_route(root, "castle_expansion_route"), "Castle expansion generated runtime route differs")
    route_proof = route_clearance(list(placements.values()), models, path, pieces)
    length = sum(math.dist(a, b) for a, b in zip(path, path[1:]))
    require(800 < length < 900 and path[0] == [0, 225], "Castle expansion exterior walking route length differs")
    require(creek.get("outside_outer_town_walls") is True, "Creek must surround the exterior town wall")
    for position in ([0, 219], [-36, -66], [-33, -112], [37, -69]):
        require(min(segment_distance(position, a, b) for a, b in zip(path, path[1:])) < .01,
                "Castle expansion route skips bridge/training/armory/monument")
    paved = 0
    for a, b in zip(path, path[1:]):
        steps = math.ceil(math.dist(a, b) / .5)
        for i in range(steps + 1):
            point = [a[k] + (b[k] - a[k]) * i / steps for k in (0, 1)]
            require(any(segment_distance(point, road["a"], road["b"]) + .37 < road["half_width"] for road in roads) or
                    any(all(abs(point[k] - square["center"][k]) + .37 < square["half"][k] for k in (0, 1)) for square in squares),
                    f"Castle expansion route leaves authored paving: {point}")
            paved += 1
    return {"instances": 63, "existing_records_preserved": start, "canonical_prefix_hash_checked": True,
            "model_hashes_checked": True, "new_model_families": 6, "new_gpu_image_bytes": 0,
            "statue_height_m": models["castle_monument"]["bounds_y_up"][1][1], "paving_regions": 3,
            "inspection_views": 3, "pedestrian_roads": 3, "asset_manifest_sha256": sha256(inventory_path),
            "route": {"id": EXPANSION_ROUTE_ID, "local_xz": path, "length_m": length,
                      "walking_seconds": length / WALK_SPEED, "frames_with_warmup": math.ceil(length / WALK_SPEED * 60) + 300,
                      "paved_stations_checked": paved, **route_proof},
            "creek": {"bridge_instances": 1, "model_lods": 2, "compound_collision_boxes": 2,
                      "helper_hash_checked": True, "provenance_hash_checked": True,
                      "water_surface_y": 22.9, "bridge_deck_y": 24.02}, "training": training_report(root)}


def canonical_prefix(metadata, placements, expected_start, title):
    start = metadata.get("existing_records_preserved")
    require(start == expected_start, f"{title} append-only baseline differs")
    encoded = json.dumps(list(placements.values())[:start], sort_keys=True, separators=(",", ":")).encode()
    require(hashlib.sha256(encoded).hexdigest() == metadata.get("baseline_records_sha256"),
            f"{title} canonical existing prefix differs")
    return start


def canonical_transform(actual, expected, title):
    require(actual.get("alias") == expected.get("alias") and actual.get("label") == expected.get("label"),
            f"{title} canonical model/label differs")
    for key in ("x", "z", "yaw", "scale", "burial", "elevation"):
        value, target = actual.get(key, 0), expected.get(key, 0)
        require(isinstance(value, (int, float)) and math.isfinite(value) and abs(value - target) < 1e-7,
                f"{title} canonical placement differs: {actual.get('label')} {key}")
    for key in ("surface_parent", "bank_pocket"):
        require(actual.get(key) == expected.get(key), f"{title} source ownership differs: {actual.get('label')}")


def road_include_report(roads, source):
    match = re.search(r'static const VillageRoad\s+village_roads\[\]\s*=\s*\{(.*?)\};', source, re.S)
    require(match is not None, "Generated runtime pedestrian roads are missing")
    actual = [[float(v.strip().removesuffix("f")) for v in body.split(",")]
              for body in re.findall(r'\{([^{}]+)\}', match.group(1))]
    expected = [[*r["a"], *r["b"], r["half_width"], r["paving"]] for r in roads]
    require(same_numeric_rows(actual, expected), "Generated runtime pedestrian roads differ")
    return len(roads)


def source_surface_height(triangles, x, z, minimum, maximum):
    hits = []
    for a, b, c in triangles:
        ux, uz, vx, vz = b[0] - a[0], b[2] - a[2], c[0] - a[0], c[2] - a[2]
        denominator = ux * vz - uz * vx
        if abs(denominator) < 1e-10:
            continue
        dx, dz = x - a[0], z - a[2]
        u, v = (dx * vz - dz * vx) / denominator, (ux * dz - uz * dx) / denominator
        if u >= -.0001 and v >= -.0001 and u + v <= 1.0001:
            height = a[1] + u * (b[1] - a[1]) + v * (c[1] - a[1])
            if minimum <= height <= maximum:
                hits.append(height)
    require(hits, "Castle wing supported supply lacks actual source triangle contact")
    return max(hits)


def wing_report(metadata, placements, models, pieces, root=ROOT, *, roads):
    """Verify source-resident wings, real supply contacts and published paths."""
    from build_castle_inner_wing import specs, SITES, ROADS, ASSET_ALIASES
    from build_starting_area import glb_bounds
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Castle inner wing metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata.get("recipe_sha256"), "Castle inner wing recipe changed")
    start = canonical_prefix(metadata, placements, 2385, "Castle inner wing")
    ground = specs()
    supported_labels = ["castle_inner_" + site["id"] + "_supported_tankard" for site in SITES]
    labels = [r["label"] for r in ground] + supported_labels
    require(metadata.get("labels") == labels and len(labels) == 16 and list(placements)[start:start + 16] == labels and
            set(labels) == {label for label in placements if label.startswith("castle_inner_")},
            "Castle inner wing instances are missing/duplicated/out of order")
    require(metadata.get("semantic_aliases") == ASSET_ALIASES and metadata.get("structures") == SITES and
            set(metadata.get("source_model_sha256", {})) == set(ASSET_ALIASES) and
            metadata.get("full_scale_structures") == 2 and metadata.get("new_mesh_bytes") == 0 and
            metadata.get("new_image_texture_policies") == 0 and metadata.get("residential_home_count_unchanged") is True,
            "Castle inner wing source residency/structure budget differs")
    for expected in ground:
        canonical_transform(placements[expected["label"]], expected, "Castle inner wing")
    for site in SITES:
        alias, donor = site["alias"], site["source_alias"]
        require(alias in models and donor in models and models[alias]["file"] == ASSET_ALIASES[alias] + ".glb" and
                models[alias]["sha256"] == metadata["source_model_sha256"].get(alias) == models[donor]["sha256"] and
                sha256(root / "projects/valwood_world/models" / models[alias]["file"]) == models[alias]["sha256"] and
                models[alias]["bounds_y_up"] == models[donor]["bounds_y_up"], "Castle inner wing resident model changed")
        low, high = glb_bounds(root / "projects/valwood_world/models" / models[alias]["file"])
        require(same_numeric_rows([low, high], models[alias]["bounds_y_up"]), "Castle inner wing actual source bounds differ")
    require(metadata.get("ground_roads") == ROADS and all(road in roads for road in ROADS),
            "Castle inner wing effective entry roads differ")
    runtime_roads = road_include_report(roads, (root / "engine/world_village_layout.inc").read_text())
    supports = metadata.get("surface_supports", [])
    require(len(supports) == 2 and [p["label"] for p in supports] == supported_labels,
            "Castle inner wing supported supplies are missing/duplicated")
    for site, proof in zip(SITES, supports):
        parent_label = "castle_inner_" + site["id"] + "_cargo"
        parent, child = placements[parent_label], placements[proof["label"]]
        require(proof.get("parent") == parent_label and proof.get("source_local_xz") == [0, 0] and
                proof.get("contact_slack_m") == .002, "Castle inner wing support ownership differs")
        low, high = models["purchased_goods_tankard"]["bounds_y_up"]
        footprint = [[0., 0.]] + [[x * .90 / parent["scale"], z * .90 / parent["scale"]]
                                   for x in (low[0], high[0]) for z in (low[2], high[2])]
        triangles = glb_bounds(root / "projects/valwood_world/models" / models[parent["alias"]]["file"], True)[3]
        heights = [source_surface_height(triangles, x, z, .92, 1.01) for x, z in footprint]
        require(same_numeric_rows(footprint, proof.get("actual_footprint_xz", [])) and
                same_numeric_rows([heights], [proof.get("actual_surface_samples_m", [])]) and max(heights) - min(heights) < .025,
                "Castle inner wing actual five-ray source support differs")
        elevation = max(heights) * parent["scale"] + .002
        expected = dict(alias="purchased_goods_tankard", label=proof["label"], x=parent["x"], z=parent["z"],
                        yaw=parent["yaw"], scale=.90, burial=0, elevation=elevation, surface_parent=parent_label)
        canonical_transform(child, expected, "Castle inner wing")
        require(abs(proof.get("ground_elevation_m", -1) - elevation) < 1e-7, "Castle inner wing support elevation differs")
    paths = [dict(id=s["id"], **route_clearance(list(placements.values()), models, s["entry_path_xz"], pieces)) for s in SITES]
    return {"instances": 16, "full_scale_structures": 2, "supported_supply_items": 2, "real_source_support_rays": 10,
            "existing_records_preserved": start, "canonical_prefix_hash_checked": True,
            "canonical_transforms_checked": True, "resident_model_hashes_checked": True,
            "paved_entry_roads": 2, "runtime_roads_verified": runtime_roads, "entry_paths": paths,
            "new_mesh_bytes": 0, "new_image_texture_policies": 0}


def creek_terrain_reader(path):
    """Independent scalar interpretation of the frozen production bed data.

    No compiler or atlas initialization is needed for this bounded flat-terrace
    patch. The integration test separately binds this equation to real meshes.
    """
    source = Path(path).read_text()
    block = source.split("world_castle_creek_points[]={", 1)[1].split("};", 1)[0]
    points = [[float(v.strip()) for v in body.split(",")] for body in re.findall(r'\{([^{}]+)\}', block)]
    require(len(points) == 22 and points[0] == points[-1] and all(len(p) == 3 for p in points) and
            "out.height_delta=-2.45*weight" in source and "fabs(x)<=205.&&fabs(z)<=205." in source and
            "world_castle_creek_smooth(10.6,11.8" in source,
            "Castle bank shared terrain profile differs")
    def smooth(a, b, x):
        t = max(0., min(1., (x - a) / (b - a)))
        return t * t * (3 - 2 * t)
    def terrain(x, z):
        if not (-225 < x < 225 and -223 < z < 223) or (abs(x) <= 205 and abs(z) <= 205):
            return 24.
        q = 2.
        for a, b in zip(points, points[1:]):
            dx, dz = b[0] - a[0], b[1] - a[1]
            t = max(0., min(1., ((x - a[0]) * dx + (z - a[1]) * dz) / (dx * dx + dz * dz)))
            width = a[2] + (b[2] - a[2]) * t
            q = min(q, math.hypot(x - a[0] - dx * t, z - a[1] - dz * t) / width)
        if q >= 1:
            return 24.
        protect = smooth(10.6, 11.8, math.hypot(abs(x) - 205, abs(z) - 205))
        return struct.unpack("f", struct.pack("f", 24 - 2.45 * (1 - smooth(.14, 1., q)) * protect))[0]
    return terrain


def model_box(item, model, base=None):
    low, high = model["bounds_y_up"]
    c, s, scale = math.cos(item["yaw"]), math.sin(item["yaw"]), item["scale"]
    lx, lz = (low[0] + high[0]) * .5 * scale, (low[2] + high[2]) * .5 * scale
    x, z = item["x"] + c * lx + s * lz, item["z"] - s * lx + c * lz
    hx, hz = (high[0] - low[0]) * .5 * scale, (high[2] - low[2]) * .5 * scale
    base = item.get("elevation", 0) - item["burial"] if base is None else base
    ax, az = abs(c) * hx + abs(s) * hz, abs(s) * hx + abs(c) * hz
    return (x, z, hx, hz, c, s, base + low[1] * scale, base + high[1] * scale, x - ax, x + ax, z - az, z + az)


def boxes_overlap(a, b, margin=.025):
    if a[7] <= b[6] + .001 or b[7] <= a[6] + .001 or a[9] + margin < b[8] or b[9] + margin < a[8] or a[11] + margin < b[10] or b[11] + margin < a[10]:
        return False
    for x, z in ((a[4], -a[5]), (a[5], a[4]), (b[4], -b[5]), (b[5], b[4])):
        radius = a[2] * abs(a[4] * x - a[5] * z) + a[3] * abs(a[5] * x + a[4] * z)
        radius += b[2] * abs(b[4] * x - b[5] * z) + b[3] * abs(b[5] * x + b[4] * z)
        if abs((a[0] - b[0]) * x + (a[1] - b[1]) * z) > radius + margin:
            return False
    return True


def bank_source_support(item, model, vertices, terrain):
    low, high = model["bounds_y_up"]
    c, s, scale = math.cos(item["yaw"]), math.sin(item["yaw"]), item["scale"]
    root = terrain(item["x"], item["z"])
    if root < 22.895:
        return None
    fern = item["alias"].startswith("fern")
    samples = []
    if not fern:
        for iz in range(5):
            for ix in range(5):
                x, z = (low[0] + ix / 4 * (high[0] - low[0])) * scale, (low[2] + iz / 4 * (high[2] - low[2])) * scale
                samples.append(terrain(item["x"] + c * x + s * z, item["z"] - s * x + c * z))
    base = min([root, *samples]) - item["burial"] - (0 if fern else .05) + item["elevation"]
    gaps, contacts, exposed = [], [], 0
    for p in vertices:
        x, z = item["x"] + (c * p[0] + s * p[2]) * scale, item["z"] + (-s * p[0] + c * p[2]) * scale
        gap = base + p[1] * scale - terrain(x, z)
        gaps.append(gap)
        if p[1] * scale < .08:
            contacts.append(gap)
        if gap > .10 and base + p[1] * scale > 22.95:
            exposed += 1
    minimum, maximum = min(gaps), max(gaps)
    if fern:
        if not contacts or min(contacts) < -.065 or min(contacts) > .025 or minimum < -.14:
            return None
    elif minimum > .015 or minimum < -max(.18, high[1] * scale * .78) or exposed < 10 or maximum < .16:
        return None
    return {"runtime_base_y": base, "root_terrain_y": root, "actual_source_vertex_rays": len(gaps),
            "terrain_support_grid_samples": len(samples), "minimum_source_to_terrain_gap_m": minimum,
            "maximum_exposed_height_m": maximum, "exposed_source_vertices": exposed}


def banks_report(metadata, placements, models, root=ROOT, *, roads):
    from build_castle_creek_banks import specs, POCKETS, ASSET_ALIASES
    from build_starting_area import glb_bounds
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Castle creek banks metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata.get("recipe_sha256"), "Castle creek banks recipe changed")
    require(sha256(root / metadata["terrain_helper_file"]) == metadata.get("terrain_helper_sha256"), "Castle creek bank terrain helper changed")
    start = canonical_prefix(metadata, placements, 2401, "Castle creek banks")
    nominal = specs();labels = [r["label"] for r in nominal]
    require(metadata.get("labels") == labels and list(placements)[start:start + 36] == labels and
            len(labels) == len(set(labels)) == 36 and set(labels) == {label for label in placements if label.startswith("castle_creek_bank_")},
            "Castle creek bank instances are missing/duplicated/out of order")
    require(metadata.get("pockets") == POCKETS and metadata.get("rocks") == 24 and metadata.get("ferns") == 10 and
            metadata.get("deadwood") == 2 and metadata.get("new_model_families") == metadata.get("new_gpu_image_keys") == 0 and
            metadata.get("global_density_changed") is False, "Castle creek bank inventory/residency differs")
    require(set(metadata.get("source_models", {})) == set(ASSET_ALIASES), "Castle creek bank source model inventory differs")
    resident = {alias for name in SCENERY_FILES[:3] for alias, _, _ in read_records(root / "projects/valwood_world/scenery" / name)}
    require(set(ASSET_ALIASES) <= resident, "Castle creek banks introduce a nonresident model family")
    vertices = {}
    for alias, stem in ASSET_ALIASES.items():
        source = metadata["source_models"][alias]
        path = root / "projects/valwood_world/models" / (stem + ".glb")
        require(source.get("file") == str(path.relative_to(root)) and sha256(path) == source.get("sha256") == models[alias]["sha256"] and
                models[alias]["file"] == stem + ".glb", "Castle creek bank resident source model changed")
        low, high, points, _ = glb_bounds(path, True)
        require(same_numeric_rows([low, high], models[alias]["bounds_y_up"]), "Castle creek bank source bounds differ")
        vertices[alias] = points
    supports = metadata.get("supports", []);declared = metadata.get("placements", [])
    require(len(supports) == len(declared) == 36 and [p.get("label") for p in supports] == labels and
            [p.get("label") for p in declared] == labels, "Castle creek bank support ownership differs")
    terrain = creek_terrain_reader(root / metadata["terrain_helper_file"])
    obstacles = [model_box(r, models[r["alias"]]) for r in list(placements.values())[:start] if r["alias"] != "starting_gate"]
    rays = 0;grids = 0
    shifts = [(0, 0), (.25, 0), (.5, 0), (.75, 0), (0, .3), (0, -.3), (.5, .3), (.5, -.3), (-.25, 0)]
    for original, support, claimed in zip(nominal, supports, declared):
        pocket = next(p for p in POCKETS if p["id"] == original["bank_pocket"])
        x0, z0, x1, z1 = pocket["bounds_xz"]
        selected = None
        for dx, dz in shifts:
            candidate = dict(original);candidate["x"] += (1 if candidate["x"] > 0 else -1) * dx;candidate["z"] += dz
            measured = bank_source_support(candidate, models[candidate["alias"]], vertices[candidate["alias"]], terrain)
            if measured is None:
                continue
            box = model_box(candidate, models[candidate["alias"]], measured["runtime_base_y"] - 24)
            if box[8] < x0 or box[9] > x1 or box[10] < z0 or box[11] > z1 or any(boxes_overlap(box, other) for other in obstacles):
                continue
            # Protect the entire existing road width using the real source box.
            low, high = models[candidate["alias"]]["bounds_y_up"]
            c, s, scale = math.cos(candidate["yaw"]), math.sin(candidate["yaw"]), candidate["scale"]
            def local(point):
                x, z = point[0] - candidate["x"], point[1] - candidate["z"]
                return [c * x - s * z, s * x + c * z]
            if any(rectangle_distance(local(r["a"]), local(r["b"]), [low[0] * scale, low[2] * scale], [high[0] * scale, high[2] * scale]) <= r["half_width"] + .37 for r in roads):
                continue
            selected = candidate, measured, box
            break
        require(selected is not None, "Castle creek bank canonical dry source placement cannot be reproduced")
        expected, measured, box = selected;actual = placements[original["label"]]
        canonical_transform(actual, expected, "Castle creek banks")
        canonical_transform(claimed, expected, "Castle creek bank support")
        require(Counter(placements[label]["alias"] for label in labels) == Counter(r["alias"] for r in nominal),
                "Castle creek bank actual alias budget differs")
        for key in ("runtime_base_y", "root_terrain_y", "minimum_source_to_terrain_gap_m", "maximum_exposed_height_m"):
            require(isinstance(support.get(key), (int, float)) and math.isfinite(support[key]) and abs(support[key] - measured[key]) < .00002,
                    f"Castle creek bank actual terrain support differs: {actual['label']} {key}")
        require(support.get("root_is_dry") is True and support.get("actual_source_vertex_rays") == measured["actual_source_vertex_rays"] and
                support.get("terrain_support_grid_samples") == measured["terrain_support_grid_samples"] and
                support.get("exposed_source_vertices") == measured["exposed_source_vertices"] and
                same_numeric_rows(support.get("source_bounds_y_up", []), models[actual["alias"]]["bounds_y_up"]),
                "Castle creek bank source contact proof differs")
        rays += measured["actual_source_vertex_rays"];grids += measured["terrain_support_grid_samples"]
        obstacles.append(box)
    return {"instances": 36, "rocks": 24, "ferns": 10, "deadwood": 2, "pockets": 4,
            "existing_records_preserved": start, "canonical_prefix_hash_checked": True,
            "canonical_transforms_independently_reproduced": True, "resident_source_hashes_checked": True,
            "actual_source_vertex_terrain_rays": rays, "native_rigid_support_grid_samples": grids,
            "new_model_families": 0, "new_gpu_image_keys": 0, "global_density_changed": False}


def torch_shader_report(root=ROOT):
    root = Path(root)
    hashes = {}
    for stage in ("vs", "fs"):
        name = f"world_torch_flame.{stage}.glsl"
        source = root / "shaders" / name
        runtime = root / "projects/valwood_world/shaders" / name
        require(source.is_file() and runtime.is_file() and runtime.read_bytes() == source.read_bytes(),
                f"Village torch runtime shader is missing/stale: {name}")
        hashes[name] = sha256(source)
    return hashes


def torches_report(metadata, inventory_summary, placements, models, root=ROOT, *, roads, demo_route):
    """Reproduce fixed source sockets/contacts and verify the published emitter data."""
    import copy
    import build_village_torches as torch
    from build_castle_village import Author, ALIASES
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Village torch metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata.get("recipe_sha256"), "Village torch recipe changed")
    shader_hashes = torch_shader_report(root)
    start = canonical_prefix(metadata, placements, 2437, "Village torches")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 22 and list(placements)[start:start + 22] == labels and
            set(labels) == {label for label in placements if label.startswith("village_torch_")},
            "Village torch instances are missing/duplicated/out of order")
    require(metadata.get("maximum_near_lights") == 8 and metadata.get("aliases") == torch.ASSET_ALIASES,
            "Village torch model/light cap differs")
    asset_path = root / metadata["asset_manifest"]
    require(sha256(asset_path) == metadata.get("asset_manifest_sha256") and
            sha256(root / metadata["provenance_file"]) == metadata.get("provenance_sha256"),
            "Village torch asset manifest/provenance changed")
    assets = load_json(asset_path)
    require(len(assets["assets"]) == 10 and assets["budget"]["unique_images"] == 4 and
            assets["budget"]["max_image_side"] <= 1024 and assets["budget"]["estimated_rgba8_mips_mib"] <= 21.34,
            "Village torch purchased image/LOD budget differs")
    for row in assets["assets"]:
        path = root / "projects/valwood_world/models" / row["file"]
        require(sha256(path) == row["sha256"] and path.stat().st_size == row["bytes"], "Village torch actual LOD model changed")
        alias = "purchased_torch_" + row["alias"]
        if row["lod"] == "near" and alias in torch.ASSET_ALIASES:
            # The scenery loader grounds GLBs at their actual minimum Y;
            # retain that same normalization for the importer's float bounds.
            low, high = list(row["bounds_min"]), list(row["bounds_max"])
            high[1] -= low[1]
            low[1] = 0
            require(alias in models and models[alias]["file"] == row["file"] and models[alias]["sha256"] == row["sha256"] and
                    same_numeric_rows(models[alias]["bounds_y_up"], [low, high]),
                    "Village torch runtime source model/bounds differ")
    require(isinstance(inventory_summary, dict) and inventory_summary.get("sha256") == sha256(asset_path) and
            inventory_summary.get("provenance_sha256") == metadata["provenance_sha256"] and
            inventory_summary.get("placed_aliases") == torch.ASSET_ALIASES and
            inventory_summary.get("available_source_variants") == 5 and inventory_summary.get("used_source_variants") == 3 and
            inventory_summary.get("available_lod_models") == 10 and inventory_summary.get("shared_atlas_images") == 4 and
            inventory_summary.get("maximum_near_lights") == 8 and inventory_summary.get("budget") == assets["budget"],
            "Village torch purchased inventory metadata differs")
    # Reproduce source-measured authored mounts against the preserved prefix,
    # independently of the claimed new transforms/contacts/emitter coordinates.
    ALIASES.update({alias: Path(model["file"]).stem for alias, model in models.items()})
    author = Author({"roads": copy.deepcopy(roads), "demo_route_xz": copy.deepcopy(demo_route)})
    author.records = copy.deepcopy(list(placements.values())[:start])
    author.bounds = {alias: model["bounds_y_up"] for alias, model in models.items()}
    try:
        expected = torch.apply(author)
        for row in author.records[start:]:
            canonical_transform(placements[row["label"]], row, "Village torch")
            require(placements[row["label"]].get("parent_structure") == row.get("parent_structure"),
                    "Village torch mounting parent differs")
        for key in ("aliases", "model_sha256", "wall_mounts", "ground_labels", "emitters", "maximum_near_lights"):
            require(metadata.get(key) == expected[key], f"Village torch measured {key} differs")
        proof = torch.validate(author)
    except AssertionError as error:
        raise ValueError(f"Village torch actual source placement failed: {error}") from error
    generated = root / metadata["generated_include"]
    require(generated.read_text() == torch.include(expected) and sha256(generated) == metadata.get("generated_include_sha256"),
            "Village torch generated runtime emitter data differs")
    return {**proof, "canonical_prefix_hash_checked": True, "canonical_transforms_reproduced": True,
            "actual_wall_contacts_reproduced": True, "generated_emitters_verified": 22,
            "model_hashes_checked": True, "provenance_hash_checked": True,
            "runtime_shader_hashes_checked": shader_hashes,
            "shared_atlas_images": 4, "texture_rgba8_mips_mib": assets["budget"]["estimated_rgba8_mips_mib"]}


def farms_report(metadata, placements, models, pieces, root=ROOT, *, roads, demo_route, views):
    """Reproduce the actual farm layer, source support and cultivated rows."""
    import copy
    import build_village_farms as farm
    from build_castle_village import Author, ALIASES
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Village farm metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata.get("recipe_sha256"), "Village farm recipe changed")
    start = canonical_prefix(metadata, placements, 2459, "Village farms")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 181 and list(placements)[start:start + 181] == labels and
            set(labels) == {label for label in placements if label.startswith("village_farm_")},
            "Village farm instances are missing/duplicated/out of order")
    road_start = metadata.get("existing_road_count")
    require(road_start == 18 and hashlib.sha256(json.dumps(roads[:road_start], sort_keys=True, separators=(",", ":")).encode()).hexdigest() ==
            metadata.get("baseline_roads_sha256"), "Village farm existing road prefix changed")
    require(roads[road_start:road_start + len(farm.ROADS)] == farm.ROADS, "Village farm public roads differ")
    require(metadata.get("aliases") == farm.ASSET_ALIASES and metadata.get("wind_horizontal_guard_m") == .45,
            "Village farm source/wind policy differs")
    asset_path = root / metadata["asset_manifest"]
    require(sha256(asset_path) == metadata.get("asset_manifest_sha256"), "Village farm source inventory changed")
    assets = load_json(asset_path)
    require(assets["budget"]["unique_images"] == 3 and assets["budget"]["max_image_side"] <= 512 and
            assets["budget"]["estimated_rgba8_mips_mib"] <= 4.01, "Village farm shared image budget differs")
    for file_key, hash_key in (("source_provenance_file", "source_provenance_sha256"), ("source_recipe_file", "source_recipe_sha256")):
        require(sha256(root / metadata[file_key]) == metadata.get(hash_key), "Village farm source recipe/provenance changed")
    require(metadata["source_recipe_sha256"] == assets["source_recipe_sha256"] and metadata["source_budget"] == assets["budget"],
            "Village farm source recipe/image budget differs")
    require(len(assets["assets"]) == 4, "Village farm four source LODs are missing")
    for row in assets["assets"]:
        require(sha256(root / "projects/valwood_world/models" / row["file"]) == row["sha256"], "Village farm actual LOD model changed")
        alias = "village_crop_" + row["alias"]
        if row["lod"] == "near":
            require(alias in models and models[alias]["file"] == row["file"] and models[alias]["sha256"] == row["sha256"] and
                    same_numeric_rows(models[alias]["bounds_y_up"], [row["bounds_min"], row["bounds_max"]]),
                    "Village farm actual source model/bounds differ")
    ALIASES.update({alias: Path(model["file"]).stem for alias, model in models.items()})
    author = Author({"roads": copy.deepcopy(roads[:road_start]), "demo_route_xz": copy.deepcopy(demo_route)})
    author.records = copy.deepcopy(list(placements.values())[:start])
    author.bounds = {alias: model["bounds_y_up"] for alias, model in models.items()}
    try:
        expected = farm.apply(author)
        for row in author.records[start:]:
            canonical_transform(placements[row["label"]], row, "Village farm")
            require(placements[row["label"]].get("farm_plot") == row.get("farm_plot"), "Village farm crop ownership differs")
        for key in ("aliases", "model_sha256", "reused_model_sha256", "plots", "cultivated_ground", "wind_horizontal_guard_m",
                    "surface_supports", "ground_roads", "walk_xz", "walking_route_xz", "approach_xz", "views", "gateway",
                    "crop_instances", "corn_instances", "wheat_instances", "fence_instances", "detail_instances", "source_plant_population"):
            require(metadata.get(key) == expected[key], f"Village farm measured {key} differs")
        proof = farm.validate(author)
    except AssertionError as error:
        raise ValueError(f"Village farm source placement failed: {error}") from error
    include = root / metadata["generated_include"]
    require(include.read_text() == farm.include(expected) and sha256(include) == metadata.get("generated_include_sha256"),
            "Village farm generated ground/row data differs")
    actual_views = {view["id"]: view for view in views}
    require(all(actual_views.get(view["id"]) == view for view in farm.VIEWS), "Village farm inspection views differ")
    route = metadata["walking_route_xz"]
    require(route == generated_route(root, "castle_farms_route") and route == farm.WALK, "Village farm generated inspection route differs")
    clearance = route_clearance(list(placements.values()), models, route, pieces)
    stations = 0
    for a, b in zip(route, route[1:]):
        steps = math.ceil(math.dist(a, b) / .5)
        for i in range(steps + 1):
            point = [a[k] + (b[k] - a[k]) * i / steps for k in (0, 1)]
            require(any(segment_distance(point, road["a"], road["b"]) + .37 < road["half_width"] for road in roads),
                    "Village farm inspection leaves authored headlands")
            stations += 1
    length = sum(math.dist(a, b) for a, b in zip(route, route[1:]))
    return {**proof, "canonical_prefix_hash_checked": True, "canonical_transforms_reproduced": True,
            "model_hashes_checked": True, "actual_source_support_reproduced": True, "soil_and_rows_generated_match": True,
            "route": {"id": FARMS_ROUTE_ID, "local_xz": route, "length_m": length, "walking_speed_mps": WALK_SPEED,
                      "walking_seconds": length / WALK_SPEED, "frames_with_warmup": math.ceil(length / WALK_SPEED * 60) + 300,
                      "paved_stations_checked": stations, **clearance}}


def reinforcements_report(metadata, placements, models, root=ROOT, *, roads, demo_route, views):
    """Reproduce support contacts and the full eight-pose watch union."""
    import copy
    import build_village_reinforcements as layer
    from build_castle_village import Author, ALIASES
    from build_starting_area import glb_bounds
    root = Path(root)
    require(isinstance(metadata, dict) and metadata.get("schema_version") == 1, "Village reinforcement metadata is missing")
    require(sha256(root / metadata["recipe_file"]) == metadata.get("recipe_sha256"), "Village reinforcement recipe changed")
    start = canonical_prefix(metadata, placements, 2640, "Village reinforcements")
    labels = metadata["labels"]
    require(len(labels) == len(set(labels)) == 38 and list(placements)[start:] == labels and
            set(labels) == {label for label in placements if label.startswith(layer.PREFIX)},
            "Village reinforcement instances are missing/duplicated/out of order")
    require(len(roads) == metadata.get("existing_road_count") == 24 and
            layer.canonical_hash(roads) == metadata.get("baseline_roads_sha256"), "Village reinforcements changed existing roads")
    count = metadata.get("existing_view_count")
    require(isinstance(count, int) and layer.canonical_hash(views[:count]) == metadata.get("baseline_views_sha256") and
            layer.canonical_hash(demo_route) == metadata.get("baseline_demo_route_sha256"), "Village reinforcement changed existing cameras/routes")
    for source in metadata["source_manifests"].values():
        require(sha256(root / source["file"]) == source["sha256"], "Village reinforcement source inventory changed")
    for alias, stem in layer.ASSET_ALIASES.items():
        require(alias in models and models[alias]["file"] == stem + ".glb", "Village reinforcement runtime model is missing")
        path = root / "projects/valwood_world/models" / models[alias]["file"]
        require(sha256(path) == models[alias]["sha256"] and models[alias]["sha256"] == metadata["model_sha256"][path.name] and
                same_numeric_rows(models[alias]["bounds_y_up"], glb_bounds(path)), "Village reinforcement actual source model/bounds differ")
    ALIASES.update({alias: Path(model["file"]).stem for alias, model in models.items()})
    author = Author({"roads": copy.deepcopy(roads), "demo_route_xz": copy.deepcopy(demo_route), "demo_views": copy.deepcopy(views[:count])})
    author.records = copy.deepcopy(list(placements.values())[:start]);author.bounds = {a: m["bounds_y_up"] for a, m in models.items()}
    try:
        expected = layer.apply(author)
        for row in author.records[start:]:
            canonical_transform(placements[row["label"]], row, "Village reinforcement")
            for key in ("support_kind", "guard_role"):
                require(placements[row["label"]].get(key) == row.get(key), "Village reinforcement support/role differs")
        for key in ("aliases", "source_manifests", "model_sha256", "reused_model_sha256", "surface_supports", "lean_supports",
                    "watch_stations", "watch_cycle_seconds", "watch_union_bounds_yup", "watch_pose_count", "views", "tools", "mounted_guards",
                    "animated_watch_guards", "new_furniture", "old_population_unchanged", "old_routes_unchanged",
                    "animated_guards_have_no_static_scenery_duplicates", "watch_plaza_exception", "earlier_layer_support_policy", "source_route_sha256"):
            require(metadata.get(key) == expected[key], f"Village reinforcement measured {key} differs")
        proof = layer.validate(author)
    except AssertionError as error:
        raise ValueError(f"Village reinforcement source placement failed: {error}") from error
    include = root / metadata["generated_include"]
    require(include.read_text() == layer.include(expected) and sha256(include) == metadata.get("generated_include_sha256"),
            "Village reinforcement generated watch stations/cycle differ")
    actual_views = {v["id"]: v for v in views}
    require(all(actual_views.get(v["id"]) == v for v in layer.VIEWS), "Village reinforcement inspection views differ")
    from build_castle_village import watch_ground_rectangles
    rectangles = watch_ground_rectangles(expected)
    terrain = (root / "engine/world_village_layout.inc").read_text()
    block = terrain.split("static const VillageRect village_rects[]={", 1)[1].split("};", 1)[0]
    actual_rects = [[float(v) for v in row.split(",")] for row in re.findall(r'\{([^{}]+)\}', block)]
    require(same_numeric_rows(actual_rects[-4:], [row["values"] for row in rectangles]),
            "Village watch rooted grass-clear patches are missing/stale")
    ground = load_json(root / "projects/valwood_world/scenery/village_ground_history_manifest.json")
    require(ground.get("scene_manifest_sha256") == sha256(root / "projects/valwood_world/scenery/castle_village_manifest.json"),
            "Village watch ground history source scene is stale")
    contacts = ground.get("virtual_watch_contacts", [])
    require(len(contacts) == 4 and [row.get("label") for row in contacts] == [row["id"] for row in rectangles] and
            [row.get("grass_clear_rectangle") for row in contacts] == rectangles and
            all(row.get("virtual_runtime_station") and row.get("source_pose_count") == 8 and row.get("source_height_m") == .1 and
                row.get("section_segments", 0) > 0 for row in contacts), "Village watch measured boot-section history is missing/stale")
    pose_hashes = {name: value for name, value in expected["model_sha256"].items() if name.startswith("village_shield_knight_idle_")}
    require(ground.get("watch_pose_model_sha256") == pose_hashes and all(row.get("source_pose_sha256") == pose_hashes for row in contacts),
            "Village watch ground history pose source hashes differ")
    return dict(proof, canonical_prefix_hash_checked=True, canonical_transforms_reproduced=True,
                actual_source_support_reproduced=True, actual_pose_union_reproduced=True, generated_watch_matches=True,
                watch_ground_clear_patches=4, actual_boot_section_ground_history=True)


def scene_report(root=ROOT, *, hash_models=False):
    root = Path(root)
    directory = root / "projects/valwood_world/scenery"
    layout = load_json(directory / "castle_village_layout.json")
    manifest = load_json(directory / "castle_village_manifest.json")
    require(layout["schema_version"] == manifest["schema_version"] == 1, "Unknown village schema")
    for flag, key in (("castle_expansion", "castle_expansion"), ("castle_inner_wing", "castle_inner_wing"),
                      ("castle_creek_banks", "demo_castle_creek_banks"), ("village_torches", "village_torches"), ("village_farms", "village_farms"),
                      ("village_reinforcements", "village_reinforcements")):
        require(not layout.get(flag) or isinstance(manifest.get(key), dict), f"Configured castle layer metadata is missing: {key}")
    placements, models = manifest["placements"], manifest["models"]
    counts = Counter(item["alias"] for item in placements)
    require(dict(counts) == manifest["aliases"], "Manifest alias counts are stale")
    require(len(placements) == manifest["validation"]["records"], "Manifest placement count is stale")
    require(len({item["label"] for item in placements}) == len(placements), "Duplicate placement labels")
    instance_path = directory / SCENERY_FILES[-1]
    require(sha256(instance_path) == manifest["instances_sha256"], "Runtime village instance bytes differ from manifest")
    records = read_records(instance_path)
    require(len(records) == len(placements), "Runtime village count differs from manifest")
    width, depth = 100800., 100800. * 2818 / 5009
    for (alias, values, label), item in zip(records, placements):
        expected = [.487 + item["x"] / width, .320 + item["z"] / depth,
                    math.degrees(item["yaw"]), item["scale"], item["burial"]]
        if len(values) == 6:
            expected.append(item.get("elevation", 0))
        else:
            require(item.get("elevation", 0) == 0, f"Missing elevation: {label}")
        require(alias == item["alias"] and label == item["label"] and
                all(abs(a - b) <= 1e-7 * max(1, abs(b)) for a, b in zip(values, expected)),
                f"Runtime transform differs from manifest: {label}")
    all_records = {name: read_records(directory / name) for name in SCENERY_FILES}
    alias_rules = runtime_alias_rules(root / "engine/world_scenery.cpp")
    for rows in all_records.values():
        for alias, values, label in rows:
            validate_runtime_record(alias, values, alias_rules, label)
    total = sum(map(len, all_records.values()))
    union_counts = Counter(row[0] for rows in all_records.values() for row in rows)
    require(total <= SCENE_CAP and max(union_counts.values()) <= MODEL_CAP, "Authored batch limit exceeded")
    require(total == manifest["validation"]["combined_records"], "Combined scenery count is stale")
    for item in layout["preserve"]:
        require(sha256(directory / item["file"]) == item["sha256"], f"Protected scenery changed: {item['file']}")
    for alias, model in models.items():
        path = root / "projects/valwood_world/models" / model["file"]
        require(path.is_file() and path.stat().st_size > 20, f"Missing model: {alias}")
        if hash_models:
            require(sha256(path) == model["sha256"], f"Model changed since scene validation: {alias}")
    homes = [item for item in placements if item["alias"].startswith(("purchased_house_", "village_house_"))]
    require(len(homes) >= 55 and len(homes) == manifest["validation"]["homes"],
            "The complete housing district has not been authored/validated")
    for alias in ("village_keep", "village_inn", "village_smithy", "village_well", "starting_gate"):
        require(counts[alias] > 0, f"Demo landmark missing: {alias}")
    require(sum(counts[f"village_stall_{color}"] for color in ("red", "blue", "gold")) >= 12,
            "The full market booth layout is missing")
    districts = manifest.get("demo_districts")
    require(isinstance(districts, list), "Demo district metadata is missing")
    district_ids = {item["id"] for item in districts}
    require(district_ids == {"market_row", "southgate_lane", "coopers_return", "retained_merchants_lane"}
            and len(district_ids) == len(districts), "Demo does not contain all four unique districts")
    by_label = {item["label"]: item for item in placements}
    roads, squares = manifest.get("roads"), manifest.get("squares")
    require(isinstance(roads, list) and isinstance(squares, list), "Effective demo streets/squares are missing")
    overlay = manifest["demo_district"]
    require(sha256(root / overlay["source_file"]) == overlay["source_sha256"], "Demo recipe changed since generation")
    road_ids = {road["id"] for road in roads}
    claimed_houses, district_counts = set(), []
    for district in districts:
        labels = district["house_labels"]
        require(len(set(labels)) == len(labels) and len(labels) >= 8 and
                not claimed_houses.intersection(labels), f"Invalid/duplicate district homes: {district['id']}")
        require(all(label in by_label and by_label[label]["alias"].startswith(("purchased_house_", "village_house_"))
                    for label in labels), f"District house references are stale: {district['id']}")
        require(district["category_counts"]["houses"] == len(labels), f"District count is stale: {district['id']}")
        require(district["road_ids"] and set(district["road_ids"]) <= road_ids,
                f"District lacks an authored road: {district['id']}")
        props = district.get("prop_labels", [])
        require(len(set(props)) == len(props) and all(label in by_label for label in props),
                f"District prop references are stale: {district['id']}")
        categories = district["category_counts"]
        require(categories.get("dressing_instances", 0) == len(props), f"District dressing count is stale: {district['id']}")
        products = sum(by_label[label]["alias"].startswith("purchased_goods_") for label in props)
        plants = sum(by_label[label]["alias"] in TREES | {"starting_celandine", "starting_wildflower", "fern0", "fern1", "fern2"}
                     for label in props)
        require(categories.get("surface_products", 0) == products and categories.get("vegetation", 0) == plants,
                f"District product/vegetation counts are stale: {district['id']}")
        claimed_houses.update(labels)
        district_counts.append({"id": district["id"], "houses": len(labels), "props": len(props),
                                "surface_products": products, "vegetation": plants, "road_ids": district["road_ids"]})
    dressing = manifest.get("demo_dressing")
    require(isinstance(dressing, dict) and dressing.get("schema_version") == 1,
            "The detailed exterior and keep furnishings are not published")
    dressing_labels, interior_labels = dressing["labels"], dressing["interior_labels"]
    require(len(dressing_labels) == len(set(dressing_labels)) >= 400 and
            len(interior_labels) == len(set(interior_labels)) >= 40 and
            all(label in by_label for label in dressing_labels), "Detailed furnishings are missing/duplicated")
    pockets = dressing["pockets"]
    require(len(pockets) == len({pocket["id"] for pocket in pockets}) == 12, "All12 exterior story pockets are required")
    pocket_labels = [label for pocket in pockets for label in pocket["labels"]]
    require(len(pocket_labels) == len(set(pocket_labels)) and not set(pocket_labels).intersection(interior_labels)
            and set(pocket_labels) | set(interior_labels) == set(dressing_labels), "Pocket/hall furnishing ownership differs")
    require(set(pocket_labels) == {label for district in districts for label in district.get("prop_labels", [])},
            "District furnishing references differ from actual story pockets")
    for pocket in pockets:
        require(pocket["street_district"] in district_ids and pocket["labels"], f"Unconnected/empty story pocket: {pocket['id']}")
    require(dressing["new_model_families"] == dressing["new_texture_policies"] == 0,
            "Demo unexpectedly introduces model/image residency")
    dressing_path = directory / dressing["source_file"]
    require(sha256(dressing_path) == dressing["source_sha256"], "Dressing recipe changed since generation")
    frontages = frontage_report(manifest.get("demo_frontages"), by_label, districts, dressing_labels, root)
    market = market_report(manifest.get("demo_market_lively"), by_label, models, root)
    pet_assets = pets_report(load_json(root / "projects/valwood_world/models/village_pets_manifest.json"), root, hash_models=hash_models)
    require(re.search(r'world_village_resident_count\s*=\s*128\s*;', (root / "engine/world_village_population.h").read_text()),
            "CPU population target differs from runtime")
    lamps = [by_label[label] for label in interior_labels if by_label[label]["alias"] == "purchased_prop_lamp"]
    expected_lamps = {(-9.8, -138.5), (9.8, -138.5), (-9.8, -151.5), (9.8, -151.5)}
    require(len(lamps) == 4 and {(round(item["x"], 3), round(item["z"], 3)) for item in lamps} == expected_lamps,
            "Actual hall lamps differ from the four shader sources")
    require(sha256(root / "engine/world_village_colliders.inc") == manifest["collision_include_sha256"],
            "Collision data changed since scene validation")
    pieces = collision_boxes(root / "engine/world_village_colliders.inc")
    expansion = None
    require(not layout.get("castle_expansion") or isinstance(manifest.get("castle_expansion"), dict),
            "Configured castle expansion layer is missing")
    if manifest.get("castle_expansion"):
        pieces["castle_drawbridge"] = drawbridge_collision_boxes(root / "engine/world_castle_drawbridge_colliders.inc")
        expansion = expansion_report(manifest["castle_expansion"], manifest.get("castle_creek"), by_label, models, pieces, root,
                                     roads=roads, squares=squares, views=manifest["demo_views"])
        from check_castle_training import inspect as inspect_training
        try:
            expansion["training"]["actual_geometry_proof"] = inspect_training(directory / "castle_village_manifest.json")
        except AssertionError as error:
            raise ValueError(f"Castle training actual source geometry failed: {error}") from error
    wings, banks = None, None
    if layout.get("castle_inner_wing") or any(label.startswith("castle_inner_") for label in by_label):
        wings = wing_report(manifest.get("castle_inner_wing"), by_label, models, pieces, root, roads=roads)
    if layout.get("castle_creek_banks") or any(label.startswith("castle_creek_bank_") for label in by_label):
        banks = banks_report(manifest.get("demo_castle_creek_banks"), by_label, models, root, roads=roads)
    torches = None
    if layout.get("village_torches") or any(label.startswith("village_torch_") for label in by_label):
        torches = torches_report(manifest.get("village_torches"), manifest.get("purchased_torches"), by_label, models, root,
                                 roads=roads, demo_route=manifest["demo_route_xz"])
    farms = None
    if layout.get("village_farms") or any(label.startswith("village_farm_") for label in by_label):
        farms = farms_report(manifest.get("village_farms"), by_label, models, pieces, root,
                             roads=roads, demo_route=manifest["demo_route_xz"], views=manifest["demo_views"])
    reinforcements = None
    if layout.get("village_reinforcements") or any(label.startswith("village_reinforcement_") for label in by_label):
        reinforcements = reinforcements_report(manifest.get("village_reinforcements"), by_label, models, root,
                                               roads=roads, demo_route=manifest["demo_route_xz"], views=manifest["demo_views"])
    hero = hero_report(manifest.get("demo_market_hero"), manifest.get("demo_hero_grove"), by_label, models, pieces, root,
                       roads=roads, squares=squares)
    route = manifest.get("demo_route_xz")
    require(isinstance(route, list) and len(route) >= 8, "Detailed demo route is not authored yet")
    require(all(len(point) == 2 and all(math.isfinite(value) for value in point) for point in route), "Invalid demo route")
    require(route[0] == route[-1], "Demo route must return to its starting point")
    require(all(math.dist(a, b) > .001 for a, b in zip(route, route[1:])), "Zero-length demo leg")
    compiled_route = generated_route(root, "castle_demo_route")
    require(compiled_route == route, "Generated runtime demo route differs from the effective scene manifest")
    require(all(max(map(abs, point)) <= layout["bounds"]["flat_half_extent_m"] for point in route),
            "Demo route leaves the flat authored terrace")
    # Full old street, market, and keep approaches must be traversed, rather
    # than merely visible from a distant camera.
    for label, point in {"lane south": [61.75, 49], "lane north": [61.75, -27],
                         "market": [0, 8], "forecourt": [0, -88]}.items():
        require(min(segment_distance(point, a, b) for a, b in zip(route, route[1:])) < 1.,
                f"Demo skips {label}")
    clearance = route_clearance(placements, models, route,
                                pieces)
    stations = 0
    for a, b in zip(route, route[1:]):
        steps = math.ceil(math.dist(a, b) / .5)
        for i in range(steps + 1):
            point = [a[k] + (b[k] - a[k]) * i / steps for k in (0, 1)]
            on_road = any(segment_distance(point, road["a"], road["b"]) + .37 < road["half_width"]
                          for road in roads)
            in_square = any(all(abs(point[k] - square["center"][k]) + .37 < square["half"][k] for k in (0, 1))
                            for square in squares)
            require(on_road or in_square, f"Demo leaves authored streets: {point}")
            stations += 1
    length = sum(math.dist(a, b) for a, b in zip(route, route[1:]))
    return {"schema_version": 1, "cpu_scene_checks_passed": True,
            "native_route_verified": False, "performance_verified": False,
            "source_files": {name: len(rows) for name, rows in all_records.items()},
            "total_authored_instances": total, "castle_instances": len(placements),
            "largest_alias_batch": max(union_counts.values()), "castle_alias_counts": dict(sorted(counts.items())),
            "homes": len(homes), "districts": district_counts,
            "furnishings": {"added_instances": len(dressing_labels), "exterior_story_pockets": len(pockets),
                            "hall_objects": len(interior_labels), "hall_lamps": len(lamps),
                            "new_models": 0, "new_texture_policies": 0},
            "frontages": frontages,
            "market": market, "expected_population": EXPECTED_POPULATION, "expected_pets": EXPECTED_PETS,
            "hero_market": hero,
            **({"castle_expansion": expansion} if expansion else {}),
            **({"castle_inner_wing": wings} if wings else {}),
            **({"castle_creek_banks": banks} if banks else {}),
            **({"village_torches": torches} if torches else {}),
            **({"village_farms": farms} if farms else {}),
            **({"village_reinforcements": reinforcements} if reinforcements else {}),
            "pet_assets": pet_assets,
            "route": {"id": ROUTE_ID, "waypoints": len(route), "length_m": length,
                      "walking_speed_mps": WALK_SPEED, "walking_seconds": length / WALK_SPEED,
                      "frames_with_warmup": math.ceil(length / WALK_SPEED * 60) + 120 + 180,
                      "local_xz": route, "paved_stations_checked": stations, **clearance},
            "models_hash_checked": hash_models, "runtime_alias_constraints_checked": True,
            "runtime_route_source_matches": True,
            "limits": ["CPU geometry checks do not certify image quality or frame rate.",
                       "Dialogue, quests, trading, combat and interiors beyond the keep ground hall are outside this visual demo."]}


def validate_native_report(scene, profile_path, *, require_budget=True, width=1600, height=900, expected_route=ROUTE_ID):
    profile_path = Path(profile_path)
    profile = load_json(profile_path)
    world = load_json(str(profile_path) + ".world.json")
    renderer = profile["renderer"].lower()
    require(not any(name in renderer for name in ("llvmpipe", "softpipe", "software")), "Native GPU report required")
    require(profile["samples"] > 120 and profile["dropped_samples"] == 0, "Invalid native timing samples")
    for key in ("gl_error", "failed_tiles", "travel_loading_frames", "remaining_texture_entries", "remaining_texture_references"):
        require(world[key] == 0, f"Runtime failure: {key}={world[key]}")
    require(world["authored_instances"] == scene["total_authored_instances"], "Native scene count differs from authored files")
    sources = {Path(item["path"]).name: item["instances"] for item in world["authored_sources"]}
    require(sources == scene["source_files"] and world["authored_source_files"] == len(sources), "Native source counts differ")
    route = world["benchmark_route"]
    routes = {ROUTE_ID: scene["route"]}
    if "hero_market" in scene: routes[HERO_ROUTE_ID] = scene["hero_market"]["route"]
    if "castle_expansion" in scene and "route" in scene["castle_expansion"]:
        routes[EXPANSION_ROUTE_ID] = scene["castle_expansion"]["route"]
    if "village_farms" in scene and "route" in scene["village_farms"]:
        routes[FARMS_ROUTE_ID] = scene["village_farms"]["route"]
    require(expected_route in routes, "Unknown authored native route")
    authored_route = routes[expected_route]
    require(route["active"] and route["id"] == expected_route and route["complete"], "Native demo route did not complete")
    require(abs(route["length_m"] - authored_route["length_m"]) < .0001, "Runtime demo route differs from authored route")
    require(route["return_error_m"] < .001, "Native demo did not return to start")
    for key in ("collision_blocked_steps", "slope_blocked_steps", "sliding_steps"):
        require(route[key] == 0, f"Native demo route failed: {key}")
    require(abs(route["actual_distance_m"] - authored_route["length_m"]) < .01, "Native walking distance differs")
    population = world["village_population"]
    require(all(population.get(key) == value for key, value in EXPECTED_POPULATION.items()) and
            population["pose_assets"] == 76 and population["walking"] > 0,
            "Full animated population is not running")
    pets = world.get("village_pets", {})
    require(all(pets.get(key) == value for key, value in EXPECTED_PETS.items()) and pets.get("pose_assets") == 32 and
            pets.get("pose_batch_capacity") == 38 and pets.get("gpu_pose_blending") is True and
            pets.get("walking", 0) > 0 and pets.get("idling", 0) > 0 and
            (pets.get("peak_visible", 0) > 0 or expected_route == FARMS_ROUTE_ID),
            "Full animated cats and dogs are not running")
    if scene.get("castle_expansion"):
        training = world.get("castle_training", {})
        require(all(training.get(key) == value for key, value in
                    {"actors": 7, "trainees": 6, "instructors": 1, "pose_assets": 28, "blend_batches": 28}.items()) and
                training.get("gpu_pose_blending") is True and training.get("updates", 0) > 0,
                "Full smooth castle training group is not running")
    if scene.get("village_reinforcements"):
        watch = world.get("village_watch", {})
        require(all(watch.get(key) == value for key, value in
                    {"actors": 6, "pose_assets": 8, "blend_batches": 8,
                     "gpu_pose_blending": True, "rooted_stations": True}.items()) and
                watch.get("updates", 0) >= profile["frames"] and
                isinstance(watch.get("visible"), (int, float)) and 0 <= watch["visible"] <= 6 and
                isinstance(watch.get("peak_visible"), (int, float)) and 0 <= watch["peak_visible"] <= 6,
                "Complete rooted animated village watch is not running")
    if scene.get("village_torches"):
        torches = world.get("village_torches", {})
        require(all(torches.get(key) == value for key, value in
                    {"models": 22, "emitters": 22, "light_cap": 8, "failures": 0,
                     "main_frames": profile["frames"], "shared_smooth_flicker": True,
                     "wall_plane_and_hall_clip": True, "point_shadow_maps": False}.items()) and
                isinstance(torches.get("peak_lights"), (int, float)) and 0 <= torches["peak_lights"] <= 8 and
                isinstance(torches.get("peak_visible_flames"), (int, float)) and 0 <= torches["peak_visible_flames"] <= 22,
                "Complete bounded village torch lighting is not running")
        if expected_route in (ROUTE_ID, HERO_ROUTE_ID, EXPANSION_ROUTE_ID):
            require(torches["peak_visible_flames"] > 0 and torches.get("flame_draws", 0) > 0,
                    "Village torch flames were not visible along the authored route")
    require(world["village_animation"]["gpu_pose_blending"] and world["village_animation"]["pose_batch_capacity"] == 91,
            "Smooth NPC interpolation is missing")
    require(world.get("shadow_map_side", 0) >= 3072 and world.get("soft_shadow_taps") == 9,
            "Detailed castle shadow quality is disabled")
    surfaces = world["village_surfaces"]
    require(surfaces["directional_sky"] and surfaces["ground_history"] and surfaces["stochastic_paving"] and
            surfaces["terrain_sampler_count"] == 16, "The complete village lighting/material path is disabled")
    practicals = world["castle_practicals"]
    require(practicals["enabled"] and practicals["count"] == 4 and practicals["radius_m"] == 20 and
            abs(practicals["emitter_y_m"] - 26.895) < .0001 and practicals["masked_to_receiving_hall"] and
            practicals["shadow_maps"] is False, "The four bounded hall practicals are not configured")
    require(world["texture_cache_unmanaged_uploads"] == 0, "Native image cache overflow")
    from benchmark_cinematic_scenery import verify_integrated_lighting
    verify_integrated_lighting(world, profile["frames"], width=width, height=height)
    metrics = {key: profile[key] for key in ("mean_fps", "one_percent_low_fps", "p99_ms", "max_ms",
                                            "within_45fps_budget_percent")}
    require(all(math.isfinite(value) and value > 0 for value in metrics.values()), "Invalid frame-time metrics")
    budget = metrics["mean_fps"] >= 45 and metrics["one_percent_low_fps"] >= 45 and metrics["p99_ms"] <= 1000 / 45
    require(budget or not require_budget, f"45FPS timing target failed: {metrics}")
    return {"native_route_verified": True, "performance_verified": budget,
            "profile": str(profile_path.resolve()), "renderer": profile["renderer"], "metrics": metrics}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Save acceptance JSON; no file is written when omitted")
    parser.add_argument("--hash-models", action="store_true", help="Also verify every village model's SHA256")
    parser.add_argument("--profile", type=Path, help="Validate a completed native demo timing report")
    parser.add_argument("--allow-slow", action="store_true", help="Report a failed FPS target without rejecting a structurally valid run")
    args = parser.parse_args()
    try:
        report = scene_report(hash_models=args.hash_models)
        if args.profile:
            report.update(validate_native_report(report, args.profile, require_budget=not args.allow_slow))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Castle demo acceptance failed: {error}\n")
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
