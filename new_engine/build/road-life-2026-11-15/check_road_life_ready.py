#!/usr/bin/env python3
"""Verify the isolated Linux busy-road demo and its actual scene acceptance."""
from pathlib import Path
import argparse
import contextlib
import importlib.util
import io
import json
import sys
import tempfile

N = Path(__file__).resolve().parent
ENGINE = N.parents[1]
ROOT = ENGINE.parent
BASE = N.parent / "demo-final-pass-2026-11-15"
BASE_CHECKER_SHA = "c4bb5f1fe1372fb807c2a76acef1da170e53210f07d5ed242d6b2944df9ad849"
BASE_READY_SHA = "205a8585492825cf7f95521e8fc01b61a52b32e60f0f633a6700afbb0b3d449c"
READY = N / "ROAD_LIFE_READY.json"
PROJECT = N / "demo-project"
REVIEWS = (
    "review/assets/ROAD_ASSETS_CPU_REVIEW.json",
    "review/assets/CART_SOURCE_PAINT_REVIEW.json",
    "review/assets/ROAD_ASSETS_VISUAL_REVIEW.json",
    "review/routes/PARKED_QUEST_CART_SHIFT.json",
    "review/routes/SOURCE_CAPSULE_ENVELOPE_REVIEW.json",
    "review/routes/ROAD_TRAFFIC_ROUTE_REVIEW.json",
    "review/road-traffic/ROAD_TRAFFIC_CPU_REVIEW.json",
    "review/road-traffic/ROAD_TRAFFIC_LIVENESS_CPU_REVIEW.json",
    "review/road-traffic/ROAD_TRAFFIC_GPU_REVIEW.json",
)
NATIVE_REVIEWS = (
    "review/native-road/ROAD_NATIVE_REVIEW.json",
    "review/ROAD_LIFE_VISUAL_REVIEW.json",
)


