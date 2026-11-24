#!/usr/bin/env python3
"""Sequential native-GPU Studio workflow and complete walking-route checks.

Build first and close the live engine. This does not cook assets or change the
saved graphics preferences. Use --output to keep captures/profiles together.
The workflow's explicit edit/loading pauses are not a walking FPS benchmark.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile

import benchmark_graphics_settings as common

ROOT = Path(__file__).resolve().parents[1]
CASES = (
    ("studio-workflow", "rainforest", "arrival", 1200, True),
    ("studio-reload", "rainforest", "arrival", 360, True),
    ("studio-repair", "rainforest", "arrival", 300, True),
    ("jungle-full-first", "rainforest", "river", 7700, False),
    ("jungle-full-third", "rainforest", "river", 7700, False),
    ("castle-third", "castle-hero-entry", "river", 1900, False),
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def run_case(case, output, document, live_document=None):
    name, scene, view, frames, edited = case
    env, _ = common.native_environment()
    env.update(VALWOOD_CAPTURE_VIEW="first", VALWOOD_QA_FRAME_TIMES_ONLY="1")
    third = name.endswith("third")
    if third:
        env.pop("VALWOOD_CAPTURE_VIEW", None)
    if edited:
        env["VALWOOD_QA_STUDIO_DOCUMENT"] = str(document)
    if live_document is not None and not edited:
        env["VALWOOD_QA_STUDIO_DOCUMENT"] = str(live_document)
    if name == "studio-workflow":
        env["VALWOOD_QA_STUDIO_WORKFLOW"] = "1"
    if name == "studio-repair":
        bad = document.with_name("changed-source.studio")
        lines = ["source 0000000000000000" if line.startswith("source ") else line
                 for line in document.read_text().splitlines()]
        bad.write_text("\n".join(lines) + "\n")
        env["VALWOOD_QA_STUDIO_DOCUMENT"] = str(bad)
    profile = output / (name + ".json")
    # The launcher returns success when another instance holds its lock. Old
    # output files must never make that no-op look like a passing new run.
    for suffix in ("", ".world.json", ".csv"):
        Path(str(profile) + suffix).unlink(missing_ok=True)
    for suffix in (".bmp", ".png"):
        (output / (name + suffix)).unlink(missing_ok=True)
    command = [str(common.LAUNCHER), "--scenic", scene, "--benchmark", "--view", view,
               "--width", "1600", "--height", "900", "--frames", str(frames),
               "--warmup", "120", "--no-hud", "--profile", str(profile),
               "--capture", str(output / (name + ".bmp"))]
    log = output / (name + ".log")
    with log.open("w") as stream:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=600)
    require(result.returncode == 0, f"{name} exited {result.returncode}; see {log}")
    require(profile.is_file() and Path(str(profile) + ".world.json").is_file(),
            f"{name}: no fresh profile; close any live engine and see {log}")
    metrics = json.loads(profile.read_text())
    world = json.loads(Path(str(profile) + ".world.json").read_text())
    renderer = metrics["renderer"].lower()
    require(not any(word in renderer for word in ("llvmpipe", "softpipe", "software")),
            "Native performance verification requires a hardware renderer")
    require(world["gl_error"] == 0 and world["remaining_texture_references"] == 0,
            f"{name}: GL error or texture ownership leak")
    require(world["camera_view"]["third_person"] == third, f"{name}: wrong camera mode")
    studio = world["studio"]
    if view == "river":
        route = world["benchmark_route"]
        require(route["complete"] and route["collision_blocked_steps"] == 0,
                f"{name}: incomplete or obstructed route")
    if name == "studio-workflow":
        require(studio["applied"] == 2 and studio["rejected"] == 2 and
                studio["qa_phase"] == 5 and studio["reused_model_groups"] > 0,
                f"{name}: workflow or geometry reuse failed")
        (output / "workflow.world.studio").write_bytes(document.read_bytes())
    if edited:
        repaired = name == "studio-repair"
        require(studio["lights"] == (0 if repaired else 2), f"{name}: wrong light state")
        require((studio["terrain_tiles"] == 0) == repaired, f"{name}: wrong terrain state")
    with Path(str(profile) + ".csv").open() as stream:
        times = sorted(float(row["frame_ms"]) for row in csv.DictReader(stream)
                       if int(row["frame"]) > 120)
    require(bool(times), f"{name}: no measured frames")
    tail = math.ceil(len(times) * .01)
    summary = dict(name=name, renderer=metrics["renderer"], mean_fps=1000/(sum(times)/len(times)),
                   low_fps=1000/(sum(times[-tail:])/tail), max_ms=max(times),
                   below45=sum(value > 1000/45 for value in times), samples=len(times),
                   studio=studio)
    try:
        from PIL import Image
        Image.open(output / (name + ".bmp")).save(output / (name + ".png"))
    except ImportError:
        pass  # BMP captures remain available without Pillow.
    print(json.dumps(summary), flush=True)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--routes-only", action="store_true")
    parser.add_argument("--demo-document", type=Path, help="Copy this candidate into /tmp for route checks")
    args = parser.parse_args()
    require(os.environ.get("VALWOOD_TEST_NATIVE_GL") == "1",
            "Run with VALWOOD_TEST_NATIVE_GL=1 after closing the live engine")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    preferences = ROOT / "projects/valwood_world/graphics.settings"
    original = preferences.read_bytes() if preferences.exists() else None
    rows = []
    try:
        # The engine accepts QA documents only under /tmp. Do not let TMPDIR
        # choose a different root and accidentally fall back to the real save.
        with tempfile.TemporaryDirectory(prefix="valwood-studio-workflow-", dir="/tmp") as private:
            document = Path(private) / "world.studio"
            demo = None
            if args.demo_document:
                demo = Path(private) / "demo.studio"
                demo.write_bytes(args.demo_document.read_bytes())
            cases = CASES
            if args.routes_only:
                cases = (
                    ("jungle-full-first", "rainforest", "river", 7700, False),
                    ("jungle-full-third", "rainforest", "river", 7700, False),
                    ("castle-streets-third", "castle-west-street", "river", 12000, False),
                    ("castle-arcade-first", "castle-finished-arcade", "river", 4000, False),
                )
            for case in cases:
                rows.append(run_case(case, output, document, demo))
                (output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
    finally:
        current = preferences.read_bytes() if preferences.exists() else None
        require(current == original, "Saved graphics preferences changed during verification")
    digest = hashlib.sha256(original).hexdigest() if original is not None else "absent"
    (output / "preferences.sha256").write_text(digest + "\n")
    print("Saved graphics preferences unchanged", flush=True)


if __name__ == "__main__":
    main()
