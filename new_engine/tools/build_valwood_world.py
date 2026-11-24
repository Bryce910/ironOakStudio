#!/usr/bin/env python3
"""Prepare an isolated map-scale Valwood project without changing Seldonne.

Re-running preserves every existing project file. --reset replaces generated
project metadata/reference copies only; it does not erase saves or authoring.
Runtime terrain comes from the engine's valwood_atlas backend, not a stretched
Seldonne sculpt file. Supporting assets are linked individually and stay at
their original resolution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "projects" / "valwood_world"
REFERENCE = ROOT / "assets" / "reference" / "Valwood-Map.jpg"
WIDTH = 100800.0
DEPTH = WIDTH * 2818.0 / 5009.0
WALK_SPEED = 3.5
WORLD_SHADERS = [
    f"world_{stage}.{kind}.glsl"
    for stage in ("terrain", "sky", "water", "foliage", "post", "temporal", "torch_flame")
    for kind in ("vs", "fs")
]


def write_generated(path, text, reset):
    path.parent.mkdir(parents=True, exist_ok=True)
    if reset or not path.exists():
        path.write_text(text, encoding="utf-8")


def shared_file(source, destination):
    """A file link never redirects an authoring directory into another project."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not os.path.lexists(destination):
        destination.symlink_to(os.path.relpath(source, destination.parent))


def copy_reference(source, destination, reset=False):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() == destination.resolve():
        return
    if reset or not destination.exists():
        shutil.copy2(source, destination)


def build(args):
    for directory in ("assets", "captures", "saves", "imports/models", "models", "ground", "fonts", "shaders"):
        (PROJECT / directory).mkdir(parents=True, exist_ok=True)

    if args.map:
        map_source = args.map.expanduser().resolve()
    elif REFERENCE.is_file():
        map_source = REFERENCE
    elif Path("/home/computer/Desktop/Valwood-Map.jpg").is_file():
        map_source = Path("/home/computer/Desktop/Valwood-Map.jpg")
    elif (PROJECT / "assets/map.jpg").is_file():
        map_source = PROJECT / "assets/map.jpg"
    else:
        raise SystemExit("Supply the original map with --map /path/to/Valwood-Map.jpg")
    if not map_source.is_file():
        raise SystemExit(f"Map does not exist: {map_source}")
    copy_reference(map_source, REFERENCE, args.reset)
    copy_reference(map_source, PROJECT / "assets/map.jpg", args.reset)

    # Keep the existing shader library available, including nested common files.
    for source in sorted((ROOT / "shaders").rglob("*")):
        if source.is_file():
            shared_file(source, PROJECT / "shaders" / source.relative_to(ROOT / "shaders"))
    # The renderer may be authored concurrently with project preparation.
    for name in WORLD_SHADERS:
        shared_file(ROOT / "shaders" / name, PROJECT / "shaders" / name)
    for source in sorted((ROOT / "fonts").rglob("*")):
        if source.is_file():
            shared_file(source, PROJECT / "fonts" / source.relative_to(ROOT / "fonts"))
    # Reuse the already cooked, licensed visual assets as immutable files;
    # none of the village layout, NPC data or mutable model settings is shared.
    model_names = ["silver_fir.glb"]
    model_names += [f"woodland_fern_{i}.glb" for i in range(3)]
    model_names += [f"mossy_boulder_{i}.glb" for i in range(3)]
    for name in model_names:
        source = ROOT / "projects/seldonne/models" / name
        if source.is_file():
            shared_file(source, PROJECT / "models" / name)

    material_sources = {
        "grass": ("leafy_grass", "leafy_grass"),
        "dirt": ("forest_ground_04", "forest_ground_04"),
        "sand": ("forest_ground_04", "forest_ground_04"),
        "rock": ("rock_boulder_dry", "rock_boulder_dry"),
    }
    for slot, (folder, prefix) in material_sources.items():
        for suffix, channel in (("", "diff"), ("_nor", "nor_gl"), ("_rgh", "arm")):
            source = ROOT / "assets/polyhaven" / folder / f"{prefix}_{channel}_2k.jpg"
            if source.is_file():
                shared_file(source, PROJECT / "ground" / f"terrain_{slot}{suffix}.png")

    spawn_x = (.384 - .5) * WIDTH
    spawn_z = (.611 - .5) * DEPTH
    manifest = (
        "# Valwood project version 1\n"
        "# The atlas renderer resolves named landmarks to safe terrain spawns.\n"
        "name=Valwood World | Map Terrain\n"
        "terrain=valwood_atlas\n"
        f"spawn={spawn_x:.6f} {spawn_z:.6f} 0\n"
        "camera=0 0.16 7\n"
        "day_fraction=0.32\n"
    )
    write_generated(PROJECT / "project.valwood", manifest, args.reset)
    metadata = {
        "format_version": 1,
        "terrain": "valwood_atlas",
        "seed": 4217,
        "world_width_metres": WIDTH,
        "world_depth_metres": DEPTH,
        "standard_walk_metres_per_second": WALK_SPEED,
        "straight_width_walk_hours": WIDTH / WALK_SPEED / 3600.0,
        "scale_reference": "Uninterrupted straight west-to-east width at standard walk speed; slopes, detours and water crossings add time.",
        "map_width_pixels": 5009,
        "map_height_pixels": 2818,
        "map_sha256": hashlib.sha256((PROJECT / "assets/map.jpg").read_bytes()).hexdigest(),
        "axes": "X east, Z south; map north is negative Z; 1 world unit = 1 metre.",
        "runtime_authority": "Engine atlas terrain/landmark definitions; this file records the preparation inputs.",
        "seldonne_project": "Preserved separately at projects/seldonne; no scene data or saves copied.",
    }
    write_generated(PROJECT / "world_generation.json", json.dumps(metadata, indent=2) + "\n", args.reset)
    write_generated(PROJECT / "README.txt", (
        "Valwood map terrain project\n\n"
        "Launch from the enclosing final_engine folder with Launch_Valwood_World.sh.\n"
        "Use --landmark seldonne, --landmark callicut or --tour for inspection.\n"
        "This project has its own assets/map.jpg, captures, imports and saves.\n"
        "The 100.8 km west/east reference width takes eight hours at 3.5 m/s.\n"
        "That is a straight uninterrupted width reference, not a timed overland journey.\n"
        "The original map supplies relative geography; its physical scale is a production decision.\n"
        "The engine atlas backend supplies the base terrain. The Seldonne village\n"
        "and its authored sculpt/props/masks remain in their original project.\n"
    ), args.reset)
    print(f"Prepared {PROJECT}")
    print(f"World {WIDTH / 1000:.3f} x {DEPTH / 1000:.3f} km; {WIDTH / WALK_SPEED / 3600:.1f} h straight-width walk")
    print("Existing project files preserved." if not args.reset else "Generated metadata/reference copies refreshed; saves and authoring preserved.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", type=Path, help="Original Valwood map JPEG; retained as a project reference")
    parser.add_argument("--reset", action="store_true", help="Refresh generated metadata/reference copies; retain saves and authored files")
    build(parser.parse_args())
