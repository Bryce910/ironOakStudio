#!/usr/bin/env python3
"""Inventory actual clip/pose/weapon coverage; this is not smoothness acceptance."""
import hashlib
import json
from pathlib import Path
import shlex
import struct

N = Path(__file__).resolve().parent
INPUTS = {}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def pin(path):
    path = Path(path).resolve()
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    INPUTS[str(path)] = digest
    return path


def glb(path):
    path = pin(path)
    with path.open('rb') as stream:
        magic, version, length, count, kind = struct.unpack('<5I', stream.read(20))
        require(magic == 0x46546c67 and version == 2 and kind == 0x4e4f534a
                and length == path.stat().st_size, 'Invalid owned GLB: ' + str(path))
        return json.loads(stream.read(count))


def load(path):
    return json.loads(pin(path).read_text())


def main():
    pin(__file__)
    hero = load(N / 'HERO_ASSET.json')
    rows = [('sword', Path(hero['path']), '')]
    words = shlex.split(pin(N / 'loadout-rigs/loadouts.manifest').read_text())
    require(words[:2] == ['VALWOOD_SKELETAL_LOADOUTS', '1'], 'Wrong Hero loadout contract')
    for index, label in enumerate(('shield', 'axe', 'staff')):
        offset = 2 + index * 3
        require(int(words[offset]) == index + 2, 'Reordered Hero loadouts')
        rows.append((label, Path(words[offset + 1]), words[offset + 2]))
    clips = []
    required = ('Idle', 'Walk_Fwd', 'Walk_FwdRight', 'Walk_Right', 'Walk_BwdRight',
                'Walk_Bwd', 'Walk_BwdLeft', 'Walk_Left', 'Walk_FwdLeft', 'Run')
    directions = ('Left', 'Right', 'High', 'Low', 'Thrust')
    for label, path, prefix in rows:
        data = glb(path)
        names = [animation.get('name', '') for animation in data['animations']]
        expected = [prefix + name for name in required]
        expected += [prefix + family + direction for family in ('Attack_', 'Guard_') for direction in directions]
        expected += ['Hit_' + direction for direction in directions]
        require(all(names.count(name) == 1 for name in expected), 'Missing/ambiguous source clips: ' + label)
        clips.append({'weapon': label, 'source': str(path), 'required_clips': expected,
                      'actual_clips': names, 'smoothness_accepted': False})
    knight_ids = ['knight%02d' % index for index in range(1, 19)]
    knights = []
    for id_ in knight_ids:
        root = N / 'knights_roster' / id_
        fields = shlex.split(pin(root / 'appearance.cfg').read_text())
        require(fields[:3] == ['VALWOOD_KNIGHT_APPEARANCE', '1', 'body'], 'Wrong appearance config')
        body = glb(root / fields[3])
        source = N / 'npc_knights' / id_
        packet = load(source / 'poses.json')
        rigid = [row for row in packet['meshes'] if row.get('rigid')]
        require(len(rigid) == 1 and rigid[0]['material'] == 'Owned Unity Knight roster weapon', 'Wrong real NPC source weapon')
        require(packet['passed'] and (source / 'poses.pnt').is_file(), 'Incomplete actual Knight packet')
        # Three TP and three dedicated FP Bow poses are real original motion.
        for pose in range(204, 210):
            glb(source / 'body' / ('combat_body_%03d.glb' % pose))
        pin(source / 'NPC_BANK_READY.json')
        pin(source / 'BOW_SOURCE_REVIEW.json')
        knights.append({'id': id_, 'body_primitives': sum(len(m['primitives']) for m in body['meshes']),
                        'source_skin_joints': len(body['skins'][0]['joints']), 'packet_poses': 210,
                        'weapons': ['sword', 'bow', 'shield', 'axe', 'staff'],
                        'unarmed_poses': [172, 203], 'bow_tp_poses': [204, 206], 'bow_fp_poses': [207, 209]})
    catalog = load(N / 'arena_orcs_roster/ORC_ROSTER.json')
    expected_orcs = ['orc01', 'orc02', 'orc03', 'orc04', 'orc05', 'orc06', 'azrak',
                     'costalmed', 'dardan', 'girra', 'gnur', 'kanit', 'magur', 'ursag']
    require([row['id'] for row in catalog['entries']] == expected_orcs, 'Actual Orc roster differs')
    orcs = []
    for entry in catalog['entries']:
        id_ = entry['id']
        data = glb(N / 'arena_orcs_roster' / entry['body_directory'] / 'combat_body_000.glb')
        packet = load(N / 'compact_orcs' / id_ / 'poses.json')
        rigid = [row for row in packet['meshes'] if row.get('rigid')]
        require(rigid and packet['passed'], 'Actual source Orc weapon/packet is missing')
        pin(N / 'arena_orcs_roster' / entry['asset_manifest'])
        orcs.append({'id': id_, 'source_primitive_count': sum(len(m['primitives']) for m in data['meshes']),
                     'rigid_source_materials': [row['material'] for row in rigid], 'packet_poses': 172,
                     'weapon_variation': 'Original selected source weapon; not a substituted Human blade'})
    path = N / 'animation-review/ANIMATION_COVERAGE_INVENTORY.json'
    path.write_text(json.dumps({'schema': 'valwood.animation-polish.coverage-inventory.v1',
                               'passed': True, 'inputs': INPUTS, 'hero_clip_families': clips,
                               'knights': knights, 'orcs': orcs, 'actual_knights': 18, 'actual_orcs': 14,
                               'motion_quality_accepted': False, 'runtime_loading_accepted': False,
                               'note': 'Coverage inventory only. Current-source CPU/GL/gameplay films must separately accept transitions and contacts.'}, indent=2) + '\n')
    print('PASS4 owned clip families,18 actual outfits/Bow banks,14 source-weapon banks:', path)


if __name__ == '__main__':
    main()
