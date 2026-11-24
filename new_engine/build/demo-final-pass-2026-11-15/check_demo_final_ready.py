#!/usr/bin/env python3
"""Publish/check the reviewed final Linux demo; never freezes player progress."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import stat as stat_module
import sys
import tempfile

N = Path(__file__).resolve().parent
ENGINE = N.parents[1]
ROOT = ENGINE.parent
READY = N / "DEMO_FINAL_READY.json"
ASSET_CACHE = N / "review/DEMO_FINAL_ASSET_CACHE.json"
SMALL_FILE = 1024 * 1024
JSON_LIMIT = 128 * 1024 * 1024
MAX_PINS = 100000
HEX = re.compile(r"^[0-9a-f]{64}$")
PROJECTS = (N / "demo-project", N / "arena-project")
ACCEPTED = (
    N.parent / "npc-cinematic-intro-2026-11-15/NPC_INTRO_READY.json",
    N.parent / "cinematic-studio-2026-11-15/CINEMATIC_STUDIO_READY.json",
)
REVIEWS = (
    "review/title-fit/TITLE_FIT_CPU_REVIEW.json",
    "review/story-interactions/DEMO_PROJECT_PREPARATION_REVIEW.json",
    "review/story-interactions/STORY_DEFAULT_PROFILE_CPU_REVIEW.json",
    "review/story-interactions/STORY_FLOW_CPU_REVIEW.json",
    "review/story-interactions/STORY_MOUSE_OWNER_CPU_REVIEW.json",
    "review/equipment-controls/ARENA_PROJECT_PREPARATION_REVIEW.json",
    "review/equipment-controls/EQUIPMENT_CONTROLS_CPU_REVIEW.json",
    "review/equipment-controls/EQUIPMENT_NATIVE_REVIEW.json",
    "review/story-interactions/FRESH_DEMO_STARTUP_NATIVE_REVIEW.json",
    "review/FINAL_PASS_VISUAL_REVIEW.json",
    "review/FINAL_PASS_SCOPE_REVIEW.json",
    "review/BUILD_RECIPE_REVIEW.json",
)
PUBLIC_FILES = (
    N / "launch_demo.sh", N / "launch_arena.sh", N / "FINAL_PASS_NOTES.txt",
    ROOT / "Launch_Valwood_Demo.sh", ROOT / "Launch_Valwood_Arena_Final.sh",
    N / "DEMO_FINAL_PUBLICATION_CHECKLIST.md", Path(__file__).resolve(),
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def snapshot(path):
    value = Path(path).stat()
    require(stat_module.S_ISREG(value.st_mode), "Not a regular reviewed file: " + str(path))
    return [value.st_dev, value.st_ino, value.st_size, value.st_mtime_ns, value.st_ctime_ns]


def read(path):
    path = Path(path)
    require(path.is_file() and path.stat().st_size <= JSON_LIMIT,
            "Missing/oversized review: " + str(path))
    return json.loads(path.read_text())


def inside(path, root):
    try:
        Path(path).relative_to(root)
        return True
    except ValueError:
        return False


def mutable_relative(relative):
    """Only user state/optional imported media, never world/rig definitions."""
    p = Path(relative)
    parts = p.parts
    if not parts:
        return False
    if parts[0] in {"saves", "settings", "recovery", "autosaves"}:
        return True
    if len(parts) >= 2 and parts[:2] == ("opening", "npc_intro"):
        return True
    name = p.name
    if name in {"graphics.settings", "combat.settings", "preview.settings", "controller.settings",
                "arena_player_character.cfg", "arena_orc_character.cfg", "studio-layout.ini", "imgui.ini"}:
        return True
    return (name.endswith((".recovery", ".autosave", ".lock", ".tmp", ".bak"))
            or ".recovery." in name or name.startswith(".valwood-instance"))


def mutable_project_path(path):
    path = Path(path).absolute()
    return any(inside(path, root) and mutable_relative(path.relative_to(root)) for root in PROJECTS)


def hash_pairs(value):
    """All actual path/hash shapes, including sibling path+sha256 receipts."""
    if isinstance(value, dict):
        if isinstance(value.get("path"), str) and HEX.fullmatch(str(value.get("sha256", ""))):
            if Path(value["path"]).is_absolute():
                yield value["path"], value["sha256"]
        for key, child in value.items():
            if isinstance(key, str) and Path(key).is_absolute() and isinstance(child, str) and HEX.fullmatch(child):
                yield key, child
            else:
                yield from hash_pairs(child)
    elif isinstance(value, list):
        for child in value:
            yield from hash_pairs(child)


class Pins:
    def __init__(self, prepared_cache=None):
        self.hashes = {}
        self.stats = {}
        self.hashed_bytes = 0
        self.accepted_cache_files = 0
        self.prepared_cache = prepared_cache or {}

    def add(self, path, expected=None, trusted_stats=()):
        path = Path(path).resolve(strict=True)
        key = str(path)
        if key in self.hashes:
            require(expected is None or expected == self.hashes[key], "Conflicting reviewed hash: " + key)
            return
        require(len(self.hashes) < MAX_PINS, "Review dependency count exceeds the bounded gate")
        before = snapshot(path)
        prepared = self.prepared_cache.get(key)
        if prepared:
            # A new inode/stat must still match the precomputed bytes. Without
            # retaining this expected hash, a snapshot-only file could change
            # after native QA and silently become a new publication baseline.
            require(expected is None or prepared[0] == expected, "Prepared/inherited hash conflict: " + key)
            expected = prepared[0]
            if prepared[1] == before:
                trusted_stats = [*trusted_stats, before]
        if expected is not None:
            require(HEX.fullmatch(expected), "Invalid expected SHA256: " + key)
        if expected is not None and before[2] >= SMALL_FILE and before in trusted_stats:
            digest = expected
            self.accepted_cache_files += 1
        else:
            digest = sha(path)
            self.hashed_bytes += before[2]
        require(expected is None or digest == expected, "Reviewed file changed: " + key)
        require(snapshot(path) == before, "File changed while being verified: " + key)
        self.hashes[key] = digest
        self.stats[key] = before

    def unchanged(self):
        for path, before in self.stats.items():
            require(snapshot(path) == before, "File changed during publication: " + path)


def accepted_catalog(pins):
    """Use accepted evidence only for files actually reached by new projects."""
    catalog = {}
    accepted_receipts = []
    for path in ACCEPTED:
        document = read(path)
        require(document.get("ready") is True and document.get("native_only") is True,
                "Inherited accepted asset receipt is not ready: " + str(path))
        pins.add(path)
        accepted_receipts.append({"path": str(path), "sha256": sha(path)})
        for key, stat_key in (("inputs", "frozen_stats"), ("arena_inputs", "arena_frozen_stats")):
            for filename, digest in document.get(key, {}).items():
                require(HEX.fullmatch(digest), "Malformed inherited hash: " + filename)
                real = str(Path(filename).resolve())
                rows = catalog.setdefault(real, [])
                row = (digest, document.get(stat_key, {}).get(filename))
                if row not in rows:
                    rows.append(row)
    return catalog, accepted_receipts


def inventory_project(project, pins, catalog, routes, bindings, fallback_counts):
    """Keep URI roots in place; pin resolved files and every symlink route."""
    require(project.is_dir() and not project.is_symlink(), "Private project root is unavailable: " + str(project))
    seen_directories = set()

    def recurse(folder, shared_asset=False):
        real_folder = folder.resolve(strict=True)
        # A cycle is an error. A second alias of a shared directory is harmless
        # but must still have its route checked before skipping repeated work.
        marker = (str(folder), str(real_folder))
        require(marker not in seen_directories, "Cyclic asset directory: " + str(folder))
        seen_directories.add(marker)
        for child in sorted(folder.iterdir()):
            relative = child.relative_to(project)
            if mutable_relative(relative):
                continue
            if child.is_symlink():
                require(child.exists(), "Broken runtime asset link: " + str(child))
                target = str(child.resolve(strict=True))
                routes[str(child)] = target
                if len(relative.parts) == 1:
                    bindings.append({"project": str(project), "path": str(child), "target": target})
            if child.is_dir():
                # Parent symlinks may point at accepted source asset trees. Never
                # replace these with a different common directory: glTF image
                # references depend on their actual original relative roots.
                ancestry = {str(p.resolve()) for p in folder.parents if inside(p, project)}
                require(str(child.resolve()) not in ancestry | {str(real_folder)}, "Asset directory loop: " + str(child))
                recurse(child, shared_asset or child.is_symlink())
            elif child.is_file():
                actual = child.resolve(strict=True)
                rows = catalog.get(str(actual), [])
                if rows:
                    digests = {row[0] for row in rows}
                    require(len(digests) == 1, "Conflicting inherited asset ancestry: " + str(child))
                    pins.add(actual, next(iter(digests)), [row[1] for row in rows if row[1] is not None])
                else:
                    # A matching logical filename in a different Intro copy
                    # is not exact ancestry: owned scene collision/pose assets
                    # legitimately differ. Snapshot the actual file once; do
                    # not claim it inherits another path's historical hash.
                    pins.add(actual)
                    fallback_counts[project.name] = fallback_counts.get(project.name, 0) + 1
            else:
                raise RuntimeError("Unsupported runtime project entry: " + str(child))

    recurse(project)


def external_runtime_assets(pins, catalog, routes, bindings, fallback_counts):
    """The arena launcher opts into these assets outside its project links."""
    equipment = N.parent / "cinematic-studio-2026-11-15/equipment-assets"
    shader_root = N.parent / "arena-animation-polish-2026-11-15/shaders"
    hero = read(equipment / "HERO_ASSET.json")
    sword = Path(hero["path"])
    require(sword.is_absolute() and inside(sword.resolve(), equipment.resolve()),
            "Arena hero asset escaped its reviewed equipment generation")
    require(HEX.fullmatch(hero["sha256"]), "Invalid arena hero SHA")
    pins.add(sword, hero["sha256"])
    lines = (equipment / "loadouts.manifest").read_text().splitlines()
    require(len(lines) == 4 and shlex.split(lines[0]) == ["VALWOOD_SKELETAL_LOADOUTS", "1"],
            "Arena must retain all four reviewed melee rigs")
    for number, prefix, line in zip((2, 3, 4), ("Shield_", "Axe_", "Staff_"), lines[1:]):
        fields = shlex.split(line)
        require(len(fields) == 3 and fields[0] == str(number) and fields[2] == prefix,
                "Invalid/out-of-order external arena loadout")
        asset = Path(fields[1])
        require(asset.is_absolute() and inside(asset.resolve(), equipment.resolve()) and asset.is_file(),
                "Missing or escaped arena weapon rig")
    for required in ("world_skeletal.vs.glsl", "rig_first_person.fs.glsl"):
        require((shader_root / required).is_file(), "Missing arena first/third-person shader: " + required)
    for root in (equipment, shader_root):
        actual = root.resolve(strict=True)
        if root.is_symlink():
            routes[str(root)] = str(actual)
        # Include the entire small shader/include generation, not just the two
        # top-level files: preprocessing resolves their relative dependencies.
        inventory_project(actual, pins, catalog, routes, bindings, fallback_counts)


def review_fixture_path(path):
    """Captured private project state is not a public runtime dependency."""
    path = Path(path)
    root = N / "review"
    if not inside(path, root):
        return False
    for parent in path.parents:
        if parent == root or not inside(parent, root):
            break
        if (parent / "project.valwood").is_file():
            return True
    return False


def verify_native(pins):
    path = N / "runtime-native-v1/READY.json"
    native = read(path)
    require(native.get("ready") is True and native.get("native_only") is True
            and native.get("Windows_work_performed") is False,
            "Final native build is not complete")
    require(native.get("application_units") == 103
            and native.get("full_application_dependency_closure_verified") is True,
            "Final native build lacks its complete 103-unit dependency closure")
    binary = N / "runtime-native-v1/native-bin/rpg"
    require(Path(native["binary"]).resolve() == binary.resolve()
            and Path(native["source"]).resolve() == (N / "source-v1").resolve(),
            "Native receipt points outside the new candidate")
    pins.add(binary, native["sha256"])
    pins.add(path)
    plan_path = N / "runtime-native-v1/BUILD_PLAN.json"
    plan = read(plan_path)
    require(plan.get("skip_player") is False and len(plan.get("units", [])) == 103
            and plan.get("native_only") is True
            and plan.get("full_application_dependency_closure_verified") is True,
            "Final compile/link plan is incomplete")
    require(plan["source_sha256"] == native["source_sha256"], "Compile plan/source freeze differs from the native receipt")
    logical = [row["logical_source"] for row in plan["units"]]
    require(len(set(logical)) == 103, "Duplicate native translation unit")
    pins.add(plan_path)
    pins.add(N / "build_native_candidate.py")
    pins.add(N / "SOURCE_ORIGIN.json")
    for name, digest in native["source_sha256"].items():
        relative = Path(name)
        require(not relative.is_absolute() and ".." not in relative.parts,
                "Invalid native source path: " + name)
        pins.add(N / "source-v1" / relative, digest)
    return native


def verify_reviews(pins, native):
    reviewed = []
    omitted_external = 0
    for name in REVIEWS:
        path = N / name
        report = read(path)
        require(report.get("passed") is True, "Final review is incomplete: " + name)
        if name.endswith(("EQUIPMENT_NATIVE_REVIEW.json", "FRESH_DEMO_STARTUP_NATIVE_REVIEW.json", "FINAL_PASS_VISUAL_REVIEW.json")):
            claimed = report.get("native_sha256")
            input_hash = report.get("inputs", {}).get(native["binary"])
            require(claimed == native["sha256"] or input_hash == native["sha256"],
                    "Final native review uses a different binary: " + name)
        if name.endswith("FRESH_DEMO_STARTUP_NATIVE_REVIEW.json"):
            require(report.get("manual_capture_review_pending") is False,
                    "Fresh title capture has not been reviewed")
        pins.add(path)
        reviewed.append({"path": str(path), "sha256": sha(path)})
        for filename, digest in hash_pairs(report):
            candidate = Path(filename).absolute()
            if mutable_project_path(candidate) or review_fixture_path(candidate):
                continue
            if inside(candidate, N):
                # New QA raw results/producers are immutable evidence. User
                # demo/arena preferences and progress were filtered above.
                pins.add(candidate, digest)
            else:
                # Historical designer metadata/public launchers are evidence
                # of the earlier review, not runtime dependencies of this demo.
                # Shared runtime assets are independently inventoried below.
                omitted_external += 1
    return reviewed, omitted_external


def prepared_asset_cache():
    if not ASSET_CACHE.is_file():
        return {}
    document = read(ASSET_CACHE)
    producer = document.get("checker_sha256")
    revision = document.get("consumer_compatibility", {})
    compatible = producer == sha(__file__)
    if revision:
        original_source = revision.get("producer_source", "")
        compatible = (revision.get("producer_sha256") == producer
                      and hashlib.sha256(original_source.encode()).hexdigest() == producer
                      and revision.get("consumer_sha256") == sha(__file__)
                      and revision.get("all_prepared_stat_records_rechecked") is True
                      and revision.get("prepared_files") == len(document.get("inputs", {})))
    require(document.get("passed") is True
            and document.get("schema") == "valwood.demo-final.prepared-assets.v1"
            and compatible,
            "Prepared asset cache is stale; rerun --prepare-assets before publication")
    for row in document["accepted_asset_receipts"]:
        require(sha(row["path"]) == row["sha256"], "Accepted asset ancestry changed after precomputation")
    require(set(document["inputs"]) == set(document["frozen_stats"])
            and len(document["inputs"]) <= MAX_PINS, "Malformed prepared asset cache")
    for digest in document["inputs"].values():
        require(HEX.fullmatch(digest), "Malformed prepared asset digest")
    return {path: (digest, document["frozen_stats"][path]) for path, digest in document["inputs"].items()}


def prepare_assets():
    require(not READY.exists(), "Do not overwrite preparation evidence after READY publication")
    pins = Pins()
    catalog, accepted = accepted_catalog(pins)
    routes, bindings, fallback = {}, [], {}
    for project in PROJECTS:
        inventory_project(project, pins, catalog, routes, bindings, fallback)
    external_runtime_assets(pins, catalog, routes, bindings, fallback)
    pins.unchanged()
    document = {
        "schema": "valwood.demo-final.prepared-assets.v1", "passed": True,
        "READY_written": False, "checker_sha256": sha(__file__),
        "inputs": pins.hashes, "frozen_stats": pins.stats,
        "asset_routes": routes, "asset_root_bindings": bindings,
        "accepted_asset_receipts": accepted,
        "hash_bytes": pins.hashed_bytes, "accepted_cache_files": pins.accepted_cache_files,
        "newly_pinned_owned_files_by_project": fallback,
    }
    temporary = ASSET_CACHE.with_suffix(".tmp")
    temporary.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    temporary.replace(ASSET_CACHE)
    print(json.dumps({"passed": True, "READY_written": False, "asset_pins": len(pins.hashes),
                      "hash_bytes": pins.hashed_bytes, "cache": str(ASSET_CACHE)}))


def publish():
    pins = Pins(prepared_asset_cache())
    native = verify_native(pins)
    reviews, omitted = verify_reviews(pins, native)
    for path in PUBLIC_FILES:
        pins.add(path)
    catalog, accepted = accepted_catalog(pins)
    if ASSET_CACHE.is_file():
        pins.add(ASSET_CACHE)
    routes, bindings, fallback = {}, [], {}
    for project in PROJECTS:
        inventory_project(project, pins, catalog, routes, bindings, fallback)
    external_runtime_assets(pins, catalog, routes, bindings, fallback)
    require(len(routes) <= MAX_PINS, "Too many linked asset routes")
    for path, target in routes.items():
        require(Path(path).is_symlink() and str(Path(path).resolve(strict=True)) == target,
                "Asset link changed during publication: " + path)
    pins.unchanged()
    report = {
        "schema": "valwood.demo-final.ready.v1", "ready": True,
        "native_only": True, "Windows_work_performed": False,
        "native_sha256": native["sha256"], "application_units": 103,
        "inputs": pins.hashes, "frozen_stats": pins.stats,
        "asset_routes": routes, "asset_root_bindings": bindings,
        "accepted_asset_receipts": accepted, "required_reviews": reviews,
        "publication_hash_bytes": pins.hashed_bytes,
        "accepted_large_file_cache_hits": pins.accepted_cache_files,
        "newly_pinned_owned_files_by_project": fallback,
        "inherited_nonruntime_evidence_not_launch_pinned": omitted,
        "mutable_project_policy": {
            "projects": [str(p) for p in PROJECTS],
            "excluded": ["saves/**", "settings/**", "graphics/combat/preview/controller.settings",
                         "arena character choices", "editor layout/recovery/locks", "opening/npc_intro/**"],
            "first_launch_freshness_is_not_a_future_launch_requirement": True,
        },
        "final_user_narration_supplied": False,
        "new_full_native_journey_playthrough_verified": False,
        "hardware_frame_rate_verified": False,
        "limits": [
            "New native startup, deterministic input/equipment, CPU story fixes and manually reviewed captures are the final-pass acceptance scope.",
            "This gate preserves the accepted asset generations; it does not claim new AAA art, source start/stop animation, final supplied narration or hardware performance.",
            "Large immutable files reuse a verified SHA only while full device/inode/size/mtime/ctime matches the published snapshot; changed files are hashed again.",
            "Owned runtime files absent from exact resolved inherited evidence are new byte snapshots, not claims that logically similar historical files were identical.",
        ],
    }
    temporary = READY.with_suffix(".tmp")
    temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    temporary.replace(READY)
    print(f"Published final demo: {len(pins.hashes)} file pins, {len(routes)} asset routes.")


def check():
    ready = read(READY)
    require(ready.get("schema") == "valwood.demo-final.ready.v1" and ready.get("ready") is True
            and ready.get("native_only") is True and ready.get("application_units") == 103,
            "Final demo review is not ready")
    hashes, stamps = ready["inputs"], ready["frozen_stats"]
    require(len(hashes) <= MAX_PINS and set(hashes) == set(stamps), "Invalid readiness input closure")
    for filename, expected in hashes.items():
        require(HEX.fullmatch(expected), "Malformed readiness hash: " + filename)
        before = snapshot(filename)
        if before[2] < SMALL_FILE or before != stamps[filename]:
            require(sha(filename) == expected and snapshot(filename) == before,
                    "Reviewed dependency changed: " + filename)
    for filename, target in ready["asset_routes"].items():
        path = Path(filename)
        require(path.is_symlink() and str(path.resolve(strict=True)) == target,
                "Reviewed runtime asset root changed: " + filename)
    print("Valwood final demo ready: independent story progress and reviewed combat practice.")


def self_test():
    """Exercise cache invalidation, real symlink routes and progress exclusions."""
    checks = 0
    with tempfile.TemporaryDirectory(prefix="valwood-final-ready-") as directory:
        root = Path(directory)
        asset = root / "asset.bin"
        asset.write_bytes(b"a" * SMALL_FILE)
        expected = sha(asset)
        verified = snapshot(asset)
        pins = Pins()
        pins.add(asset, expected, [verified])
        require(pins.accepted_cache_files == 1 and pins.hashed_bytes == 0, "Verified large-file cache did not apply")
        checks += 1
        asset.write_bytes(b"b" * SMALL_FILE)
        try:
            Pins().add(asset, expected, [verified])
        except RuntimeError:
            checks += 1
        else:
            raise RuntimeError("Changed same-size asset bypassed SHA verification")
        copy = root / "copy.bin"
        copy.write_bytes(b"b" * SMALL_FILE)
        pins = Pins()
        pins.add(copy, sha(asset), [snapshot(asset)])
        require(pins.hashed_bytes == SMALL_FILE and pins.accepted_cache_files == 0,
                "Different inode trusted only size/mtime")
        checks += 1
        cached = {str(copy): (sha(copy), snapshot(copy))}
        copy.write_bytes(b"c" * SMALL_FILE)
        try:
            Pins(cached).add(copy)
        except RuntimeError:
            checks += 1
        else:
            raise RuntimeError("Snapshot-only asset changed after precomputation without rejection")
        copy.write_bytes(b"b" * SMALL_FILE)
        pins = Pins(cached)
        pins.add(copy)
        require(pins.hashed_bytes == SMALL_FILE, "Changed stat record reused cached bytes without verification")
        checks += 1
        alias = root / "alias"
        alias.symlink_to(asset)
        require(alias.is_symlink() and alias.resolve() == asset, "Asset root route failed")
        alias.unlink()
        alias.symlink_to(copy)
        require(alias.resolve() != asset, "Changed symlink route was not distinguishable")
        checks += 1
        for name in ("saves/player.progress", "graphics.settings", "settings/controller.cfg", "studio-layout.ini", "world.studio.recovery", "opening/npc_intro/gen/frames.pack"):
            require(mutable_relative(name), "Mutable player path would be frozen: " + name)
            checks += 1
        for name in ("world.studio", "opening.config", "player-profile.config", "models/body.glb", "opening/first-journey.settings"):
            require(not mutable_relative(name), "Runtime definition incorrectly excluded: " + name)
            checks += 1
        rows = list(hash_pairs({"inputs_sha256": {str(asset): expected}, "child": {"path": str(copy), "sha256": expected}, "outputs": {str(alias): expected}}))
        require(len(rows) == 3, "Transitive receipt path/hash shapes were dropped")
        checks += 1
    print(json.dumps({"passed": True, "checks": checks, "READY_written": False, "GPU_used": False}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="Publish only after every final native/CPU/visual review passes")
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--arena", action="store_true", help="Compatibility flag; the final receipt always covers both projects")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--prepare-assets", action="store_true", help="Verify/cache actual shared assets once without publishing READY")
    args = parser.parse_args()
    require(sum((args.write, args.self_test, args.prepare_assets)) <= 1, "Choose publication, preparation or self-test")
    if args.self_test:
        self_test()
        return
    if args.prepare_assets:
        prepare_assets()
        return
    if args.write:
        publish()
    check()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
