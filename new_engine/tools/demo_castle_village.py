#!/usr/bin/env python3
"""Open the real Valdoa castle district, or run its guided native walking test.

Interactive mode uses the normal game camera. Guided/benchmark modes use the
same collision-aware authored route and fixed 60Hz movement; their measured
rendering frame rate remains independent. No asset downloads or rebuilds.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shlex
import subprocess

from check_castle_demo import ROOT, ROUTE_ID, HERO_ROUTE_ID, EXPANSION_ROUTE_ID, FARMS_ROUTE_ID, require, scene_report, validate_native_report

STOPS = {"arrival": "castle-demo", "alley": "castle-alley", "market": "castle-market", "stalls": "castle-stalls",
         "marketrow": "castle-marketrow", "housing": "castle-housing",
         "forecourt": "castle-forecourt", "keep": "castle-forecourt", "hall": "castle-hall"}
STOPS.update({"drawbridge":"castle-drawbridge","creek":"castle-creek","training":"castle-training","statue":"castle-statue","armory":"castle-armory"})
STOPS.update({"farms": "castle-farms", "farms-reverse": "castle-farms-reverse",
              "watch": "castle-watch", "watch-close": "castle-watch-close", "farm-tools": "castle-farm-tools"})
STOPS.update({"hero": "castle-hero-entry", "hero-mid": "castle-hero-mid", "hero-reverse": "castle-hero-reverse"})
ROUTES = {"district": ("arrival", None, ROUTE_ID, "complete district"),
          "hero": ("hero", "hero_market", HERO_ROUTE_ID, "market street"),
          "castle": ("drawbridge", "castle_expansion", EXPANSION_ROUTE_ID, "castle training court"),
          "farms": ("farms", "village_farms", FARMS_ROUTE_ID, "corn and wheat headlands")}


def route_for(report, route_name):
    field = ROUTES[route_name][1]
    return (report[field] if field else report)["route"]


def command_for(mode, stop, report, output, width=1600, height=900, route_name="district"):
    scenic = STOPS[stop] if mode == "interactive" else STOPS[ROUTES[route_name][0]]
    route = route_for(report, route_name)
    command = [str(ROOT.parent / "Launch_Valwood_World.sh"), "--scenic", scenic,
               "--width", str(width), "--height", str(height), "--no-hud"]
    if mode != "interactive":
        command += ["--benchmark", "--view", "river",
                    "--warmup", "120", "--frames", str(route["frames_with_warmup"]),
                    "--profile", str(output / "profile.json"),
                    "--capture", str(output / "final.bmp")]
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("interactive", "guided", "benchmark"), default="interactive",
                        help="Walk yourself, watch the full route, or require its native 45FPS timing target")
    parser.add_argument("--stop", choices=STOPS, default=None,
                        help="Interactive starting location (default: castle drawbridge; guided/benchmark use the selected route)")
    parser.add_argument("--output", type=Path, help="Guided/benchmark logs and reports directory")
    parser.add_argument("--route", choices=ROUTES, default="district",
                        help="Guided/benchmark walk: whole district, market street, castle training/monument/armory, or corn/wheat headlands")
    parser.add_argument("--lighting", choices=("afternoon", "dusk"), default="afternoon",
                        help="Editable afternoon look or warm torch-lit dusk")
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--height", type=int, default=900)
    parser.add_argument("--dry-run", action="store_true", help="Validate data and print the command without launching")
    parser.add_argument("--check", action="store_true", help="Print CPU acceptance only; no engine launch")
    args = parser.parse_args()
    if not 320 <= args.width <= 3840 or not 240 <= args.height <= 2160:
        parser.error("Window dimensions must be320..3840 by240..2160, matching the engine CLI")
    route_start, _, route_id, route_description = ROUTES[args.route]
    stop = args.stop if args.stop is not None else ("drawbridge" if args.mode == "interactive" else route_start)
    if args.mode != "interactive" and stop not in ("arrival", route_start):
        parser.error(f"Guided and benchmark modes for --route {args.route} start at --stop {route_start}")
    if args.mode == "benchmark" and (args.width, args.height) != (1600, 900):
        parser.error("The comparison benchmark is fixed at1600x900; use guided for another window size")
    try:
        report = scene_report()
        if args.check:
            print(json.dumps(report, indent=2))
            return
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        output = (args.output or ROOT / "docs/performance/castle-demo" / stamp).resolve()
        command = command_for(args.mode, stop, report, output, args.width, args.height, args.route)
        environment = os.environ.copy()
        environment["VALWOOD_VILLAGE_LIGHTING"] = args.lighting
        route = route_for(report, args.route)
        if args.dry_run:
            print(shlex.join(["env", "VALWOOD_VILLAGE_LIGHTING=" + args.lighting, *command]))
            print(f"Route: {route['length_m']:.1f}m, {route['walking_seconds'] / 60:.1f} simulated walking minutes")
            return
        require((ROOT / "rpg").is_file(), "Engine is not built. Build it from new_engine with make before opening this demo.")
        if args.mode == "interactive":
            print("Valdoa castle district: 128 residents, cats and dogs, detailed market streets and keep.", flush=True)
            print("WASD walk; hold right mouse to look; Shift run; H controls; Tab map; F fly; Esc release mouse.", flush=True)
            print("Press H, then use Scenic trails to return to a demo stop. Close the window to quit.", flush=True)
            raise SystemExit(subprocess.run(command, cwd=ROOT, env=environment, check=False).returncode)
        output.mkdir(parents=True, exist_ok=True)
        (output / "scene-acceptance.json").write_text(json.dumps(report, indent=2) + "\n")
        (output / "command.txt").write_text(shlex.join(command) + "\n")
        print(f"Guided walk: {route['length_m']:.1f}m through the {route_description}; close its window to stop early.", flush=True)
        print(f"Reports: {output}", flush=True)
        environment["SDL_AUDIODRIVER"] = "dummy"
        with (output / "engine.log").open("w") as log:
            subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        capture = json.loads((output / "final.bmp.json").read_text())
        require(capture["capture_ok"] and not capture["offscreen"] and
                (capture["width"], capture["height"]) == (args.width, args.height), "Native demo capture failed")
        native = validate_native_report(report, output / "profile.json", require_budget=args.mode == "benchmark",
                                        width=args.width, height=args.height, expected_route=route_id)
        report.update(native)
        (output / "acceptance.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(native, indent=2))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Castle demo failed: {error}\n")


if __name__ == "__main__":
    main()