def load_base():
    # Pin the accepted verifier before executing it. Its cache retains expected
    # bytes even when an inherited inode/stat changes and must be hashed again.
    import hashlib
    def digest(path):
        with path.open("rb") as handle:
            return hashlib.file_digest(handle, "sha256").hexdigest()
    if digest(BASE / "check_demo_final_ready.py") != BASE_CHECKER_SHA:
        raise RuntimeError("The accepted baseline verifier changed")
    if digest(BASE / "DEMO_FINAL_READY.json") != BASE_READY_SHA:
        raise RuntimeError("The accepted baseline receipt changed")
    spec = importlib.util.spec_from_file_location("accepted_demo_ready", BASE / "check_demo_final_ready.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


B = load_base()
require, sha, read, inside = B.require, B.sha, B.read, B.inside


def inherited_catalog():
    with contextlib.redirect_stdout(io.StringIO()):
        B.check()
    document = read(BASE / "DEMO_FINAL_READY.json")
    require(set(document["inputs"]) == set(document["frozen_stats"]), "Incomplete accepted file catalog")
    prepared, catalog = {}, {}
    for filename, digest in document["inputs"].items():
        require(B.HEX.fullmatch(digest), "Malformed accepted digest")
        key = str(Path(filename).resolve(strict=True))
        row = (digest, document["frozen_stats"][filename])
        require(key not in prepared or prepared[key] == row, "Conflicting inherited canonical file")
        prepared[key] = row
        catalog[key] = [row]
    return prepared, catalog


def private_fixture_path(path):
    path = Path(path).absolute()
    review = N / "review"
    if not inside(path, review):
        return False
    # Native QA copies mutable profiles/settings and deliberately reseeds save
    # files. Those fixture bytes are reported by the producer, never frozen as
    # user runtime dependencies. Captures, telemetry, recipes remain pinned.
    for parent in path.parents:
        if parent == review or not inside(parent, review):
            break
        if (parent / "project.valwood").is_file():
            return True
    return False


def mutable_path(path):
    path = Path(path).absolute()
    return inside(path, PROJECT) and B.mutable_relative(path.relative_to(PROJECT))


def pin_report(pins, path, passed=True):
    report = read(path)
    require(not passed or report.get("passed") is True, "Required review has not passed: " + str(path))
    pins.add(path)
    for filename, digest in B.hash_pairs(report):
        candidate = Path(filename).absolute()
        if mutable_path(candidate) or private_fixture_path(candidate):
            continue
        pins.add(candidate, digest)
    return report


def verify_native(pins):
    location = N / "runtime-native-v1/READY.json"
    native = read(location)
    require(native.get("ready") is True and native.get("native_only") is True
            and native.get("Windows_work_performed") is False, "Native Linux build is not ready")
    require(native.get("application_units") == 104
            and native.get("full_application_dependency_closure_verified") is True,
            "The complete 104-unit native dependency closure is required")
    binary = N / "runtime-native-v1/native-bin/rpg"
    require(Path(native["binary"]).resolve() == binary.resolve()
            and Path(native["source"]).resolve() == (N / "source-v1").resolve(), "Native build escaped this stage")
    pins.add(location)
    pins.add(binary, native["sha256"])
    plan_path = N / "runtime-native-v1/BUILD_PLAN.json"
    plan = read(plan_path)
    require(plan.get("skip_player") is False and len(plan.get("units", [])) == 104
            and plan.get("native_only") is True and plan.get("Windows_work_performed") is False
            and plan.get("full_application_dependency_closure_verified") is True,
            "Incomplete native compile/link plan")
    require(plan["source_sha256"] == native["source_sha256"], "Build plan and native source closure differ")
    logical = {row["logical_source"] for row in plan["units"]}
    inherited_plan = BASE / "runtime-native-v1/BUILD_PLAN.json"
    pins.add(inherited_plan)
    inherited_units = {row["logical_source"] for row in read(inherited_plan)["units"]}
    require(len(logical) == 104, "Duplicate native translation unit")
    require(logical == inherited_units | {"engine/world_road_traffic.cpp"},
            "Native closure must retain all 103 accepted units and add the road module")
    pins.add(plan_path)
    for filename in ("build_native_candidate.py", "SOURCE_ORIGIN.json"):
        pins.add(N / filename)
    for name, digest in native["source_sha256"].items():
        relative = Path(name)
        require(not relative.is_absolute() and ".." not in relative.parts, "Invalid source dependency path")
        pins.add(N / "source-v1" / relative, digest)
    return native


def verify_project_derivation():
    def metadata(root):
        return {p.relative_to(root): p for p in root.rglob("*")
                if p.is_file() and not p.is_symlink() and not B.mutable_relative(p.relative_to(root))}
    before, after = metadata(BASE / "demo-project"), metadata(PROJECT)
    additions = {Path("opening/road-traffic.settings"), Path("opening/road-traffic.json")}
    require(set(after) == set(before) | additions, "Unexpected added/removed public story metadata")
    permitted = Path("scenery/authored_instances.txt")
    for name, old in before.items():
        if name != permitted:
            require(sha(old) == sha(after[name]), "Unreviewed story metadata change: " + str(name))
    shift = read(N / "review/routes/PARKED_QUEST_CART_SHIFT.json")
    require(shift.get("passed") is True, "The exact three roadside prop shifts must be reviewed")
    # The shift report carries exact old/new file hashes; its entire closure is
    # separately verified, rather than accepting a pathname-only exception.
    pairs = dict(B.hash_pairs(shift))
    for p in (before[permitted], after[permitted]):
        require(pairs.get(str(p)) == sha(p), "Prop-shift review does not pin actual old/new scenery")
    old_links = {p.name: str(p.resolve(strict=True)) for p in (BASE / "demo-project").iterdir() if p.is_symlink()}
    new_links = {p.name: str(p.resolve(strict=True)) for p in PROJECT.iterdir() if p.is_symlink()}
    require(new_links == {**old_links, "road-traffic": str((N / "assets/road_life").resolve())},
            "Story asset URI roots changed outside the new road-traffic binding")
    settings = (PROJECT / "opening/road-traffic.settings").read_text().splitlines()
    require(settings.count("lanes 1.30 2.75") == 1 and sum(row.startswith("point ") for row in settings) == 6,
            "The tested six-point road and 1.30m lanes are required")
    return {"unchanged_metadata_files": len(before) - 1,
            "added": [str(x) for x in sorted(additions)], "changed": [str(permitted)],
            "camp_start_and_existing_dialogue_unchanged": True}


def verify_reviews(pins, native, include_native):
    reviews = []
    for name in REVIEWS + (NATIVE_REVIEWS if include_native else ()):
        report = pin_report(pins, N / name)
        if name in NATIVE_REVIEWS:
            claimed = report.get("native_sha256")
            binary_input = report.get("inputs", {}).get(native["binary"])
            require(claimed == native["sha256"] or binary_input == native["sha256"],
                    "Actual scene/visual review belongs to another native binary: " + name)
        if name == "review/native-road/ROAD_NATIVE_REVIEW.json":
            verify_scene_equivalence(report)
        reviews.append({"path": str(N / name), "sha256": sha(N / name)})
    route = read(N / "review/routes/ROAD_TRAFFIC_ROUTE_REVIEW.json")
    require(route.get("lane_offset_m") == 1.3 and route.get("memory_only_scenery_overrides") is False
            and route.get("all6_loaded_physical_profiles_enclosed") is True
            and route.get("camp_town_protection") is True
            and route.get("rounded_full_body_and_tail_samples", 0) >= 19000,
            "Actual full cart/rider road clearance is incomplete")
    cpu = read(N / "review/road-traffic/ROAD_TRAFFIC_CPU_REVIEW.json")
    profile = cpu.get("actual_source_profiles", {})
    require(profile.get("passed") is True and profile.get("maximum_horizontal_excess_m") == 0
            and profile.get("maximum_top_excess_m") == 0 and len(profile.get("profiles", [])) == 6,
            "Production fitted physical profiles do not enclose actual source meshes")
    gpu = read(N / "review/road-traffic/ROAD_TRAFFIC_GPU_REVIEW.json")
    raw = gpu.get("raw", {})
    require(raw.get("passed") is True and raw.get("actors") == 31 and raw.get("pose_assets") == 82
            and raw.get("gl_errors") == 0 and raw.get("texture_references_after_free") == 0
            and gpu.get("manual_review", {}).get("pending") is False,
            "Production traffic renderer/resource review is incomplete")
    if include_native:
        raw = pin_report(pins, N / "review/native-road/captures/ROAD_NATIVE_RAW.json")
        verify_native_raw(raw)
    return reviews


def verify_scene_equivalence(report):
    # Private save/preferences must remain unfrozen, but the scene tested by
    # native QA must be the exact public scene. Compare its historical reported
    # input hashes to public bytes without making the disposable fixture a
    # future launch dependency.
    private = N / "review/native-road/private-project"
    historical = report.get("inputs", {})
    for public in PROJECT.rglob("*"):
        if not public.is_file() or public.is_symlink():
            continue
        relative = public.relative_to(PROJECT)
        if B.mutable_relative(relative):
            continue
        require(historical.get(str(private / relative)) == sha(public),
                "Native fixture/public scene metadata differs: " + str(relative))
    bindings = report.get("private_asset_root_bindings", {})
    for public in PROJECT.iterdir():
        if public.is_symlink():
            require(bindings.get(str(private / public.name)) == str(public.resolve(strict=True)),
                    "Native fixture/public shared asset root differs: " + public.name)


def verify_native_raw(raw):
    require(raw.get("passed") is True and raw.get("actors") == 31 and raw.get("captures") == 3
            and raw.get("actual_native_scene") is True
            and raw.get("ordinary_continue_and_sdl_controls") is True
            and raw.get("user_session_untouched") is True,
            "Actual ordinary-control road scene proof is incomplete")


def build_review(include_native):
    prepared, catalog = inherited_catalog()
    pins = B.Pins(prepared)
    pins.add(BASE / "check_demo_final_ready.py", BASE_CHECKER_SHA)
    pins.add(BASE / "DEMO_FINAL_READY.json", BASE_READY_SHA)
    native = verify_native(pins)
    derivation = verify_project_derivation()
    manifest = pin_report(pins, N / "assets/road_life/ROAD_LIFE_ASSETS.json", passed=False)
    require(manifest.get("schema") == "valwood.owned-road-life.assets.v1"
            and manifest.get("producer_complete") is True, "Owned road assets are incomplete")
    reviews = verify_reviews(pins, native, include_native)
    for path in (Path(__file__), N / "launch_demo.sh", ROOT / "Launch_Valwood_Busy_Road_Demo.sh"):
        pins.add(path)
    routes, bindings, fallback = {}, [], {}
    B.inventory_project(PROJECT, pins, catalog, routes, bindings, fallback)
    # project_runtime changes cwd to the selected project before rendering.
    # Its shaders/ and relative includes are already in the project inventory.
    require((PROJECT / "shaders/world_terrain.vs.glsl").is_file()
            and (PROJECT / "shaders/world_foliage.vs.glsl").is_file(),
            "Actual project-local terrain/model shaders are unavailable")
    for path, target in routes.items():
        require(Path(path).is_symlink() and str(Path(path).resolve(strict=True)) == target, "Changed asset route")
    pins.unchanged()
    return pins, native, reviews, routes, bindings, fallback, derivation


def publish():
    require(not READY.exists(), "Published readiness is immutable; use --check-only for an accepted release")
    pins, native, reviews, routes, bindings, fallback, derivation = build_review(True)
    document = {
        "schema": "valwood.busy-road.ready.v1", "ready": True,
        "native_only": True, "Windows_work_performed": False,
        "native_sha256": native["sha256"], "application_units": 104,
        "inputs": pins.hashes, "frozen_stats": pins.stats,
        "asset_routes": routes, "asset_root_bindings": bindings,
        "required_reviews": reviews, "project_derivation": derivation,
        "accepted_baseline": {"path": str(BASE / "DEMO_FINAL_READY.json"), "sha256": BASE_READY_SHA},
        "publication_hash_bytes": pins.hashed_bytes,
        "accepted_large_file_cache_hits": pins.accepted_cache_files,
        "newly_pinned_owned_files_by_project": fallback,
        "actors": 31, "user_session_untouched": True,
        "hardware_frame_rate_verified": False, "full_new_journey_playthrough_verified": False,
        "mutable_project_policy": {"projects": [str(PROJECT)],
            "excluded": ["saves/**", "settings/**", "graphics/controller preferences", "optional imported narration", "editor recovery/layout/locks"],
            "freshness_is_not_a_future_launch_requirement": True},
        "limits": [
            "Ambient authored road loops, player yielding and quest exclusion zones; not general-world navigation or rideable mounts.",
            "Actual bounded turn/walk/pause/resume scene proof plus source collision sweeps; not a new full story playthrough or hardware/controller performance certification.",
            "The original campsite-facing arrival and three story speakers remain unchanged; road traffic is visible after turning toward the road.",
            "If the quest encounter remains uncleared for several minutes, campsite-side traffic can queue; clearing the encounter releases it.",
            "Accepted large-file SHA values are reused only for exact device/inode/size/mtime/ctime matches; changed files must still match their expected SHA.",
        ],
    }
    temporary = READY.with_suffix(".tmp")
    temporary.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    temporary.replace(READY)
    print(json.dumps({"ready": True, "file_pins": len(pins.hashes), "native_sha256": native["sha256"],
                      "hashed_bytes": pins.hashed_bytes, "inherited_large_file_cache_hits": pins.accepted_cache_files}))


def check():
    ready = read(READY)
    require(ready.get("schema") == "valwood.busy-road.ready.v1" and ready.get("ready") is True
            and ready.get("native_only") is True and ready.get("application_units") == 104,
            "The busy-road demo has not completed review")
    require(ready.get("accepted_baseline", {}).get("sha256") == BASE_READY_SHA, "Unexpected accepted ancestry")
    hashes, stamps = ready["inputs"], ready["frozen_stats"]
    require(set(hashes) == set(stamps) and len(hashes) <= B.MAX_PINS, "Incomplete readiness closure")
    for filename, expected in hashes.items():
        require(B.HEX.fullmatch(expected), "Malformed readiness hash")
        before = B.snapshot(filename)
        if before[2] < B.SMALL_FILE or before != stamps[filename]:
            require(sha(filename) == expected and B.snapshot(filename) == before,
                    "Reviewed dependency changed: " + filename)
    for filename, target in ready["asset_routes"].items():
        require(Path(filename).is_symlink() and str(Path(filename).resolve(strict=True)) == target,
                "Reviewed asset URI root changed: " + filename)
    print("Valwood busy-road demo ready: reviewed walkers, riders and carts; independent story progress.")


def self_test():
    # Run the inherited cache-tamper tests, then test the new actual-scene gate:
    # partial captures or merely synthetic module evidence cannot publish.
    with contextlib.redirect_stdout(io.StringIO()):
        B.self_test()
    good = {"passed": True, "actors": 31, "captures": 3, "actual_native_scene": True,
            "ordinary_continue_and_sdl_controls": True, "user_session_untouched": True}
    verify_native_raw(good)
    checks = 1
    for key, value in (("passed", False), ("actors", 30), ("captures", 2),
                       ("actual_native_scene", False), ("ordinary_continue_and_sdl_controls", False),
                       ("user_session_untouched", False)):
        try:
            verify_native_raw({**good, key: value})
        except RuntimeError:
            checks += 1
        else:
            raise RuntimeError("Incomplete actual scene proof was accepted: " + key)
    with tempfile.TemporaryDirectory(dir=N / "review", prefix="ready-fixture-") as folder:
        project = Path(folder) / "private-project"
        project.mkdir()
        (project / "project.valwood").write_text("private fixture")
        require(private_fixture_path(project / "graphics.settings"), "Private QA preferences would be launch-pinned")
        require(not private_fixture_path(Path(folder) / "capture.png"), "Actual capture evidence was excluded")
        checks += 2
    private = N / "review/native-road/private-project"
    historical = {str(private / p.relative_to(PROJECT)): sha(p) for p in PROJECT.rglob("*")
                  if p.is_file() and not p.is_symlink() and not B.mutable_relative(p.relative_to(PROJECT))}
    bindings = {str(private / p.name): str(p.resolve(strict=True)) for p in PROJECT.iterdir() if p.is_symlink()}
    scene = {"inputs": historical, "private_asset_root_bindings": bindings}
    verify_scene_equivalence(scene)
    checks += 1
    for bad in ({**scene, "inputs": {}},
                {**scene, "inputs": {**historical, next(iter(historical)): "0" * 64}},
                {**scene, "private_asset_root_bindings": {}}):
        try:
            verify_scene_equivalence(bad)
        except RuntimeError:
            checks += 1
        else:
            raise RuntimeError("Native/public scene mismatch was accepted")
    print(json.dumps({"passed": True, "new_scene_scope_checks": checks,
                      "inherited_cache_tamper_tests_passed": True, "READY_written": False, "GPU_used": False}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--write", action="store_true", help="Finalize only after actual native and visual acceptance")
    group.add_argument("--check-only", action="store_true")
    group.add_argument("--preflight", action="store_true", help="Check all current CPU/asset/native-build inputs without requiring pending scene reviews")
    group.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.preflight:
        pins, native, *_ = build_review(False)
        print(json.dumps({"passed": True, "READY_written": False, "native_sha256": native["sha256"],
                          "pins": len(pins.hashes), "hash_bytes": pins.hashed_bytes,
                          "inherited_large_file_cache_hits": pins.accepted_cache_files}))
    elif args.write:
        publish()
        check()
    else:
        check()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
