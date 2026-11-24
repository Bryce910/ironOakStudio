#!/usr/bin/env python3
"""Capture real native gameplay animation; deliberately separate from FPS runs."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ACTORS = {"knight": (0, 720), "traveler": (26, 660), "travelerwoman": (27, 660)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actor", choices=["all", *ACTORS], default="all")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/performance/npc-animation/clips")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name, (index, frames) in ACTORS.items():
        if args.actor not in ("all", name):
            continue
        with tempfile.TemporaryDirectory(prefix="valwood-npc-capture-") as temporary:
            prefix = Path(temporary) / "frame"
            stem = output / name
            env = os.environ.copy()
            # A frozen diagnostic world would conceal the animation under test.
            env.pop("VALWOOD_QA_FIXED_TIME", None)
            env.pop("VALWOOD_FOLIAGE_SEQUENCE", None)
            env.pop("VALWOOD_QA_VILLAGE_LOOK", None)
            env.pop("VALWOOD_DISABLE_VILLAGE_POPULATION", None)
            env.pop("LIBGL_ALWAYS_SOFTWARE", None)
            if env.get("SDL_VIDEODRIVER") in ("offscreen", "dummy"):
                env.pop("SDL_VIDEODRIVER")
            env.update(SDL_AUDIODRIVER="dummy", VALWOOD_QA_NPC_FOLLOW=str(index),
                       VALWOOD_NPC_SEQUENCE=str(prefix))
            command = [str(ROOT.parent / "Launch_Valwood_World.sh"), "--scenic", "castle-market",
                       "--width", "1600", "--height", "900", "--frames", str(frames),
                       "--no-hud", "--capture", str(stem.with_suffix(".bmp"))]
            print(f"Native NPC animation capture: {name}", flush=True)
            with stem.with_suffix(".log").open("w") as log:
                subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                               stderr=subprocess.STDOUT, check=True)
            capture = json.loads(Path(str(stem.with_suffix(".bmp")) + ".json").read_text())
            assert capture["capture_ok"] and not capture["offscreen"]
            assert "AMD Radeon RX 5300" in capture["renderer"], capture["renderer"]
            world = json.loads(Path(str(stem.with_suffix(".bmp")) + ".world.json").read_text())
            assert world["gl_error"] == 0 and world["failed_tiles"] == 0
            assert world["village_population"]["residents"] == 128
            assert world["village_animation"]["gpu_pose_blending"]
            assert world["village_animation"]["pose_batch_capacity"] == 91
            images = sorted(Path(temporary).glob("frame-*.bmp"))
            assert len(images) == 360, len(images)
            subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                            "-framerate", "60", "-start_number", "0", "-i", str(prefix) + "-%03d.bmp",
                            "-frames:v", "360", "-an", "-c:v", "libx264", "-preset", "fast",
                            "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart",
                            str(stem.with_suffix(".mp4"))], check=True)
            # Keep a lossless representative frame for visual review.
            from PIL import Image
            Image.open(images[180]).save(stem.with_suffix(".png"))
            record = {"actor": index, "rendered_frames": frames, "clip_frames": 360,
                      "simulation_hz": 60, "width": 1600, "height": 900,
                      "first_visual_time": 17 + (frames - 359) / 60,
                      "last_visual_time": 17 + frames / 60,
                      "renderer": capture["renderer"], "timed_performance_run": False,
                      "capture": "unmodified display pixels after temporal resolve; H.264 encoding"}
            stem.with_suffix(".clip.json").write_text(json.dumps(record, indent=2) + "\n")
            print(stem.with_suffix(".mp4"), flush=True)


if __name__ == "__main__":
    main()
