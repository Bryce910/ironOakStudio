#!/usr/bin/env python3
"""Build the merged Linux cinematic/editor source with a verified 104-unit closure.

Uses only matching source-and-flags cached objects; leaves canonical and Windows outputs alone.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import shlex
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True
WORK = Path(__file__).resolve().parent
ROOT = WORK.parents[1]
BASE = ROOT / "build/opening-finish-2026-11-18"
SOURCE = WORK / "source-v1"
RUNTIME = WORK / "runtime-native-v1"
BASE_PLAN = BASE / "runtime-native-v1/BUILD_PLAN.json"
LIBRARY = ROOT / "build/physics/libJolt.a"
PHYSICS_SHA = "47dd97e1d9f570e4e7b3ea45234db21c205dbd2460e9d816738421caf8c316d8"
CXX = "/usr/bin/x86_64-linux-gnu-g++-13"
CC = "/usr/bin/x86_64-linux-gnu-gcc-13"
ADAPTER = "engine/world_skeletal_hero.cpp"
LOADOUT_ADAPTER = "engine/world_skeletal_hero_loadouts.cpp"


def sha(path):
    with Path(path).open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def inventory(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob("*")) if p.is_file()}


def prepare_source():
    if not SOURCE.exists():
        shutil.copytree(BASE / "source-v1", SOURCE)
        copied = inventory(SOURCE)
        original = inventory(BASE / "source-v1")
        if copied != original:
            raise RuntimeError("Source copy did not match surviving baseline")
        write_json(WORK / "SOURCE_ORIGIN.json", dict(
            schema="valwood.modular-knight.source-origin.v1", source=str(SOURCE),
            baseline=str(BASE / "source-v1"), baseline_plan_sha256=sha(BASE_PLAN),
            baseline_sha256=original, canonical_source_edited=False))
    origin = WORK / "SOURCE_ORIGIN.json"
    if not origin.is_file():
        raise RuntimeError("Existing private source lacks its origin receipt")
    # Private copies close the two exceptional include roots. No fallback to
    # canonical engine headers is allowed during the application compilation.
    jolt = SOURCE / "third_party/jolt"
    if not jolt.exists():
        shutil.copytree(ROOT / "third_party/jolt", jolt)
    metadata = Path("projects/valwood_world/models/castle_dragons/dragon_metadata.inc")
    target = SOURCE / metadata
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / metadata, target)
    if sha(LIBRARY) != PHYSICS_SHA:
        raise RuntimeError("Pinned native Jolt library changed")


def logical_object(value):
    path = Path(value)
    object_root = BASE / 'runtime-native-v1/native-objects'
    if path.is_absolute() and path.is_relative_to(object_root):
        logical = path.relative_to(object_root).with_suffix('')
        if (SOURCE / logical).is_file():
            return str(logical)
    if path.is_absolute():
        name = path.name[:-2]
        matches = [p for p in (SOURCE / "engine" / name, SOURCE / name) if p.is_file()]
    else:
        stem = str(path.with_suffix(""))
        candidates = [SOURCE / stem] if stem.endswith((".c", ".cpp")) else [SOURCE / (stem + x) for x in (".c", ".cpp")]
        matches = [p for p in candidates if p.is_file()]
    if len(matches) != 1:
        raise RuntimeError("Ambiguous/missing private unit for " + value)
    return str(matches[0].relative_to(SOURCE))


def plan(skip_player=False):
    prepare_source()
    baseline = json.loads(BASE_PLAN.read_text())
    sdl = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "sdl2", "SDL2_mixer", "SDL2_ttf"], text=True))
    flags = ["-O2", "-Wall", "-Wextra", "-I" + str(SOURCE / "third_party/imgui"),
             "-I" + str(SOURCE / "third_party/imgui/backends"), "-I" + str(SOURCE / "engine"),
             "-I" + str(SOURCE), *sdl, "-DENGINE_ENABLE_IMGUI=1"]
    units = []
    link = [CXX]
    seen = set()
    for arg in baseline["link"][1:]:
        if not arg.endswith(".o"):
            continue
        name = logical_object(arg)
        # The accepted Gnur closure already contains the two player adapters.
        # Add those once below, preserving all 101 application units in order.
        if name in {ADAPTER, LOADOUT_ADAPTER, "engine/world_road_traffic.cpp"}:
            continue
        if name in seen:
            raise RuntimeError("Repeated link unit " + name)
        seen.add(name)
        units.append(name)
    if len(units) != 101:
        raise RuntimeError("Unexpected baseline closure size")
    if (SOURCE / ADAPTER).is_file():
        units.append(ADAPTER)
    elif not skip_player:
        raise RuntimeError("Wait for the new player adapter before final build")
    if (SOURCE / LOADOUT_ADAPTER).is_file():
        units.append(LOADOUT_ADAPTER)
    elif not skip_player:
        raise RuntimeError("Wait for the weapon rig adapter before final build")
    if not (SOURCE / "engine/world_road_traffic.cpp").is_file():
        raise RuntimeError("Wait for the new road traffic implementation before final build")
    units.append("engine/world_road_traffic.cpp")
    rows = []
    for name in units:
        if skip_player and name in {ADAPTER, LOADOUT_ADAPTER, "engine/world_viewer.cpp"}:
            continue
        obj = RUNTIME / "native-objects" / (name + ".o")
        dep = obj.with_suffix(".d")
        cc = CC if name.endswith(".c") else CXX
        extra = []
        if name == "engine/world_physics.cpp":
            extra = ["-DJPH_DOUBLE_PRECISION", "-DNDEBUG", "-fno-rtti", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-I" + str(SOURCE / "third_party/jolt")]
        command = [cc, "-std=c99" if name.endswith(".c") else "-std=c++17", *flags, *extra,
                   "-MMD", "-MP", "-MF", str(dep), "-c", str(SOURCE / name), "-o", str(obj)]
        rows.append(dict(logical_source=name, target=str(obj), dependency_file=str(dep), command=command))
    link += [str(RUNTIME / "native-objects" / (name + ".o")) for name in units]
    link += ["-o", str(RUNTIME / "native-bin/rpg"), "-lSDL2_mixer", "-lSDL2_ttf", "-lSDL2", "-lGL", "-lm", "-lstdc++", "-pthread", str(LIBRARY)]
    return dict(schema="valwood.modular-knight.native-candidate-plan.v1", source=str(SOURCE),
                output=str(RUNTIME), baseline_plan_sha256=sha(BASE_PLAN), units=rows, link=link,
                source_sha256=inventory(SOURCE), jolt_library_sha256=PHYSICS_SHA,
                native_only=True, Windows_work_performed=False, gpu_used=False,
                full_application_closure_rebuild=False, full_application_dependency_closure_verified=True, skip_player=skip_player)


def dependencies(path):
    raw = Path(path).read_text().replace("\\\n", " ")
    first_rule = raw.split("\n", 1)[0]
    return [Path(p) for p in shlex.split(first_rule.split(":", 1)[1])]


def compile_one(row):
    obj = Path(row["target"])
    dep = Path(row["dependency_file"])
    receipt = obj.with_suffix(".json")
    if obj.is_file() and dep.is_file() and receipt.is_file():
        old = json.loads(receipt.read_text())
        if old.get("command") == row["command"]:
            prior = old.get("actual_dependency_sha256", {})
            if prior and all(Path(p).is_file() and sha(p) == h for p, h in prior.items()):
                return dict(logical_source=row["logical_source"], cached=True, elapsed_seconds=0)
    obj.parent.mkdir(parents=True, exist_ok=True)
    log = obj.with_suffix(".log")
    started = time.monotonic()
    input_sha = sha(SOURCE / row["logical_source"])
    source_before = inventory(SOURCE)
    with log.open("w") as handle:
        proc = subprocess.run(row["command"], cwd=SOURCE, stdout=handle, stderr=subprocess.STDOUT)
    elapsed = time.monotonic() - started
    if proc.returncode:
        raise RuntimeError(f"Compile failed: {row['logical_source']}; log {log}\n" + log.read_text()[-6000:])
    if sha(SOURCE / row["logical_source"]) != input_sha:
        obj.unlink()
        raise RuntimeError("Source changed during compile: " + row["logical_source"])
    actual = {}
    for p in dependencies(dep):
        absolute = p.resolve() if p.is_absolute() else (SOURCE / p).resolve()
        # pkg-config supplies some system headers through -I, so -MMD may
        # retain them. Record those actual inputs as well; canonical project
        # headers are still rejected rather than silently mixed into the fork.
        if not absolute.is_relative_to(SOURCE) and not any(absolute.is_relative_to(p) for p in (Path("/usr/include"), Path("/usr/lib"))):
            raise RuntimeError("Compiler escaped frozen source: " + str(absolute))
        actual[str(absolute)] = sha(absolute)
        if absolute.is_relative_to(SOURCE) and source_before.get(str(absolute.relative_to(SOURCE))) != actual[str(absolute)]:
            obj.unlink()
            raise RuntimeError("Dependency changed during compile: " + str(absolute))
    result = dict(**row, elapsed_seconds=elapsed, cached=False, actual_dependency_sha256=actual)
    write_json(receipt, result)
    return result


def build(skip_player, jobs):
    current = plan(skip_player)
    RUNTIME.mkdir(parents=True, exist_ok=True)
    write_json(RUNTIME / "BUILD_PLAN.json", current)
    completed = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(compile_one, row): row for row in current["units"]}
        for future in as_completed(futures):
            result = future.result()
            completed.append(result)
            print(f"[{len(completed)}/{len(futures)}] {result['logical_source']} {'cached' if result['cached'] else format(result['elapsed_seconds'], '.1f') + ' s'}", flush=True)
    if skip_player:
        write_json(RUNTIME / "BASE_COMPILE.json", dict(ready=False, base_units_compiled=len(completed), full_player_build_pending=True, native_only=True))
        return
    # Validate every actual unit dependency immediately before linking.
    for row in current["units"]:
        receipt = json.loads(Path(row["target"]).with_suffix(".json").read_text())
        if any(sha(p) != h for p, h in receipt["actual_dependency_sha256"].items()):
            raise RuntimeError("Header changed after compile: " + row["logical_source"])
    if sha(LIBRARY) != PHYSICS_SHA:
        raise RuntimeError("Jolt changed before link")
    binary = RUNTIME / "native-bin/rpg"
    binary.parent.mkdir(parents=True, exist_ok=True)
    log = RUNTIME / "link.log"
    with log.open("w") as handle:
        proc = subprocess.run(current["link"], cwd=SOURCE, stdout=handle, stderr=subprocess.STDOUT)
    if proc.returncode:
        raise RuntimeError("Link failed: " + str(log) + "\n" + log.read_text()[-10000:])
    # A concurrently polished controller must not be credited to an older
    # compiled object if its source changes during the link itself.
    for row in current["units"]:
        receipt = json.loads(Path(row["target"]).with_suffix(".json").read_text())
        if any(sha(p) != h for p, h in receipt["actual_dependency_sha256"].items()):
            binary.unlink()
            raise RuntimeError("Source/header changed during link: " + row["logical_source"])
    write_json(RUNTIME / "READY.json", dict(ready=True, binary=str(binary), sha256=sha(binary),
               source=str(SOURCE), source_sha256=inventory(SOURCE), native_only=True,
               Windows_work_performed=False, gpu_used=False, application_units=len(current["units"]),
               full_application_closure_rebuilt=False, full_application_dependency_closure_verified=True, visual_acceptance_pending=True))
    print(json.dumps(dict(ready=True, binary=str(binary), sha256=sha(binary))), flush=True)


def arena_document():
    # The full artist workspace currently exceeds this older frozen runtime's
    # custom-asset budget. This gameplay candidate owns an independently
    # authored arena, with actual render/collision meshes rather than borrowing
    # unrelated city architecture or ignoring scene validation failures.
    rows = ["VALWOOD_STUDIO 14", "look 0.18 1.1 1.1 1.0 14.0 -54 0.22 0 0.25 0.08", "structure_ids 8"]
    def structure(identity, kind, width, height, points):
        flat = " ".join(f"{v:g}" for point in points for v in point)
        rows.append(f'structure {identity} {kind} "" 3 {width:g} {height:g} 0.35 1.1 0 0 0 0 0 {len(points)} {flat}')
    structure(1, 1, 24, 3, [(-1812, 64, -10500), (-1788, 64, -10500)])
    structure(2, 1, 4, 3, [(-1800, 64, -10520), (-1800, 64, -10512)])
    structure(3, 0, 4, 1.3, [(-1812, 64, -10512), (-1812, 64, -10488)])
    structure(4, 0, 4, 1.3, [(-1788, 64, -10512), (-1788, 64, -10488)])
    structure(5, 0, 4, 1.3, [(-1812, 64, -10488), (-1788, 64, -10488)])
    structure(6, 0, 4, 1.3, [(-1812, 64, -10512), (-1802, 64, -10512)])
    structure(7, 0, 4, 1.3, [(-1798, 64, -10512), (-1788, 64, -10512)])
    rows += ['entities "VALWOOD_ENTITIES 4 1 0"', 'surfaces "VALWOOD_SURFACES 2"']
    return "\n".join(rows) + "\n"


def arena_scenery():
    # These nine sources are mandatory in the frozen viewer. Its real parser
    # rejects missing or empty instance files, so each owns a real supply prop
    # outside the fenced walking pad. All use an existing registered alias and
    # the same explicit elevated pivot as the arena's floor.
    names = ["authored_instances.txt", "starting_area_instances.txt", "rainforest_instances.txt",
             "castle_village_instances.txt", "tanthoon_bandit_camp_instances.txt", "castle_streets_instances.txt",
             "bep_environment_instances.txt", "demo_finish_instances.txt", "demo_polish_instances.txt"]
    points = [(-1814.5, -10508), (-1814.5, -10502), (-1814.5, -10496),
              (-1785.5, -10508), (-1785.5, -10502), (-1785.5, -10496),
              (-1808, -10485.5), (-1800, -10485.5), (-1792, -10485.5)]
    return {name: "# Private arena supply crate outside the fenced combat floor.\n" +
            f"purchased_crate {x / 100800 + .5:.17g} {z / 56700 + .5:.17g} 0 0.75 0 0 0 0 0 1 1 1 64\n"
            for name, (x, z) in zip(names, points)}


def stage_project():
    source = ROOT / "projects/valwood_design"
    output = WORK / "arena-project"
    if output.exists():
        raise RuntimeError("Private arena already exists; preserving it")
    output.mkdir()
    shared = {"models", "assets", "audio", "blood_assets", "environments", "fonts", "opening", "particles", "surfaces"}
    for p in sorted(source.iterdir()):
        target = output / p.name
        if p.name == "player-profile.config":
            continue
        if p.name == "opening.config":
            # The apartment's opening guard correctly rejects unguarded --play
            # launches. This independent combat project never runs that story.
            shutil.copy2(p, output / "opening.config.disabled-for-arena", follow_symlinks=True)
            continue
        if p.is_dir() and p.name in shared:
            target.symlink_to(p.resolve(), target_is_directory=True)
        elif p.is_dir():
            shutil.copytree(p, target, symlinks=False)
        elif p.is_file():
            shutil.copy2(p, target, follow_symlinks=True)
    # Canonical profile must never be used by this isolated candidate.
    # The frozen profile parser intentionally requires this exact basename.
    (output / "player-profile.config").write_text("VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n")
    spec = importlib.util.spec_from_file_location("knight_arena_geometry", ROOT / "tools/prepare_combat_arena.py")
    sys.path.insert(0, str(ROOT / "tools"))
    geometry = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(geometry)
    (output / "ground/showcase.height").write_bytes(geometry.flat_heightfield())
    (output / "city.physics").write_bytes(geometry.physics())
    (output / "playtest.route").write_bytes(geometry.route())
    (output / "arena.config").write_text("VALWOOD_COMBAT_ARENA 1\n")
    write_json(output / "combat_arena.json", geometry.contract())
    x, z = geometry.CENTER
    (output / "project.valwood").write_text(f"name=Valwood | Modular Knight Test\nterrain=valwood_atlas\nspawn={x} {z - 5} -.04\ncamera={geometry.math.pi} -.04 7\nday_fraction=.60\n")
    (output / "world.studio").rename(output / "source-world.studio.disabled-for-arena")
    (output / "world.studio").write_text(arena_document())
    for path in output.glob("world.studio.recovery*"):
        path.rename(path.with_name(path.name + ".disabled-for-arena"))
    for name, content in arena_scenery().items():
        (output / "scenery" / name).write_text(content)
    # The viewer conditionally appends this tenth source when it is present.
    optional_completion = output / "scenery/demo_completion_instances.txt"
    if optional_completion.exists():
        optional_completion.rename(optional_completion.with_name("demo_completion_instances.disabled-for-arena"))
    # There are no imported city architecture instances in this scene. The
    # procedural floor and fence meshes are the matching collision source.
    (output / "collision/structures/catalog").write_text('VALWOOD_STRUCTURE_COLLISION 1\nnone "starting_barrel" "models/starting_barrel.glb"\n')
    bow_bank = ROOT / "build/combat-arena/assets"
    bow_record = geometry.bow_asset_record(bow_bank)
    (output / "arena_assets").symlink_to(bow_bank, target_is_directory=True)
    write_json(WORK / "PROJECT_ORIGIN.json", dict(schema="valwood.modular-knight.private-project.v1", source_project=str(source),
        project=str(output), shared_banks={n: str((source / n).resolve()) for n in sorted(shared)},
        private_saves=True, private_profile=True, shaders_copied=True, terrain_copied=True, collision_copied=True,
        editor_allowed=False, source_unchanged=True, native_only=True, authored_arena_structures=7,
        bow_assets=bow_record, authored_arena_props=9, full_world_authoring_included=False))
    print(json.dumps(dict(project=str(output), private_mutable_files=True)), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--inspect", action="store_true")
    mode.add_argument("--build", action="store_true")
    mode.add_argument("--stage-project", action="store_true")
    parser.add_argument("--skip-player", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if sys.platform != "linux":
        raise RuntimeError("This candidate builder is Linux-only")
    if args.prepare:
        prepare_source()
        print(json.dumps(dict(source=str(SOURCE), ready_for_player_changes=True)))
    elif args.inspect:
        current = plan(args.skip_player)
        write_json(WORK / "EDITOR_BUILD_INSPECT.json", current)
        print(json.dumps(dict(units=len(current["units"]), source=str(SOURCE), skip_player=args.skip_player)))
    elif args.stage_project:
        stage_project()
    else:
        build(args.skip_player, max(1, min(3, args.jobs)))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        sys.exit(str(error))
