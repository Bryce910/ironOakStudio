#!/usr/bin/env python3
"""Read-only input audit; writes its result only to review/CPU_REVIEW.json.

AUDIT_EXPECTATIONS.json is intentionally supplied after the changes are reviewed.
Schema: source_changed/source_added/shader_changed/shader_added are exact maps
of relative filename to SHA256. candidate_files maps paths inside this generation
to final hashes. authoring_changes contains {live_path, staged_path, before_sha256,
after_sha256}. No changed file is implicitly allowed by a directory wildcard.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys

N = Path(__file__).resolve().parents[1]
BASE = N.with_name("window-camera-2026-11-18")


def sha(path):
    with Path(path).open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def inventory(root):
    return {str(p.relative_to(root)): sha(p) for p in root.rglob("*") if p.is_file()}


def path_in_candidate(value):
    path = N / value
    need(path.absolute().is_relative_to(N), "Path outside candidate: " + value)
    need(".." not in Path(value).parts, "Parent path not allowed: " + value)
    return path


def exact_delta(before, after, changed, added, label):
    need(not (before.keys() - after.keys()), label + " files were removed")
    actual_changed = {k: after[k] for k in before.keys() & after.keys() if before[k] != after[k]}
    actual_added = {k: after[k] for k in after.keys() - before.keys()}
    need(actual_changed == changed, label + " changes differ from reviewed whitelist")
    need(actual_added == added, label + " additions differ from reviewed whitelist")
    return dict(changed=sorted(changed), added=sorted(added))


def audit(after_authoring=False):
    expected_path = N / "review/AUDIT_EXPECTATIONS.json"
    expected = json.loads(expected_path.read_text())
    need(Path(expected["baseline_generation"]) == BASE, "Unexpected baseline generation")
    need(sha(BASE / "runtime/rpg") == expected["baseline_binary_sha256"], "Baseline binary changed")
    origin = json.loads((N / "SOURCE_ORIGIN.json").read_text())
    before = inventory(BASE / "source-v1")
    need(origin["baseline_sha256"] == before, "Immutable predecessor source changed")
    actual = inventory(N / "source-v1")
    source_delta = exact_delta(before, actual, expected["source_changed"], expected["source_added"], "Source")
    build = json.loads((N / "runtime/ORIGIN_BUILD.json").read_text())
    binary = sha(N / "runtime/rpg")
    need(build["ready"] and build["application_units"] == 104 and build["sha256"] == binary, "Build incomplete")
    need(build["source_sha256"] == actual, "Compiled source differs from current candidate")
    plan = json.loads((N / "runtime-native-v1/BUILD_PLAN.json").read_text())
    need(len(plan["units"]) == 104, "Application closure changed")
    for row in plan["units"]:
        receipt = json.loads(Path(row["target"]).with_suffix(".json").read_text())
        need(receipt["command"] == row["command"], "Object command differs: " + row["logical_source"])
        need(bool(receipt["actual_dependency_sha256"]), "No object dependencies")
        for path, digest in receipt["actual_dependency_sha256"].items():
            need(sha(path) == digest, "Compiled dependency changed: " + path)
    shader_delta = exact_delta(
        inventory(BASE / "shared/shaders"), inventory(N / "shared/shaders"),
        expected["shader_changed"], expected["shader_added"], "Shader",
    )
    for path, digest in expected["candidate_files"].items():
        need(sha(path_in_candidate(path)) == digest, "Reviewed candidate file changed: " + path)
    authoring_plan = json.loads((N / "review/AUTHORING_PLAN.json").read_text())
    need(expected["authoring_changes"] == authoring_plan["changes"], "Authored plan differs from reviewed whitelist")
    for row in authoring_plan["original_assets"]:
        need(sha(row["live"]) == row["before_sha256"], "Original live asset changed: " + row["live"])
        need(os.readlink(row["live"]) == row["before_symlink"], "Original live asset link changed")
        need(sha(N / "world-project" / row["relative"]) == row["before_sha256"], "Original staged asset changed")
    for row in authoring_plan["new_asset_links"]:
        need(sha(row["candidate_asset"]) == row["after_sha256"], "New asset bytes changed")
        need(sha(N / "world-project" / row["relative"]) == row["after_sha256"], "New staged asset link changed")
        target = Path(row["live"])
        if after_authoring:
            need(target.is_symlink() and target.resolve() == Path(row["candidate_asset"]).resolve(), "New live asset link differs")
        else:
            need(not target.exists() and not target.is_symlink(), "Additive asset exists before publication")
    for path in expected.get("cpu_reviews", []):
        need(json.loads(path_in_candidate(path).read_text())["passed"] is True, "CPU review failed: " + path)
    terrain = json.loads((N / "review/TERRAIN_PROJECTION_CPU.json").read_text())
    need(terrain["passed"] is True and terrain["shader_sha256"] == sha(N / "shared/shaders/world_surface_library.fs.glsl"), "Terrain projection review failed or stale")
    protected = json.loads((N / "review/LIVE_BEFORE.json").read_text())
    intentional = {row["live_path"]: row for row in expected["authoring_changes"]}
    need(len(intentional) == len(expected["authoring_changes"]), "Duplicate authored path")
    for path, row in intentional.items():
        need(path in protected, "Authored change lacks live baseline snapshot: " + path)
        need(protected[path] == row["before_sha256"], "Wrong authored baseline: " + path)
        need(sha(path_in_candidate(row["staged_path"])) == row["after_sha256"], "Authored staging changed: " + path)
    for path, digest in protected.items():
        wanted = intentional[path]["after_sha256"] if after_authoring and path in intentional else digest
        need(sha(path) == wanted, "Live authored file changed outside publication: " + path)
    camera = json.loads((N / "review/CAMERA_CPU.json").read_text())
    need(camera["passed"] is True, "Camera math review failed")
    need(len(camera["tests"]) == 4, "Camera regression coverage changed")
    for name, result in camera["tests"].items():
        need(result["passed"] and result["dependency_sha256"], "Camera test incomplete: " + name)
        for path, digest in result["dependency_sha256"].items():
            need(sha(N / "source-v1" / path) == digest, "Camera dependency changed: " + path)
    return dict(
        passed=True, binary_sha256=binary, baseline_generation=str(BASE),
        expectations_sha256=sha(expected_path), dependency_units_verified=104,
        source_delta=source_delta, shader_delta=shader_delta,
        reviewed_candidate_files=len(expected["candidate_files"]),
        original_asset_bytes_and_links_preserved=True, additive_tv_assets_verified=2,
        terrain_projection_review_sha256=sha(N / "review/TERRAIN_PROJECTION_CPU.json"),
        protected_live_files=len(protected), intentional_authored_paths=sorted(intentional),
        after_authoring=after_authoring, camera_math_tests=4,
        native_only=True, Windows_work_performed=False,
        limit="Input integrity and CPU camera math; image quality and runtime performance require separate native reviews.",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--after-authoring", action="store_true")
    args = parser.parse_args()
    try:
        report = audit(args.after_authoring)
    except Exception as error:
        report = dict(passed=False, error=str(error))
    (N / "review/CPU_REVIEW.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    sys.exit(0 if report["passed"] else 1)
