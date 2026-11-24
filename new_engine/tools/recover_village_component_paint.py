#!/usr/bin/env python3
"""Recover this purchased UE4.26 map's per-instance vertex paint as data.

Checks every record boundary and both serialized color copies. Output retains
local Unreal centimetre positions/normals so an exporter can match geometry
without assuming that an FBX and the render buffer share vertex indices.
Purchased files and playable projects are never written.
"""
from pathlib import Path
import argparse
import hashlib
import json
import mmap
import struct
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent / '.local-tools/village-source'
VERTEX = np.dtype([('position', '<f4', (3,)), ('normal', '<f4', (4,)),
                   ('bgra', 'u1', (4,))])


def parse_tail(raw, start, size):
    end = start + size
    cursor = start

    def take(count):
        nonlocal cursor
        if count < 0 or cursor + count > end:
            raise ValueError('Serialized paint record crosses component boundary')
        value = raw[cursor:cursor + count]
        cursor += count
        return value

    def number():
        return struct.unpack('<I', take(4))[0]

    if number() != 0:
        raise ValueError('Unsupported component native prefix')
    count = number()
    if count > 8:
        raise ValueError('Unexpected component LOD count')
    lods = []
    for index in range(count):
        strip = take(2)
        guid = take(16).hex()
        override = take(1)[0]
        if strip != b'\0\0' or override not in (0, 1):
            raise ValueError('Unsupported LOD strip/override flags')
        colors = None
        if override:
            if take(2) != b'\0\0':
                raise ValueError('Unsupported color buffer strip flags')
            stride, vertices, item_size, items = (number() for _ in range(4))
            if stride != 4 or item_size != 4 or vertices != items or items > 1000000:
                raise ValueError('Invalid serialized color buffer shape')
            colors = np.frombuffer(take(items * 4), dtype='u1').reshape(items, 4)
        painted = number()
        if painted > 1000000:
            raise ValueError('Invalid painted vertex count')
        records = np.frombuffer(take(painted * VERTEX.itemsize), dtype=VERTEX)
        if not np.isfinite(records['position']).all() or not np.isfinite(records['normal']).all():
            raise ValueError('Nonfinite source vertex')
        if colors is not None and (painted != len(colors) or not np.array_equal(colors, records['bgra'])):
            raise ValueError('Painted vertex/color buffer copies disagree')
        lods.append(dict(index=index, guid=guid, override=bool(override), records=records,
                         vertices=0 if colors is None else len(colors)))
    if cursor != end:
        raise ValueError(f'Unconsumed native tail: {end - cursor} bytes')
    return lods


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene', type=Path, default=PRIVATE / 'publisher-scene.json')
    parser.add_argument('--output', type=Path, default=PRIVATE / 'component-paint')
    args = parser.parse_args()
    scene = json.loads(args.scene.read_text())
    source = Path(scene['source'])
    args.output.mkdir(parents=True, exist_ok=True)
    variants, components, errors, scanned = {}, {}, [], 0
    with source.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as raw:
        if hashlib.sha256(raw).hexdigest() != scene['source_sha256']:
            raise ValueError('Map changed since the scene audit')
        for component in scene['placements']:
            if component['class_name'] != 'StaticMeshComponent':
                continue
            scanned += 1
            try:
                lods = parse_tail(raw, component['native_tail_offset'], component['native_tail_bytes'])
                if not lods or not lods[0]['override']:
                    continue
                records = lods[0]['records']
                rgba = records['bgra'][:, [2, 1, 0, 3]].copy()
                position = records['position'].copy()
                normal = records['normal'].copy()
                arrays = dict(position_unreal_cm=position, normal_unreal=normal, rgba=rgba)
                fingerprint = hashlib.sha256()
                for lod in lods:
                    fingerprint.update(bytes((lod['index'], int(lod['override']))))
                    if not lod['override']:
                        continue
                    values = lod['records']
                    lp = values['position'].copy()
                    ln = values['normal'].copy()
                    lc = values['bgra'][:, [2, 1, 0, 3]].copy()
                    arrays.update({f"lod{lod['index']}_position_unreal_cm": lp,
                                   f"lod{lod['index']}_normal_unreal": ln,
                                   f"lod{lod['index']}_rgba": lc})
                    fingerprint.update(lp.tobytes() + ln.tobytes() + lc.tobytes())
                key = fingerprint.hexdigest()
                if key not in variants:
                    target = args.output / (key + '.npz')
                    np.savez_compressed(target, **arrays)
                    variants[key] = dict(path=str(target.resolve()), sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                        vertices=len(records), rgba_min=rgba.min(0).tolist(), rgba_max=rgba.max(0).tolist(),
                        unique_rgba=len(np.unique(rgba, axis=0)),
                        bounds_unreal_cm=[position.min(0).tolist(), position.max(0).tolist()])
                components[str(component['export_index'])] = dict(variant=key,
                    mesh=component.get('mesh'), actor=component.get('actor_label'),
                    lods=[{k: v for k, v in lod.items() if k != 'records'} for lod in lods])
            except (ValueError, struct.error) as error:
                errors.append(dict(export_index=component['export_index'], actor=component.get('actor_label'),
                                   error=str(error), native_tail_bytes=component['native_tail_bytes']))
    result = dict(version=1, source=str(source.resolve()), source_sha256=scene['source_sha256'],
        scanned_components=scanned, painted_components=len(components), variants=variants,
        components=components, errors=errors, coordinate_system='Unreal Xforward/Yright/Zup centimetres',
        channel_order='canonical RGBA, source FColor BGRA exchanged exactly once',
        geometry_binding='Match source local position and normal; never assume FBX vertex ordering',
        lod_arrays='lodN_position_unreal_cm, lodN_normal_unreal, lodN_rgba for every overridden LOD; unprefixed arrays alias LOD0',
        purchased_source_unmodified=True, all_serialized_color_copies_match=not errors)
    (args.output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ['scanned_components', 'painted_components', 'errors']} |
                     {'paint_variants': len(variants), 'output': str(args.output)}, indent=2))
    if errors:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
