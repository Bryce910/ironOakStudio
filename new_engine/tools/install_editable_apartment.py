#!/usr/bin/env python3
"""Install a reviewed editable apartment stage after the editor has closed."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil


def old_alias(name: str) -> bool:
    return name == "nm_opening_home" or name.startswith("nm_opening_reference_finward_scene_")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    project = args.project.resolve(strict=True)
    stage = args.stage.resolve(strict=True)
    manifest = json.loads((stage / "manifest.json").read_text())
    if manifest.get("schema") != "valwood.editable-apartment.v1" or len(manifest["records"]) != 593:
        raise ValueError("Incomplete apartment stage")
    studio = project / "world.studio"
    scenery = project / "scenery/authored_instances.txt"
    assets = (stage / "assets.txt").read_text().splitlines()
    instances = (stage / "instances.txt").read_text().splitlines()
    if len(assets) != 593 or len(instances) != 593:
        raise ValueError("Stage asset and instance count mismatch")
    old_studio = studio.read_text().splitlines()
    old_instances = scenery.read_text().splitlines()
    kept_studio = [line for line in old_studio if not (
        line.startswith("asset ") and old_alias(line.split()[1]))]
    kept_instances = [line for line in old_instances if not (
        line.split() and old_alias(line.split()[0]))]
    if len(old_studio) - len(kept_studio) != 13 or len(old_instances) - len(kept_instances) != 13:
        raise ValueError("Apartment source scene differs from the reviewed 13 chunks")
    if any(record["alias"] in "\n".join(kept_studio) for record in manifest["records"]):
        raise ValueError("A new apartment alias collides with existing scene data")
    models_source = stage / "models/opening/editable"
    models_target = project / "models/opening/editable"
    if models_target.exists() or not models_source.is_dir():
        raise ValueError("Editable model bank is unavailable or already installed")
    for record in manifest["records"]:
        stem = Path(record["stem"]).name
        if not (models_source / (stem + ".glb")).is_file() or not (models_source / (stem + "_mid.glb")).is_file():
            raise ValueError("Missing staged model: " + stem)

    backup = stage / "before-install"
    backup.mkdir(exist_ok=False)
    shutil.copy2(studio, backup / "world.studio")
    shutil.copy2(scenery, backup / "authored_instances.txt")
    models_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(models_source), str(models_target))
    changes = ((studio, kept_studio + assets), (scenery, kept_instances + instances))
    try:
        for target, lines in changes:
            temporary = target.with_name(target.name + ".editable-apartment.tmp")
            temporary.write_text("\n".join(lines) + "\n")
            os.replace(temporary, target)
    except BaseException:
        shutil.copy2(backup / "world.studio", studio)
        shutil.copy2(backup / "authored_instances.txt", scenery)
        shutil.move(str(models_target), str(models_source))
        raise
    print(f"Installed {manifest['furniture']} selectable apartment furnishings and {manifest['structure_chunks']} structural chunks")
    print("Backups:", backup)


if __name__ == "__main__":
    main()
