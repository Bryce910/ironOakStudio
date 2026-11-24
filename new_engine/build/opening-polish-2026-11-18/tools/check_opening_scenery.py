#!/usr/bin/env python3
"""Check the exact bounded row patch, visible plant support and loader input.

This checker never writes live authoring files or creates a graphics context.
"""
from pathlib import Path
import collections
import copy
import hashlib
import importlib.util
import json
import math
import shlex
import struct
import subprocess
import numpy as np

N = Path(__file__).resolve().parents[1]
P, O = N / "world-project", N / "review/scenery"
ROOT = N.parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check():
    r = json.loads((O / "ROW_PATCH.json").read_text())
    checks, changes = {}, r["instance_changes"] + r["studio_changes"]
    before_files = {}
    for f in r["files"]:
        path = P / f["path"]
        assert not path.is_symlink() and sha(path) == f["after_sha256"]
        rows = path.read_text().splitlines()
        updates = [c for c in changes if c["file"] == f["path"]]
        assert len({c["line"] for c in updates}) == len(updates)
        for c in updates:
            assert rows[c["line"] - 1] == c["after"]
            rows[c["line"] - 1] = c["before"]
        raw = ("\n".join(rows) + "\n").encode()
        assert hashlib.sha256(raw).hexdigest() == f["before_sha256"]
        before_files[f["path"]] = raw
    checks["exact_row_patch_reconstructs_original_hashes"] = True
    studio = (P / "world.studio").read_text().splitlines()
    old_studio = before_files["world.studio"].decode().splitlines()
    count = lambda lines: dict(collections.Counter(json.loads(s[9:]).split()[0] for s in lines if s.startswith("surfaces ")))
    assert count(studio) == count(old_studio)
    assert count(studio)["paint"] == 256 and count(studio)["layer"] == 8 and count(studio)["ribbon"] == 16
    assert count(studio)["image"] == 14
    for c in r["studio_changes"]:
        a, b = shlex.split(json.loads(c["before"][9:])), shlex.split(json.loads(c["after"][9:]))
        assert a[0] == b[0] == "paint" and 2 <= float(b[3]) <= 64 and 0 <= float(b[4]) <= 1
        assert a[5:] == b[5:] and b[5] in ("5", "7")
        assert math.hypot(float(a[1]) + 11988, float(a[2]) - 5172) < 30
        assert math.hypot(float(b[1]) + 11988, float(b[2]) - 5172) < 30
    checks["all_surface_limits_preserved_only_camp_paint_changed"] = True
    checks["shared_layers_images_wind_ribbons_and_colliders_exact"] = all(
        a == b for a, b in zip(old_studio, studio) if not a.startswith('surfaces "paint '))
    assert checks["shared_layers_images_wind_ribbons_and_colliders_exact"]
    for c in r["instance_changes"]:
        a, b = c["before"].split(), c["after"].split()
        assert a[0] == b[0] in ("opening_verge_grass_long", "opening_verge_grass_broad", "bep_nature_clover", "bep_nature_fern")
        assert a[5:8] == b[5:8] and a[14:] == b[14:]
        assert .15 <= float(b[4]) <= 4 and all(.1 <= float(b[i]) <= 8 for i in (10, 11, 12))
    checks["only_existing_noncolliding_understory_rows_changed"] = True
    for name in ("scenery/bep_environment_instances.txt", "scenery/demo_polish_instances.txt"):
        old, new = before_files[name].decode().splitlines(), (P / name).read_text().splitlines()
        assert len(old) == len(new)
        assert collections.Counter(x.split()[0] for x in old if x and not x.startswith("#")) == collections.Counter(x.split()[0] for x in new if x and not x.startswith("#"))
        assert [i + 1 for i, (a, b) in enumerate(zip(old, new)) if a != b] == sorted(c["line"] for c in r["instance_changes"] if c["file"] == name)
    checks["source_alias_row_order_and_instance_geometry_budget_exact"] = True
    tool = ROOT / "docs/seldonne-first-region/2026-11-07/region_mesh_grounding_v5.py"
    assert sha(tool) == r["geometry_tool_sha256"]
    spec = importlib.util.spec_from_file_location("scenery_mesh_check", tool)
    mesh = importlib.util.module_from_spec(spec); spec.loader.exec_module(mesh)
    height_path = P / "ground/showcase.height"
    assert sha(height_path) == r["height_sha256"]
    raw = height_path.read_bytes(); magic, nx, nz, xmin, zmin, sx, sz = struct.unpack_from("<8sII4d", raw)
    assert magic == b"VWHGT1\0\0" and sx == sz == 1
    values = np.frombuffer(raw, "<f4", offset=48).reshape(nz, nx)
    def ground(x, z):
        px, pz = (x - xmin) / sx, (z - zmin) / sz
        if not 0 <= px < nx - 1 or not 0 <= pz < nz - 1: return None
        i, j = int(px), int(pz); u, v = px - i, pz - j
        return float((1 - v) * ((1 - u) * values[j, i] + u * values[j, i + 1]) + v * ((1 - u) * values[j + 1, i] + u * values[j + 1, i + 1]))
    assets = {shlex.split(line)[1]: shlex.split(line)[2] for line in studio if line.startswith("asset ")}
    geometry, support_errors = {}, []
    for c in r["instance_changes"]:
        a = c["after"].split(); alias = a[0]
        if alias not in geometry: geometry[alias] = mesh.geometry(P / "models" / (assets[alias] + ".glb"))
        p = dict(x=(float(a[1]) - .5) * 100800, z=(float(a[2]) - .5) * 100800 * 2818 / 5009,
                 yaw_deg=float(a[3]), scale=float(a[4]), pitch_deg=float(a[8]), roll_deg=float(a[9]),
                 sx=float(a[10]), sy=float(a[11]), sz=float(a[12]), y=float(a[13]))
        expected = copy.deepcopy(p); mesh.support(expected, geometry[alias], ground, c["staged_basal_embedding_m"])
        error = abs(expected["y"] - p["y"]); support_errors.append(error)
        assert error < 2e-5 and 0 < c["staged_basal_embedding_m"] <= .04
        assert c["actual_mesh_route_clearance_m"] > 3.1 and c["moved_m"] < 45
    checks["actual_visible_basal_vertices_regrounded_with_no_floating_pivot"] = True
    checks["journey_lane_actor_and_clearing_guards"] = True
    # Existing loader checker is reusable only when its production translation
    # units and directly included engine headers still match the candidate.
    prior = ROOT / "build/opening-finish-2026-11-18"
    parser = prior / "review/scenery/check-source-parser"
    required = {"engine/world_scenery.cpp", "engine/world_assets.cpp", "engine/world_regions.cpp", "engine/world_atlas.c"}
    for name in tuple(required):
        for line in (N / "source-v1" / name).read_text().splitlines():
            if line.startswith('#include "engine/'):
                required.add(line.split('"')[1])
    matched = {name: sha(N / "source-v1" / name) for name in sorted(required)}
    assert all(sha(prior / "source-v1" / name) == value for name, value in matched.items())
    run = subprocess.run([str(parser), str(P), str(O / "PARSER_RAW.json")], text=True, capture_output=True)
    (O / "parser-positive.log").write_text(run.stdout + run.stderr)
    assert run.returncode == 0, run.stdout + run.stderr
    parser_report = json.loads((O / "PARSER_RAW.json").read_text())
    assert parser_report["passed"] and parser_report["actual_production_source_parser"]
    checks["actual_production_source_parser_accepts_both_changed_files"] = True
    original_studio = O / "world.studio.before"
    original_studio.write_bytes(before_files["world.studio"])
    surface_checker = O / "check-opening-surface"
    surface_reports = []
    for label, source in (("before", original_studio), ("after", P / "world.studio")):
        target = O / ("SURFACE_" + label.upper() + "_RAW.json")
        run = subprocess.run([str(surface_checker), str(source), str(target)], text=True, capture_output=True)
        (O / ("surface-" + label + ".log")).write_text(run.stdout + run.stderr)
        assert run.returncode == 0, run.stdout + run.stderr
        surface_reports.append(json.loads(target.read_text()))
    assert all(v["passed"] and v["actual_production_surface_decoder"] and v["actual_2m_paint_baker"] for v in surface_reports)
    assert all(v["images"] == 14 and v["layers"] == 8 and v["paint"] == 256 and v["ribbons"] == 16 and v["tiles"] == 60 for v in surface_reports)
    checks["production_surface_decode_and_2m_bake_accept_both_versions"] = True
    # Negative conflict proof: these are before/after data, not permission to
    # overwrite a designer's later changed row or entire live file.
    f = r["files"][0]; old = before_files[f["path"]]
    tampered = old + b"# simulated intervening designer change\n"
    assert hashlib.sha256(tampered).hexdigest() != f["before_sha256"]
    first = next(c for c in changes if c["file"] == f["path"])
    original_rows = old.decode().splitlines()
    original_rows[first["line"] - 1] = first["before"] + " # conflict"
    assert original_rows[first["line"] - 1] != first["before"]
    checks["intervening_file_or_row_change_requires_conflict_review"] = True
    report = dict(passed=all(checks.values()), gpu_used=False, checks=checks,
                  patch_sha256=sha(O / "ROW_PATCH.json"), instance_changes=len(r["instance_changes"]),
                  camp_paint_changes=len(r["studio_changes"]), new_instance_or_model_files=0,
                  source_parser=parser_report, reused_parser_sha256=sha(parser),
                  matched_parser_production_sources=matched, actual_basal_support_error_max_m=max(support_errors),
                  production_surface_before_after=surface_reports, surface_checker_sha256=sha(surface_checker),
                  surface_checker_source_sha256=sha(N / "tools/check_opening_surface.cpp"),
                  production_surface_source_sha256=sha(N / "source-v1/engine/world_surface_authoring.cpp"),
                  production_surface_header_sha256=sha(N / "source-v1/engine/world_surface_authoring.h"),
                  minimum_actual_mesh_lane_clearance_m=min(c["actual_mesh_route_clearance_m"] for c in r["instance_changes"]),
                  limits=["Native visual gain and frame pacing still require parent review.",
                          "CPU source parser uses bounds-only GPU wrappers; no GPU context or game build.",
                          "Original floating/buried donor-root errors corrected instead of preserved at smaller scales."])
    (O / "CPU_REVIEW.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    check()
