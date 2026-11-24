#!/usr/bin/env python3
"""Build and run the accepted camera math regressions against this candidate.

This is CPU-only coverage. It does not substitute for reviewing camera framing,
animation, lighting, input or frame pacing in the running game.
"""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess
import time


N = Path(__file__).resolve().parents[1]
SOURCE = N / "source-v1"
REVIEW = N / "review" / "camera-cpu"
TESTS = (
    "world_combat_closeup_camera_test",
    "world_combat_focus_camera_test",
    "world_follow_framing_test",
    "world_arena_camera_test",
)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run():
    REVIEW.mkdir(parents=True, exist_ok=True)
    compiler = "g++-13"
    version = subprocess.run(
        [compiler, "--version"], capture_output=True, text=True, check=True
    ).stdout.splitlines()[0]
    results = {}
    started = time.monotonic()
    for name in TESTS:
        source = SOURCE / "tests" / (name + ".cpp")
        binary = REVIEW / name
        units = [source]
        if name == "world_arena_camera_test":
            units.append(SOURCE / "engine" / "world_training_combat.cpp")
        command = [
            compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic",
            "-I", str(SOURCE), *map(str, units), "-o", str(binary),
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=120)
        executed = None
        if compiled.returncode == 0:
            executed = subprocess.run(
                [str(binary)], capture_output=True, text=True, timeout=30
            )
        dependency_hashes = {}
        if compiled.returncode == 0:
            for unit in units:
                dependencies = subprocess.run(
                    [compiler, "-std=c++17", "-I", str(SOURCE), "-MM", str(unit)],
                    capture_output=True, text=True, check=True, timeout=30,
                ).stdout
                (REVIEW / (unit.stem + ".d")).write_text(dependencies)
                make_rule = dependencies.replace("\\\n", " ")
                for item in shlex.split(make_rule.split(":", 1)[1]):
                    path = Path(item)
                    dependency_hashes[str(path.relative_to(SOURCE))] = sha(path)
        passed = (
            compiled.returncode == 0
            and executed is not None
            and executed.returncode == 0
            and executed.stdout.startswith("PASS")
            # The close-up test also emits an informational overlap comparison.
        ) or (
            name == "world_combat_closeup_camera_test"
            and compiled.returncode == 0
            and executed is not None
            and executed.returncode == 0
            and "\nPASS close-melee camera:" in executed.stdout
        )
        results[name] = {
            "passed": bool(passed),
            "compile_command": command,
            "compile_returncode": compiled.returncode,
            "compiler_output": compiled.stdout + compiled.stderr,
            "returncode": None if executed is None else executed.returncode,
            "output": "" if executed is None else executed.stdout + executed.stderr,
            "source_sha256": sha(source),
            "dependency_sha256": dependency_hashes,
            "binary_sha256": sha(binary) if compiled.returncode == 0 else None,
        }
        print(name + ": " + ("PASS" if passed else "FAIL"), flush=True)
    report = {
        "passed": all(result["passed"] for result in results.values()),
        "candidate": str(N),
        "compiler": version,
        "elapsed_seconds": time.monotonic() - started,
        "tests": results,
        "cpu_only": True,
        "limits": [
            "Exercises candidate camera headers, not the engine viewer integration.",
            "Does not certify native input, animation, image quality or frame pacing.",
        ],
    }
    (N / "review" / "CAMERA_CPU.json").write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(run())
