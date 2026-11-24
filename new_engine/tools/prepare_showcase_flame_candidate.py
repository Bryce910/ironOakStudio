#!/usr/bin/env python3
"""Prepare source-verified practical flames without editing the running project.

Fuel positions come from the purchased prefab's Ember transform, transformed
through each exported visible holder mesh. Point lights are validation oracles,
never the emitter placement source. This script only writes candidate files.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT.parent / '.local-tools/unity-source'
OUT = ROOT / 'docs/stronghold/2026-11-01/visual-finish/flame-candidate'
ORIGIN = np.array([-43344., 1130.8125, -23987.82415651827])
FAMILIES = {
    'Prefab_Forged_torch_complete_01_on': 'Forged_torch_complete_01_LOD0',
    'Prefab_Forged_torch_complete_02_on': 'Forged_torch_complete_02_LOD0',
    'Prefab_Brazier_01_on': 'Brazier_01_LOD0',
    'Prefab_Brazier_02_on': 'Brazier_02_LOD0',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def documents(path):
    blocks = re.split(r'(?m)^--- !u!(\d+) &([0-9]+)(?: stripped)?\n', path.read_text())
    result = {}
    for at in range(1, len(blocks), 3):
        kind, ident = int(blocks[at]), int(blocks[at + 1])
        data = yaml.load(blocks[at + 2], Loader=yaml.CSafeLoader)
        result[ident] = (kind, next(iter(data.values())))
    return result


def trs(transform):
    p, q, scale = [transform[name] for name in ('m_LocalPosition', 'm_LocalRotation', 'm_LocalScale')]
    x, y, z, w = np.array([q[a] for a in 'xyzw']) / np.linalg.norm([q[a] for a in 'xyzw'])
    matrix = np.eye(4)
    matrix[:3, :3] = np.array([
        [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)],
    ]) @ np.diag([scale[a] for a in 'xyz'])
    matrix[:3, 3] = [p[a] for a in 'xyz']
    return matrix


def prefab(path, mesh_name):
    docs = documents(path)
    objects = {i: d for i, (k, d) in docs.items() if k == 1}
    transforms = {i: d for i, (k, d) in docs.items() if k == 4 and 'm_Father' in d}
    by_name = {objects[d['m_GameObject']['fileID']]['m_Name']: i for i, d in transforms.items()}

    def relative(ident):
        t = transforms[ident]
        parent = t['m_Father']['fileID']
        # The instance root replaces the prefab asset's authoring position.
        return np.eye(4) if not parent else relative(parent) @ trs(t)

    light_object = next(d['m_GameObject']['fileID'] for k, d in docs.values() if k == 108)
    light_transform = next(i for i, t in transforms.items() if t['m_GameObject']['fileID'] == light_object)
    inverse_mesh = np.linalg.inv(relative(by_name[mesh_name]))
    return {
        'fuel': inverse_mesh @ relative(by_name['Fire_Torch_Embers_01']),
        'flame': inverse_mesh @ relative(by_name['Fire_Torch_Flames_01']),
        'light': inverse_mesh @ relative(light_transform),
        'fuel_transform_id': by_name['Fire_Torch_Embers_01'],
        'flame_transform_id': by_name['Fire_Torch_Flames_01'],
        'path': str(path), 'sha256': digest(path),
    }


def native(point):
    return (ORIGIN + np.array([point[0]-500, point[1], 500-point[2]])).tolist()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--project', type=Path, default=ROOT / 'projects/valwood_showcase')
    args = parser.parse_args()
    output, project = args.output.resolve(), args.project.resolve()
    if output == project or project in output.parents:
        raise ValueError('Candidate output must be outside the production project')
    output.mkdir(parents=True, exist_ok=True)
    scene_path = PRIVATE / 'showcase-native/scene.json'
    scene = json.loads(scene_path.read_text())
    folder = PRIVATE / 'asset-bridge/Assets/NatureManufacture Assets/Castle and Dungeon/Architecture/Torches'
    local = {name: prefab(folder / (name + '.prefab'), mesh) for name, mesh in FAMILIES.items()}
    records, seen = [], set()
    for placement in scene['placements']:
        if not placement['lod_mask'] & 1:
            continue
        family = next((name for name, mesh in FAMILIES.items()
                       if placement['name'] == mesh and re.search(r'/' + re.escape(name) + r'(?: \(\d+\))?/', placement['hierarchy'])), None)
        if not family:
            continue
        world = np.array(placement['matrix']).reshape(4, 4, order='F')
        points = {key: (world @ local[family][key])[:3, 3] for key in ('fuel', 'flame', 'light')}
        key = tuple(round(float(v), 5) for v in points['fuel'])
        if key in seen:
            raise ValueError('Duplicate source fuel point: ' + placement['hierarchy'])
        seen.add(key)
        hierarchy = placement['hierarchy'].rsplit('/', 1)[0]
        matches = [light for light in scene['lights'] if light['type'] == 'Point' and light['hierarchy'].startswith(hierarchy + '/')]
        if not matches:
            raise ValueError('Holder has no authored point-light validation: ' + hierarchy)
        match = min(matches, key=lambda light: np.linalg.norm(points['light'] - np.array(light['matrix'][12:15])))
        error = float(np.linalg.norm(points['light'] - np.array(match['matrix'][12:15])))
        # One source instance explicitly moves its lightY from.569 to.570;
        # none modifies the fuel/ember transform. Float export adds tiny error.
        if error > .005:
            raise ValueError(f'Holder/prefab transform mismatch {error}: {hierarchy}')
        brazier = 'Brazier' in family
        position = native(points['fuel'])
        seed = int(hashlib.sha256(hierarchy.encode()).hexdigest()[:8], 16)
        records.append(dict(
            hierarchy=hierarchy, group=placement['group'], family=family,
            source_fuel_position=points['fuel'].tolist(), source_flame_billboard_position=points['flame'].tolist(),
            source_point_light_position=match['matrix'][12:15], point_light_validation_error_m=error,
            fuel_to_light_distance_m=float(np.linalg.norm(points['fuel'] - np.array(match['matrix'][12:15]))),
            native_position=position, height=.65 if brazier else .40, width=.28 if brazier else .13,
            intensity=.65, dynamic_light_power=0., suggested_future_actor_only_light_power=.35 if brazier else .15,
            seed=seed, source_prefab=local[family]['path'], source_prefab_sha256=local[family]['sha256'],
            fuel_transform_id=local[family]['fuel_transform_id'], flame_transform_id=local[family]['flame_transform_id'],
        ))
    records.sort(key=lambda r: (r['group'], r['hierarchy']))
    # Ten wall torches and six matching braziers in the inhabited lower hall.
    # This exactly fits the current16-effect Studio document budget.
    selected = [r for r in records if r['group'] == 'Temple_Complex'
                and 232 <= r['source_fuel_position'][0] <= 246
                and 612 <= r['source_fuel_position'][2] <= 674.5
                and 153 <= r['source_fuel_position'][1] <= 158]
    if len(selected) != 16:
        raise ValueError(f'Lower-hall candidate must have exactly16 emitters, got{len(selected)}')
    rows = []
    for r in selected:
        x, y, z = r['native_position']
        rows.append(f"particle -1 2 1 {x:.12g} {y:.12g} {z:.12g} {y+3.2*r['height']+.2:.12g} "
                    f"{r['width']} {r['height']} {r['intensity']} .8 1 1 1 .2 0 {r['seed']} 1 1 1 0")
    original = project / 'world.studio'
    before = digest(original)
    text = original.read_text()
    if not text.startswith('VALWOOD_STUDIO 7\n') or re.search(r'(?m)^particle ', text):
        raise ValueError('Expected current empty-effect v7 showcase document')
    (output / 'world.studio').write_text(text.rstrip() + '\n' + '\n'.join(rows) + '\n')
    (output / 'particles.txt').write_text('\n'.join(rows) + '\n')
    source_rows = ['VALWOOD_SOURCE_FLAMES 1', str(len(records))]
    for r in records:
        x, y, z = r['native_position']
        source_rows.append(f"{x:.12g} {y:.12g} {z:.12g} {r['height']} {r['width']} {r['intensity']} {r['seed']}")
    (output / 'showcase.flames').write_text('\n'.join(source_rows) + '\n')
    base_count = 3 + sum(len(re.findall(r'(?m)^\s*\{', (ROOT / 'engine' / name).read_text()))
                         for name in ('world_village_torches.inc', 'world_castle_streets_torches.inc', 'world_bandit_torches.inc'))
    if base_count + len(selected) > 56:
        raise ValueError('Candidate exceeds the current shared flame renderer capacity')
    report = dict(version=1, scene=str(scene_path), source_scene_sha256=digest(scene_path),
                  source_emitters=len(records), by_family=dict(collections.Counter(r['family'] for r in records)),
                  placement_method='Exact prefab Ember fuel-mouth transform through exported holder mesh world matrix; light position used only for independent validation',
                  maximum_light_validation_error_m=max(r['point_light_validation_error_m'] for r in records),
                  current_studio_effect_limit=16, renderer_total_limit=56, current_base_emitters=base_count, candidate_total_emitters=base_count+len(selected),
                  selected_count=len(selected), proposed_source_pool_capacity=256, proposed_source_pool_emitters=len(records), static_baked_illumination_unchanged=True,
                  runtime_light_policy='Zero dynamic power until actor-only light masking exists; no duplicate static irradiance',
                  production_document_sha256_before=before, production_document_sha256_after=digest(original),
                  selected=selected, all_emitters=records)
    if report['production_document_sha256_before'] != report['production_document_sha256_after']:
        raise ValueError('Production document changed while preparing candidate')
    (output / 'flames.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('selected', 'all_emitters')}, indent=2))


if __name__ == '__main__':
    main()
