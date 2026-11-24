#!/usr/bin/env python3
"""Write a reviewable Studio look layer using measured existing demo pockets.

Explicit output is required. Existing output is preserved; this never edits the
purchased meshes, source placements, saved graphics settings or world.studio.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'projects/valwood_world/scenery/demo_polish_manifest.json'


def compose():
    source = json.loads(SOURCE.read_text())
    # Preserve map UVs and source normal scales. Multipliers refine the response
    # without flattening the authored maps or manufacturing texture detail.
    materials = [
        ('purchased_house_a', (1.01, 1.0, .98), 1.06, .90),
        ('purchased_house_b', (1.0, .99, .97), 1.06, .90),
        ('purchased_house_e', (1.0, 1.0, .99), 1.06, .90),
        ('purchased_shop_d', (1.0, .99, .97), 1.04, .92),
        ('village_tavern', (1.0, .98, .96), 1.04, .92),
        ('bep_nature_fern', (.94, .99, .91), .96, 1.0),
        ('bep_nature_bush_a', (.98, 1.0, .93), 1.02, .96),
        ('bep_nature_bush_b', (.94, .98, .92), 1.02, .96),
        ('bep_nature_clover', (.98, 1.0, .94), 1.0, 1.0),
        ('bep_nature_grass_broad', (.96, .99, .90), 1.0, 1.0),
        ('rock0', (.98, 1.0, .98), 1.06, .94),
        ('rock1', (.98, 1.0, .98), 1.06, .94),
        ('showcase_roots', (1.0, .98, .95), 1.04, .94),
    ]
    lines = ['VALWOOD_STUDIO 2',
             'look 0.26 1.0 1.06 1.025 17.05 0 .28 .48 .85 .62']
    for alias, tint, roughness, normal in materials:
        lines.append('material ' + alias + ' ' + ' '.join(map(str, (*tint, roughness, normal, 1))))
    patches = []
    # Small low-opacity moss/soil transitions follow surveyed planted pockets.
    # No collision, new textures, lights, altered landforms or extra meshes.
    for zone, indices, size, color, opacity in [
        ('arcade_footing_plants', [0, 24], (2.6, .55, 1.6), (.09, .115, .06), .16),
        ('street_soft_edges', [-1], (2.8, .55, 1.8), (.10, .075, .045), .17),
        ('jungle_layered_floor', [0, 73], (3.8, 1.4, 3.2), (.075, .10, .045), .18),
        ('demo_trail_edge_groups', [30, -1], (2.2, .65, 1.5), (.085, .06, .035), .18),
    ]:
        anchors = [row for row in source['placements'] if row['zone'] == zone]
        for index in indices:
            row = anchors[index]
            x, z = row['world_xz']
            y = (row['ground_height_min_m'] + row['ground_height_max_m']) * .5
            values = [x, y, z, *size, *color, (len(patches) * 31) % 180,
                      opacity, .82, 1 if zone.startswith('jungle') else 0]
            lines.append('decal ' + ' '.join(map(str, values)))
            patches.append(dict(anchor=row['id'], zone=zone, values=values))
    text = '\n'.join(lines) + '\n'
    return text, dict(source=str(SOURCE.relative_to(ROOT)),
                      source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                      document_sha256=hashlib.sha256(text.encode()).hexdigest(),
                      materials=len(materials), patches=patches,
                      new_meshes=0, new_images=0,
                      notes='Modest response and ground integration; retains canopy, fog, NPCs and all navigation.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text, manifest = compose()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        stream.write(text)
    args.output.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(args.output)


if __name__ == '__main__':
    main()
