#!/usr/bin/env python3
"""Stage only selected purchased furniture data for exact Unity prefab export.

Original prefab, FBX and material GUIDs are preserved. Textures stay in the
offline purchased source tree; no purchased scripts or project scenes are copied.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "assets/purchased/finward_house_props"
BRIDGE = ROOT.parent / ".local-tools/unity-source/asset-bridge"
SELECTED = [
    ("couch", "Furniture/Couch"),
    ("bed", "Furniture/Bed"),
    ("toilet", "Bathroom/Toilet"),
    ("bathroom_sink", "Bathroom/Bathroom_sink"),
    ("bathroom_sink_cabinet", "Bathroom/Bathroom_sink_cabinet"),
    ("kitchen_counter_a", "Cabinets_kitchen/Kitchen_counter_A"),
    ("kitchen_counter_sink", "Cabinets_kitchen/Kitchen_counter_sink"),
    ("kitchen_cabinet_a", "Cabinets_kitchen/Kitchen_cabinet_A"),
    ("kitchen_cabinet_b", "Cabinets_kitchen/Kitchen_cabinet_B"),
    ("kitchen_cabinet_large", "Cabinets_kitchen/Kitchen_cabinet_large"),
    ("fridge", "Electronics/Fridge"),
    ("stove", "Electronics/Stove"),
    ("lamp_floor_a", "Lamps/Lamp_floor_A"),
    ("sidetable_a", "Furniture/Sidetable_A"),
    ("lamp_table_a", "Lamps/Lamp_table_A"),
]
STAGED = {".fbx", ".prefab", ".mat"}
GUID = re.compile(r"guid:\s*([a-fA-F0-9]{32})")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    inventory = json.loads((DATA / "INVENTORY.json").read_text())
    guids = {entry["guid"]: entry for entry in inventory["entries"]}
    paths = {entry["path"]: entry for entry in inventory["entries"]}
    selected = [{"name": name, "path": f"Assets/HouseProps/Prefabs/{path}.prefab"}
                for name, path in SELECTED]
    pending = [item["path"] for item in selected]
    seen, skipped, missing = set(), set(), set()
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        if path not in paths or Path(path).suffix.lower() not in STAGED:
            raise ValueError("Invalid staged data dependency: " + path)
        source = DATA / "source" / path
        if not source.is_file():
            raise FileNotFoundError(source)
        seen.add(path)
        metadata = Path(str(source) + ".meta")
        if not metadata.is_file():
            raise FileNotFoundError(metadata)
        texts = [metadata.read_text()]
        if source.suffix.lower() in {".prefab", ".mat"}:
            texts.append(source.read_text())
        for text in texts:
            for guid in GUID.findall(text):
                dependency = guids.get(guid)
                if dependency is None:
                    if guid.strip("0"):
                        missing.add(guid)
                    continue
                dep = dependency["path"]
                if Path(dep).suffix.lower() in STAGED:
                    pending.append(dep)
                else:
                    skipped.add(dep)
    copies = []
    for path in sorted(seen):
        for suffix in ("", ".meta"):
            source = Path(str(DATA / "source" / path) + suffix)
            dest = Path(str(BRIDGE / path) + suffix)
            digest = sha(source)
            if dest.exists() and sha(dest) != digest:
                # Unity marks FBX data readable for extraction and updates its
                # importer metadata. Preserve that state, but never replace a
                # different asset or silently accept a different GUID.
                source_guid = GUID.search(source.read_text()) if suffix else None
                dest_guid = GUID.search(dest.read_text()) if suffix else None
                if not (source_guid and dest_guid and source_guid.group(1) == dest_guid.group(1)):
                    raise ValueError("Refusing to overwrite different bridge source: " + str(dest))
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists():
                shutil.copy2(source, dest)
            copies.append({"path": path + suffix, "bytes": source.stat().st_size,
                           "sha256": digest, "staged_sha256": sha(dest)})
    report = {
        "version": 1, "assets": selected,
        "source_inventory_sha256": sha(DATA / "INVENTORY.json"),
        "output": str(DATA / "exported"),
        "staged_dependencies": len(seen), "staged_files": copies,
        "offline_only_dependencies": sorted(skipped),
        "unresolved_non_data_guids": sorted(missing),
        "purchased_scripts_staged": False, "textures_staged": False,
    }
    target = DATA / "selection.json"
    target.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"selected_prefabs": len(selected), "staged_dependencies": len(seen),
                      "staged_bytes": sum(item["bytes"] for item in copies),
                      "selection": str(target)}, indent=2))


if __name__ == "__main__":
    main()
