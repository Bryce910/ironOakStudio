#!/usr/bin/env python3
"""Profile the animated village sequentially, without capture-sequence readbacks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/performance/npc-animation/native")
    parser.add_argument("--scenes", nargs="+", choices=("market", "stalls", "alley", "courtyard", "housing", "marketrow", "forecourt", "hall", "route", "demo-route", "hero-entry", "hero-mid", "hero-reverse", "hero-route", "training", "statue", "armory", "drawbridge", "creek", "castle-route", "farms", "farms-reverse", "farm-route", "watch", "watch-close", "farm-tools"),
                        default=("market", "stalls", "alley", "route"))
    parser.add_argument("--require-village-surfaces", action="store_true",
                        help="Require the directional sky, ground history and bounded paving pass")
    parser.add_argument("--keep-going", action="store_true",
                        help="Collect every scene before rejecting any timing-budget failures")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    inputs = [ROOT / "rpg", ROOT / "Makefile", *ROOT.glob("engine/world_*"),
              *ROOT.glob("engine/texture_cache.[ch]"), *ROOT.glob("shaders/world_*.glsl"),
              *ROOT.glob("projects/valwood_world/models/*"),
              *ROOT.glob("projects/valwood_world/scenery/*"),
              *ROOT.glob("projects/valwood_world/ground/village*"),
              ROOT / "assets/purchased/pets/PROVENANCE.json", ROOT / "assets/purchased/pets/LICENSE_SOURCE.txt"]
    inputs = sorted(p for p in inputs if p.is_file() and p.suffix != ".o")

    def freeze():
        return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}

    frozen = freeze()
    (output / "source-freeze.json").write_text(json.dumps(frozen, indent=2) + "\n")
    env = os.environ.copy()
    for name in ("VALWOOD_QA_FIXED_TIME", "VALWOOD_QA_VILLAGE_LOOK", "VALWOOD_FOLIAGE_SEQUENCE",
                 "VALWOOD_NPC_SEQUENCE", "VALWOOD_QA_NPC_FOLLOW", "VALWOOD_QA_PET_FOLLOW", "VALWOOD_DISABLE_VILLAGE_POPULATION", "VALWOOD_DISABLE_VILLAGE_PETS",
                 "VALWOOD_DISABLE_VILLAGE_DIRECTIONAL", "VALWOOD_DISABLE_VILLAGE_GROUND_HISTORY",
                 "VALWOOD_QA_FOLIAGE_FRAGMENT", "VALWOOD_QA_TERRAIN_FRAGMENT", "LIBGL_ALWAYS_SOFTWARE"):
        env.pop(name, None)
    if env.get("SDL_VIDEODRIVER") in ("offscreen", "dummy"):
        env.pop("SDL_VIDEODRIVER")
    env["SDL_AUDIODRIVER"] = "dummy"
    fields = ("mem_info_vram_used", "mem_info_vram_total", "mem_info_gtt_used")
    devices = [p for p in Path("/sys/class/drm").glob("card*/device") if (p / fields[0]).exists()]
    samples = []
    peaks = {field: 0 for field in fields}
    stop = threading.Event()

    def monitor():
        while not stop.is_set():
            for device in devices:
                row = {"time": time.time(), "device": str(device)}
                for field in fields:
                    try:
                        row[field] = int((device / field).read_text())
                        peaks[field] = max(peaks[field], row[field])
                    except OSError:
                        pass
                samples.append(row)
            stop.wait(1)

    thread = threading.Thread(target=monitor, daemon=True)
    thread.start()
    profiles = []
    try:
        for scene in args.scenes:
            subprocess.run(["python3", str(ROOT / "tools/benchmark_castle_village.py"),
                            "--scene", scene, "--output", str(output)],
                           cwd=ROOT, env=env, check=True)
            profile = json.loads((output / f"{scene}-final.json").read_text())
            world = json.loads((output / f"{scene}-final.json.world.json").read_text())
            assert freeze() == frozen, "Source or runtime assets changed during profiling"
            result = {"scene": scene, **{key: profile[key] for key in (
                "mean_fps", "one_percent_low_fps", "p99_ms", "max_ms", "within_45fps_budget_percent",
                "mean_render_total_gpu_ms", "mean_forest_gpu_ms", "mean_raw_update_ms")},
                "animation": world["village_animation"], "population": world["village_population"], "pets": world["village_pets"],
                "training": world["castle_training"], "creek": world["castle_creek"],
                "watch": world["village_watch"],
                "gl_error": world["gl_error"], "failed_tiles": world["failed_tiles"],
                "remaining_texture_entries": world["remaining_texture_entries"]}
            if "village_surfaces" in world:
                result["surfaces"] = world["village_surfaces"]
            if args.require_village_surfaces:
                surfaces = world["village_surfaces"]
                assert surfaces["directional_sky"] and surfaces["ground_history"] and surfaces["stochastic_paving"]
                assert surfaces["terrain_sampler_count"] == 16 and surfaces["height_slices"] >= 9
            if scene in ("route", "demo-route", "hero-route", "castle-route", "farm-route"):
                result["route"] = world["benchmark_route"]
            profiles.append(result)
            (output / "results.json").write_text(json.dumps({"source_unchanged": True,
                "resolution": [1600, 900], "frame_cap": 60, "profiles": profiles}, indent=2) + "\n")
            if not args.keep_going:
                assert 45 <= profile["mean_fps"] <= 60.5 and profile["one_percent_low_fps"] >= 45, result
    finally:
        stop.set()
        thread.join()
        (output / "driver-memory.json").write_text(json.dumps({
            "note": "Whole-system Radeon memory at1Hz, including desktop/apps; no readbacks during timed profiles.",
            "peaks": peaks, "samples": samples}, indent=2) + "\n")
    print(json.dumps({"profiles": profiles, "driver_memory_peaks": peaks}, indent=2), flush=True)
    failures = [result for result in profiles if not (45 <= result["mean_fps"] <= 60.5 and result["one_percent_low_fps"] >= 45)]
    assert not failures, {"timing_budget_failures": failures}


if __name__ == "__main__":
    main()
