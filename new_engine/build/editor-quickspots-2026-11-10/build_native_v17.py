#!/usr/bin/env python3
"""Build the spatial Quick Spots streaming revision from isolated sources."""
from pathlib import Path
import json
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent
old = root / "runtime-native-v10"
new = root / "runtime-native-v17"
plan = json.loads((old / "BUILD_PLAN.json").read_text())
unit = next(item for item in plan["units"] if item["logical_source"] == "engine/world_viewer.cpp")
objects = new / "native-objects"
objects.mkdir(parents=True, exist_ok=True)
all_units = ("world_studio.cpp", "world_scenery.cpp", "world_surface_field.cpp", "world_viewer.cpp", "world_designer.cpp",
             "world_entity_graph.cpp", "world_authoring_identity.cpp", "world_prefab.cpp", "world_studio_tools.cpp", "world_regions.cpp", "world_terrain_authoring.cpp")
units = ("world_terrain_authoring.cpp",) if sys.argv[1:] == ["--terrain-authoring-only"] else ("world_viewer.cpp",) if sys.argv[1:] == ["--viewer-only"] else (("world_surface_field.cpp", "world_viewer.cpp") if sys.argv[1:] == ["--terrain-only"] else (("world_scenery.cpp", "world_viewer.cpp") if sys.argv[1:] == ["--stream-only"] else all_units))
for name in units:
    command = [part.replace("source-v10", "source-v17").replace("runtime-native-v10", "runtime-native-v17")
               for part in unit["command"]]
    command[command.index("-c") + 1] = str(root / "source-v17/engine" / name)
    command[command.index("-o") + 1] = str(objects / (name + ".o"))
    command[command.index("-MF") + 1] = str(objects / (name + ".d"))
    print("Compiling", name, flush=True)
    with (objects / (name + ".log")).open("w") as log:
        subprocess.run(command, cwd=plan["cwd"], stdout=log, stderr=subprocess.STDOUT, check=True)

new_bin = new / "native-bin"
shutil.copytree(old / "native-bin", new_bin, dirs_exist_ok=True)
link = [str(objects / Path(part).name) if any(part.endswith("/" + name + ".o") for name in all_units) else part
        for part in plan["link"]]
link[link.index("-o") + 1] = str(new_bin / "rpg")
print("Linking editor", flush=True)
with (objects / "link.log").open("w") as log:
    subprocess.run(link, cwd=plan["cwd"], stdout=log, stderr=subprocess.STDOUT, check=True)
print(new_bin / "rpg")
