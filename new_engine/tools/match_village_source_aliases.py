#!/usr/bin/env python3
"""Fingerprint renamed purchased FBXs against exact UE local bounds/slots."""
from pathlib import Path
import argparse
import json
import re
import numpy as np

PRIVATE = Path(__file__).resolve().parents[2] / '.local-tools/village-source'


def normalized(name):
    value = re.sub('[^a-z0-9]', '', name.lower().replace('ı', 'i').replace('tileable', ''))
    return 'mirope' if value == 'mirotatedrope' else value


def material_slots(row, slots):
    mapping, evidence = {}, []
    for part in row['parts']:
        hits = [i for i, slot in enumerate(slots)
                if normalized(slot['MaterialInterface']) == normalized(part['material'])]
        if not hits and len(slots) == 1:
            hits = [0]
            evidence.append('Single UE material assignment overrides the exported FBX default')
        if not hits and part['material'] == 'WorldGridMaterial':
            hits = [i for i, slot in enumerate(slots) if slot['MaterialInterface'] == 'MM_Simple_Black']
            evidence.append('Vendor unassigned WorldGrid geometry uses the explicit UE black slot')
        if not hits or len({slots[i]['MaterialInterface'] for i in hits}) != 1:
            raise ValueError(f"{row['name']}: unresolved FBX material {part['material']}")
        mapping[str(part['slot'])] = min(hits)
        if len(hits) > 1:
            evidence.append('Equivalent duplicate UE slots share the same material instance')
    unused = [dict(slot=i, material=slot['MaterialInterface'], imported_name=slot['ImportedMaterialSlotName'])
              for i, slot in enumerate(slots) if i not in mapping.values()]
    return mapping, sorted(set(evidence)), unused


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private', type=Path, default=PRIVATE)
    args = parser.parse_args()
    base = args.private
    raw = json.loads((base / 'fbx-raw/meshes.json').read_text())['meshes']
    bounds = json.loads((base / 'mesh-bounds.json').read_text())['meshes']
    materials = json.loads((base / 'material-registry.json').read_text())['meshes']
    scene = json.loads((base / 'publisher-scene.json').read_text())
    candidates = []
    for row in raw:
        if row['name'].startswith('UCX_') or ('_LOD' in row['name'] and not row['name'].endswith('_LOD0')):
            continue
        vertices = np.fromfile(base / 'fbx-raw' / row['vertices'], dtype='<f4').reshape(-1, 18)[:, :3]
        candidates.append((row, np.array([vertices.min(0), vertices.max(0)])))
    aliases, skipped = {}, {}
    for name in scene['mesh_counts']:
        if name in {row['mesh_name'] for row, _ in candidates}:
            continue
        if name not in bounds:
            skipped[name] = 'No local purchased static mesh bounds; engine-native helper remains explicit'
            continue
        target = np.array(bounds[name]['bounds_unreal_cm'])
        near = [(row, float(np.abs(value - target).max())) for row, value in candidates
                if float(np.abs(value - target).max()) < .001]
        slots = materials[name]['slots']
        target_names = [normalized(v['MaterialInterface']) for v in slots]
        matches = []
        for row, residual in near:
            mapping, unused, unresolved = {}, set(range(len(slots))), []
            for part in row['parts']:
                hits = [i for i in unused if target_names[i] == normalized(part['material'])]
                if len(hits) == 1:
                    mapping[str(part['slot'])] = hits[0]
                    unused.remove(hits[0])
                else:
                    unresolved.append(part)
            fallback = False
            if len(unresolved) == len(unused) == 1 and unresolved[0]['material'] == 'WorldGridMaterial':
                mapping[str(unresolved[0]['slot'])] = unused.pop()
                unresolved.clear()
                fallback = True
            if not unresolved and not unused:
                matches.append((row, residual, mapping, fallback))
        alternatives = []
        if len(matches) > 1:
            expected = ('SM_Castle_' + name.removeprefix('SM_')) if name.startswith('SM_Boundary_') else None
            preferred = [m for m in matches if m[0]['mesh_name'] == expected]
            if len(preferred) == 1:
                alternatives = [m[0]['mesh_name'] for m in matches if m is not preferred[0]]
                matches = preferred
        if len(matches) != 1:
            raise ValueError(f'{name}: expected one bounds+material match, found {len(matches)}')
        row, residual, mapping, fallback = matches[0]
        aliases[name] = dict(fbx_mesh_name=row['mesh_name'], fbx_lod0_name=row['name'],
            bounds_max_error_unreal_cm=residual, fbx_slot_to_ue_slot=mapping,
            source_materials=[v['MaterialInterface'] for v in slots],
            evidence='Unique bounds within0.001cm plus normalized material identity',
            other_geometry_within_tolerance=alternatives,
            naming_evidence='Source Boundary name retains the supplied Castle_Boundary suffix' if alternatives else None,
            exported_unassigned_slot=fallback,
            fallback_evidence='One vendor WorldGridMaterial slot maps to the sole remaining UE material slot' if fallback else None)
    # Validate named families too: the vendor FBX exporter reordered house,
    # shop and hitching-post material indices relative to UE StaticMaterials.
    families = set(scene['mesh_counts']) | {r['mesh']['name'] for r in scene['native_instance_components'] if r.get('mesh')}
    for name in sorted(families):
        if name == 'Plane':
            continue
        fbx_name = aliases.get(name, {}).get('fbx_mesh_name', name)
        rows = [r for r in raw if r['mesh_name'] == fbx_name and not r['name'].startswith('UCX_')]
        if not rows or name not in materials:
            raise ValueError('Missing named family/material table: ' + name)
        lod_maps, notes, unused = {}, [], []
        for row in rows:
            lod = row['name'].rsplit('_LOD', 1)[1] if '_LOD' in row['name'] else '0'
            if not lod.isdigit():
                raise ValueError('Unexpected source LOD name: ' + row['name'])
            value, proof, omitted = material_slots(row, materials[name]['slots'])
            if lod in lod_maps:
                raise ValueError('Duplicate source LOD family: ' + name)
            lod_maps[lod] = value
            notes.extend(proof)
            if lod == '0':
                unused = omitted
        entry = aliases.setdefault(name, dict(fbx_mesh_name=fbx_name,
            fbx_lod0_name=next(r['name'] for r in rows if '_LOD' not in r['name'] or r['name'].endswith('_LOD0')),
            source_materials=[v['MaterialInterface'] for v in materials[name]['slots']],
            evidence='Exact source mesh name and per-material identities; numeric FBX order is not assumed'))
        entry.update(fbx_slot_to_ue_slot=lod_maps['0'], fbx_lod_slot_to_ue_slot=lod_maps,
                     slot_notes=sorted(set(notes)), unused_source_slots=unused)
        if any(v['material'] == 'MI_Roof2_2' for v in unused):
            entry['export_limitation'] = 'Supplied FBX has one merged Roof2 slot; separate UE Roof2_2 face assignment is unavailable in that export. Canonical Roof2 material is retained.'
    output = base / 'source-aliases.json'
    output.write_text(json.dumps(dict(aliases=aliases, skipped=skipped,
        source_unmodified=True, tolerance_unreal_cm=.001), indent=2) + '\n')
    print(json.dumps(dict(aliases=len(aliases), skipped=skipped, output=str(output)), indent=2))


if __name__ == '__main__':
    main()
