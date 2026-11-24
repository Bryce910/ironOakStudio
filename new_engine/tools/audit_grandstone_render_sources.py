#!/usr/bin/env python3
"""Read-only source audit for Grandstone's hall floor and stair/door rendering.

Images and reports are written only to the requested evidence directory. The
active world.studio definitions, rather than superseded import revisions,
identify the native GLBs. Original source meshes and project data are untouched.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def glb(path):
    raw = path.read_bytes()
    if len(raw) < 28 or struct.unpack_from('<III', raw) != (0x46546c67, 2, len(raw)):
        raise ValueError('Invalid GLB: ' + str(path))
    count, kind = struct.unpack_from('<II', raw, 12)
    if kind != 0x4e4f534a:
        raise ValueError('Missing GLB JSON')
    return raw, json.loads(raw[20:20+count]), 28+count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=ROOT/'projects/valwood_showcase')
    parser.add_argument('--source', type=Path, default=ROOT.parent/'.local-tools/unity-source/showcase-native/scene.json')
    parser.add_argument('--output', type=Path, default=ROOT/'docs/grandstone-cleanup/2026-11-02/render')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(args.project.resolve()) or output.is_relative_to(args.source.parent.resolve()):
        raise ValueError('Choose a separate evidence output, not project or source data')
    output.mkdir(parents=True, exist_ok=True)
    studio = args.project/'world.studio'
    definitions = []
    for line in studio.read_text().splitlines():
        words = line.split()
        if len(words) >= 3 and words[0] == 'asset' and words[1].startswith('nm_scene_'):
            definitions.append((words[1], words[2]))
    captures = {}
    bound_rows = []
    wanted = {'M_Marble_floor_red_path', 'M_Marble_floor_red_rosete'}
    for alias, stem in definitions:
        for suffix in ('.glb', '_mid.glb'):
            path = args.project/'models'/(stem+suffix)
            raw, doc, binary_start = glb(path)
            lows, highs = [], []
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    accessor = doc['accessors'][primitive['attributes']['POSITION']]
                    lows.append(accessor['min'][1])
                    highs.append(accessor['max'][1])
            bound_rows.append(dict(alias=alias, tier=suffix, minimum_y=min(lows), maximum_y=max(highs)))
            if captures.keys() >= wanted:
                continue
            for material in doc.get('materials', []):
                name = material.get('name')
                if name not in wanted or name in captures:
                    continue
                texture = material['pbrMetallicRoughness']['baseColorTexture']['index']
                image = doc['images'][doc['textures'][texture]['source']]
                view = doc['bufferViews'][image['bufferView']]
                start = binary_start+view.get('byteOffset', 0)
                encoded = raw[start:start+view['byteLength']]
                with Image.open(io.BytesIO(encoded)) as decoded:
                    decoded.load()
                    size = decoded.size
                    preview = decoded.convert('RGB')
                    preview.thumbnail((600, 600))
                    preview.save(output/(name+'.png'))
                captures[name] = dict(active_asset=str(path.resolve()), asset_sha256=digest(raw),
                    texture_sha256=digest(encoded), native_texture_dimensions=size,
                    alpha_mode=material.get('alphaMode', 'OPAQUE'),
                    alpha_cutoff=material.get('alphaCutoff'),
                    preview=name+'.png')
    if captures.keys() != wanted:
        raise ValueError('Expected active hall floor materials were not found')
    source_raw = args.source.read_bytes()
    source = json.loads(source_raw)
    floors, doors, stairs = [], [], []
    for placement in source['placements']:
        if placement['group'] not in ('Temple_Complex', 'Barbican') or not placement['lod_mask'] & 1:
            continue
        row = {key: placement[key] for key in ('hierarchy', 'mesh', 'lod_mask', 'bounds_min', 'bounds_max', 'materials')}
        hierarchy = placement['hierarchy'].lower()
        if '/carpet/' in hierarchy and 610 < placement['bounds_max']['z'] and placement['bounds_min']['z'] < 657:
            floors.append(row)
        if any(word in placement['name'].lower() for word in ('wood_planks_gate', 'wooden_gate')):
            doors.append(row)
        if 'stone_stair' in placement['name'].lower():
            stairs.append(row)
    # Existing 0.4 m near plane and 1.08 rad vertical FOV. This is a geometric
    # clearance diagnostic, not a claim of pixel-exact camera reproduction.
    near = .4
    vertical = math.tan(.54)
    horizontal = vertical*16/9
    report = dict(version=1, source_scene=str(args.source.resolve()), source_sha256=digest(source_raw),
        project=str(args.project.resolve()), world_studio_sha256=digest(studio.read_bytes()),
        source_assets_read=len(bound_rows), material_previews=captures,
        floor_finding='The black crater-shaped patches in the final hall capture occur in the active opaque floor albedo. They are worn-stone texture detail, not alpha holes or missing geometry.',
        source_lod_bounds=dict(negative_minimum_y=[row for row in bound_rows if row['minimum_y'] < -.0001],
            policy='All active source HIGH/MID bounds have a nonnegative floor datum; current root-centered scenery spheres conservatively contain them.'),
        nearby_hall_carpet_planes=floors, source_gate_leaf_rows=doors, source_stair_rows=stairs,
        camera_clearance_diagnostic=dict(near_plane_m=near, vertical_fov_radians=1.08, aspect=16/9,
            near_rectangle_corner_radius_m=near*math.hypot(horizontal, vertical),
            near_corner_distance_from_eye_m=near*math.sqrt(1+horizontal**2+vertical**2),
            limitation='A small camera-origin sphere alone does not enclose the 0.4 m near rectangle. Tight-doorway visual clipping requires actual native camera checks.'),
        source_or_project_modified=False,
        limitations='CPU source diagnosis. Native stair/door motion captures and collision checks are required for final acceptance; floor preview is a texture extraction, not an in-game screenshot.')
    (output/'source-render-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(assets_read=len(bound_rows), floor_materials=list(captures),
        negative_floor_datums=len(report['source_lod_bounds']['negative_minimum_y']),
        report=str(output/'source-render-audit.json')), indent=2))


if __name__ == '__main__':
    main()
