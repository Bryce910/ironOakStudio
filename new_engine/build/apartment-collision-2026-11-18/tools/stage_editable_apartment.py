#!/usr/bin/env python3
"""Stage individually selectable purchased apartment props without touching a live project.

The purchased Unity scene is the source of truth. Building surfaces stay in
small static chunks; each nonstructural prefab root becomes one authored model
and placement at its exact original transform. The output is staged for review
and must be installed into an editor project separately.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

from cook_finward_scene import (
    Source, SourceProbeSamples, SceneMaterials, LightmapSampler,
    emit_chunk, partitions, prepare_unit, units_for_scene,
)

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "assets/purchased/finward_house_props"
HOME = (-3500., 600., -11000.)


def structural(source: Source, unit: dict) -> bool:
    def shell(placement: dict) -> bool:
        mesh = source.meshes[placement["mesh"]]
        return ("/Meshes_building/" in mesh.get("source", "") or
                "/Prefabs/Building/" in placement.get("source_prefab", ""))
    values = {shell(placement) for placement in unit["placements"]}
    if len(values) != 1:
        raise ValueError("Mixed building and furnishing assembly")
    return values.pop()


def stem_for(unit: dict, index: int) -> str:
    placement = next(p for p in unit["placements"] if p["lod_mask"] & 1)
    name = re.sub(r"_LOD\d+$", "", placement["name"], flags=re.I)
    name = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")[:32] or "prop"
    return f"item_{index:04d}_{name}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Choose a new staging directory")
    source = Source(DATA / "scene_export/scene.json")
    units = units_for_scene(source)
    shell_units = [u for u in units if structural(source, u)]
    prop_units = [u for u in units if not structural(source, u)]
    if len(units) != 701 or len(shell_units) != 115 or len(prop_units) != 586:
        raise ValueError("Purchased scene inventory changed")
    source_review = json.loads((DATA / "scene-cooked-v3/COOK_REVIEW.json").read_text())
    sampler = LightmapSampler(Path(source_review["source_lightmap_manifest"]))
    probes = SourceProbeSamples(source, Path(source_review["source_lightprobe_manifest"]), sampler.registry)
    output.mkdir(parents=True)
    models = output / "models/opening/editable"
    models.mkdir(parents=True)
    near_folder, mid_folder = output / "near_materials", output / "mid_materials"
    near_folder.mkdir(); mid_folder.mkdir()
    near = SceneMaterials(source, near_folder, 512, DATA / "scene-cooked-v3", "near")
    mid = SceneMaterials(source, mid_folder, 128, DATA / "scene-cooked-v3", "mid")
    counters = collections.Counter()
    records = []

    def cook(stem: str, alias: str, grouped: list[dict], kind: str) -> None:
        pieces = []
        for unit in grouped:
            pieces.extend(prepare_unit(source, unit, sampler, .12, counters, probes))
        result = emit_chunk(stem, pieces, near, mid, models, source, counters)
        src_near = models / f"{stem}_near.glb"
        dst_near = models / f"{stem}.glb"
        src_near.rename(dst_near)
        record = dict(alias=alias, stem=f"opening/editable/{stem}", kind=kind,
                      origin_native=result["origin_native"],
                      source_assemblies=[u["key"] for u in grouped],
                      near_bytes=dst_near.stat().st_size,
                      mid_bytes=(models / f"{stem}_mid.glb").stat().st_size,
                      near_sha256=hashlib.sha256(dst_near.read_bytes()).hexdigest())
        records.append(record)
        if len(records) % 25 == 0:
            print(f"Staged {len(records)} / {len(prop_units) + len(shell_chunks)} models", flush=True)

    shell_chunks = partitions(shell_units, 16., 24, 16)
    for index, (_, grouped) in enumerate(shell_chunks):
        stem = f"shell_{index:02d}"
        alias = "nm_opening_home" if index == 0 else f"nm_opening_reference_{stem}"
        cook(stem, alias, grouped, "structure")
    for index, unit in enumerate(prop_units):
        stem = stem_for(unit, index)
        cook(stem, f"nm_opening_reference_{stem}", [unit], "furnishing")

    # Models embed their exact cooked materials. The temporary PNG material
    # cache is not part of the staged project and can be discarded.
    shutil.rmtree(near_folder)
    shutil.rmtree(mid_folder)
    assets = []
    instances = []
    for record in records:
        x, y, z = record["origin_native"]
        world_x, world_z = HOME[0] + x, HOME[2] + z
        # Match world_atlas_uv_to_world exactly; rounding this depth moves
        # the apartment away from its world-space collision and room controls.
        nx, nz = .5 + world_x / 100800., .5 + world_z / (100800. * 2818. / 5009.)
        assets.append(f"asset {record['alias']} {record['stem']} 2 0 0 240 75 1")
        instances.append(" ".join((record["alias"], format(nx, ".17g"), format(nz, ".17g"),
                                   "0", "1", "0", "0", "0", "0", "0", "1", "1", "1",
                                   format(HOME[1] + y, ".17g"))))
    (output / "assets.txt").write_text("\n".join(assets) + "\n")
    (output / "instances.txt").write_text("\n".join(instances) + "\n")
    (output / "manifest.json").write_text(json.dumps(dict(
        schema="valwood.editable-apartment.v1", source_scene=str(source.path),
        source_sha256=hashlib.sha256(source.path.read_bytes()).hexdigest(),
        structure_chunks=len(shell_chunks), furniture=len(prop_units),
        source_logical_assemblies=len(units), records=records), indent=2) + "\n")
    print(f"Staged {len(prop_units)} individual furnishings and {len(shell_chunks)} structural chunks in {output}")


if __name__ == "__main__":
    main()
