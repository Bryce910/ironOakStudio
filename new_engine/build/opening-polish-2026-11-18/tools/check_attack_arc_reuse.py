#!/usr/bin/env python3
"""CPU-only regression and timing evidence for exact wardrobe arc-table reuse."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import subprocess
import time

N = Path(__file__).resolve().parents[1]
COMPILER = "/usr/bin/x86_64-linux-gnu-g++-13"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(args, out, label):
    result = subprocess.run(args, capture_output=True, text=True)
    (out / (label + ".stdout.txt")).write_text(result.stdout)
    (out / (label + ".stderr.txt")).write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{label} failed ({result.returncode}): {result.stderr[-3000:]}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rig-dir", type=Path)
    parser.add_argument("--output", type=Path, default=N / "review/attack-arc-reuse")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = N / "source-v1"
    rig = (args.rig_dir or (N / "motion/rigs" if (N / "motion/rigs/loadouts.manifest").exists() else N / "assets/story_hero/rigs")).resolve()
    cfgs = [N / "assets/story_hero" / outfit / "appearance.cfg" for outfit in ("civilian", "armored")]
    rigs = [rig / (name + ".glb") for name in ("sword", "sword_shield", "great_axe", "staff")]
    manifest = rig / "loadouts.manifest"
    rows = [shlex.split(line) for line in manifest.read_text().splitlines() if line.strip()]
    if rows[0] != ["VALWOOD_SKELETAL_LOADOUTS", "1"] or len(rows) != 4:
        raise RuntimeError("Unexpected loadout manifest")
    for index, prefix in enumerate(("Shield_", "Axe_", "Staff_")):
        if rows[index + 1] != [str(index + 2), str(rigs[index + 1]), prefix]:
            raise RuntimeError("Manifest must point at the exact tested rig paths")
    asset_files = [*cfgs, *[cfg.parent / shlex.split(cfg.read_text().splitlines()[1])[1] for cfg in cfgs], *rigs, manifest]
    before_assets = {str(path): sha(path) for path in asset_files}
    unit = source / "tests/world_skeletal_attack_arc_cache_test.cpp"
    owned = source / "tests/world_skeletal_attack_arc_owned_test.cpp"
    engine = [source / "engine" / name for name in ("world_skeletal_hero.cpp", "world_skeletal_hero_loadouts.cpp", "world_animation.cpp")]
    common = [COMPILER, "-std=c++17", "-O2", "-Wall", "-Wextra", "-I" + str(source)]
    dependencies = {Path(__file__).resolve()}
    for path in [unit, owned, *engine]:
        result = subprocess.run([*common, "-MM", str(path)], capture_output=True, text=True, check=True)
        paths = shlex.split(result.stdout.replace("\\\n", " ").split(":", 1)[1])
        dependencies.update(Path(path).resolve() for path in paths)
    before_sources = {str(path): sha(path) for path in sorted(dependencies)}
    commands = [
        [*common, str(unit), "-o", str(out / "exact-input-test")],
        [*common, str(owned), *map(str, engine), "-o", str(out / "owned-test")],
    ]
    start = time.monotonic()
    for index, command in enumerate(commands):
        run(command, out, f"build-{index}")
    unit_command = [str(out / "exact-input-test")]
    owned_command = [str(out / "owned-test"), *map(str, cfgs), *map(str, rigs), str(manifest)]
    raw_unit = run(unit_command, out, "exact-input-test")
    raw_owned = run(owned_command, out, "owned-test")
    exact = json.loads(raw_unit.stdout)
    actual = json.loads(raw_owned.stdout)
    after_assets = {str(path): sha(path) for path in asset_files}
    after_sources = {str(path): sha(path) for path in sorted(dependencies)}
    (out / "EXACT_INPUTS.json").write_text(json.dumps(exact, indent=2) + "\n")
    (out / "OWNED_RAW.json").write_text(json.dumps(actual, indent=2) + "\n")
    passed = exact["passed"] and actual["passed"] and before_assets == after_assets and before_sources == after_sources
    report = {
        "passed": passed,
        "gpu_used": False,
        "elapsed_seconds": time.monotonic() - start,
        "asset_bytes_unchanged_during_check": before_assets == after_assets,
        "source_bytes_unchanged_during_check": before_sources == after_sources,
        "asset_sha256": before_assets,
        "source_sha256": before_sources,
        "build_commands": commands,
        "commands": [unit_command, owned_command],
        "binary_sha256": {str(out / name): sha(out / name) for name in ("exact-input-test", "owned-test")},
        "timing_limitations": [
            "The control computes attack tables and the reused case copies exact tables; both parse and validate real assets.",
            "These sequential CPU checks do not represent cold disk IO, GPU uploads, shader warmup, or normal gameplay frame pacing.",
            "Runtime appearance stage clocks separately include body parsing, composition, world preparation and first-person preparation.",
        ],
    }
    (out / "EXECUTION.json").write_text(json.dumps(report, indent=2) + "\n")
    if not passed:
        raise RuntimeError("Source or asset bytes changed during check")
    print(json.dumps({"passed": True, "exact_inputs": exact, "owned_inputs": actual, "report": str(out / "EXECUTION.json")}, indent=2))


if __name__ == "__main__":
    main()
