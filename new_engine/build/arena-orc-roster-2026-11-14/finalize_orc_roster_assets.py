#!/usr/bin/env python3
"""Close each genuine owned-character producer bank, then emit the full roster.

This is a producer receipt, not native, combat, visual, or hardware acceptance.
The previously accepted Gnur bank is copied byte-for-byte and never rewritten.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import math
import struct

STAGE = Path(__file__).resolve().parent
BANKS = STAGE / 'arena_orcs_roster'
INVENTORY = STAGE / 'intake_roster/ORC_SOURCE_ROSTER.json'
IDS = {'orc01', 'orc02', 'orc03', 'orc04', 'orc05', 'orc06', 'azrak',
       'costalmed', 'dardan', 'girra', 'gnur', 'kanit', 'magur', 'ursag'}


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def checked(values):
    result = {}
    require(isinstance(values, dict) and values, 'Missing producer dependency closure')
    for name, expected in values.items():
        path = Path(name).resolve()
        require(path.is_file() and sha(path) == expected, 'Changed producer dependency: ' + str(path))
        result[str(path)] = expected
    return result


def proof(path, sources, generators):
    path = Path(path).resolve()
    data = read(path)
    require(data.get('passed') is True, 'Producer proof has not passed: ' + str(path))
    sources[str(path)] = sha(path)
    for key in ('inputs', 'source_files', 'dependencies_sha256', 'outputs_sha256'):
        if data.get(key):
            sources.update(checked(data[key]))
    if data.get('generator'):
        generators.update(checked(data['generator']))
    return data, {'path': str(path), 'sha256': sha(path)}


def inventory():
    data = read(INVENTORY)
    require(data.get('passed') and data.get('character_count') == 14 and
            data.get('weapon_mesh_count') == 16 and
            {row['id'] for row in data['characters']} == IDS, 'Incomplete owned source inventory')
    return data


def finalize(identity):
    require(identity in IDS - {'gnur'}, 'Gnur is immutable; only finalize new owned characters')
    original = next(row for row in inventory()['characters'] if row['id'] == identity)
    bank = BANKS / identity
    body = bank / 'body'
    anatomy = bank / 'anatomy'
    sources = {str(INVENTORY): sha(INVENTORY)}
    generators = {str(Path(__file__).resolve()): sha(__file__)}
    poses = [body / f'combat_body_{i:03}.glb' for i in range(172)]
    caps = [anatomy / 'caps' / f'cap_{i:03}.glb' for i in range(172)]
    require(all(path.is_file() for path in poses + caps), 'Complete body and cap bank required: ' + identity)
    require(len(list(body.glob('combat_body_*.glb'))) == 172 and
            len(list((anatomy / 'caps').glob('cap_*.glb'))) == 172, 'Unexpected pose files')
    motion = read(body / 'combat_motion_manifest.json')
    require(motion.get('character_id') == identity and motion['pose_count'] == 172 and
            motion.get('pose_origin_policy') == 'preserve' and
            motion.get('authored_locomotion') == 'Run' and not motion.get('provisional_materials'),
            'Character motion producer is incomplete')
    sources.update(checked(motion['source_files']))
    generators.update(checked(motion['generator']))
    require(sources.get(str(Path(original['body']['path']).resolve())) == original['body']['sha256'],
            'Cooked character is not its original owned body')
    for row in [original['body_prefab'], *original['weapons']]:
        require(sha(row['path']) == row['sha256'], 'Owned character source changed')
        sources[str(Path(row['path']).resolve())] = row['sha256']

    mapping, mapping_ref = proof(bank / 'ORC_MATERIAL_MAPPING.json', sources, generators)
    require(mapping['source_character_id'] == identity and mapping['original_textures_present'] and
            not mapping['materials_provisional'], 'Original material provenance is missing')
    sources.update(checked(mapping['converted_textures']))
    texture, texture_ref = proof(bank / 'ORC_TEXTURE_MAPPING.json', sources, generators)
    require(texture['source_character_id'] == identity and texture['geometry_edited'] is False,
            'Texture conversion changed character geometry')
    material, material_ref = proof(bank / 'SOURCE_MATERIAL_CORRESPONDENCE.json', sources, generators)
    require(material['source_character_id'] == identity and
            material['source_prefab_guid'] == original['source_prefab_guid'],
            'Wrong source prefab/material correspondence')
    normal, normal_ref = proof(bank / 'NORMAL_BASIS_SOURCE.json', sources, generators)
    require(normal['source_character_id'] == identity and not normal['vertical_bitmap_flip'],
            'Unverified source tangent/normal convention')
    alpha, alpha_ref = proof(bank / 'ALPHA_IMPORT_REVIEW.json', sources, generators)
    require(alpha['source_character_id'] == identity, 'Wrong source alpha import review')
    uv, uv_ref = proof(STAGE / 'intake_roster' / identity / 'SOURCE_TO_COOKED_UV.json', sources, generators)
    retarget, retarget_ref = proof(STAGE / 'intake_roster' / identity / 'RETARGET_AND_LAYOUT_REVIEW.json', sources, generators)
    meshes = len(motion['primitive_names'])
    require(uv['primitive_count'] == meshes and retarget['character_id'] == identity and
            retarget['source_mesh_primitives'] == meshes and retarget['body_poses'] == 172 and
            retarget['actual_target_bones'] == motion['source_actual_bones'] and
            retarget['immutable_uv_index_tangent_sign_material_layout'] and
            retarget['actual_source_weights_normalized_without_ratio_changes'] and
            retarget['explicit_virtual_held_frames'] == ['Weapon1'],
            'Actual character retarget/layout contract incomplete')
    curve, curve_ref = proof(bank / 'MOTION_CURVE_REVIEW.json', sources, generators)
    require(curve['character_id'] == identity and curve['continuous_source_sampling'] and
            curve['idle_source_fraction'] == 1 and curve['idle_both_endpoints_shared_neutral'] and
            curve['authored_locomotion'] == 'Run' and curve['locomotion_source_phase_offset'] == .125 and
            curve['pose_origin_policy'] == 'preserve', 'Curated motion contract incomplete')
    walk, walk_ref = proof(bank / 'WALK_SOURCE_MEASUREMENT.json', sources, generators)
    require(walk['authored_locomotion'] == 'Run' and walk['source_phase_offset'] == .125 and
            math.isfinite(walk['walk_stride_m']) and .25 <= walk['walk_stride_m'] <= 4,
            'Physical source Run stride is invalid')
    for key in ('source', 'root_motion_source'):
        row = walk[key]
        sources.update(checked({row['source']: row['source_sha256']}))
    guard_path = bank / 'GUARD_FIT_MEASUREMENTS.json'
    guard = read(guard_path)
    require(set(guard) == {'left', 'right', 'high', 'low', 'thrust'} and
            all(row['fixed_limb_lengths'] and row['source_finger_grip_unchanged'] and
                0 <= row['edge_center_error_metres'] < .003 and
                0 <= row['unreachable_distance_source_metres'] < .003 for row in guard.values()),
            'Actual five-direction guard fit is incomplete')
    sources[str(guard_path)] = sha(guard_path)
    guard_ref = {'path': str(guard_path), 'sha256': sha(guard_path)}
    anatomy_proof, anatomy_ref = proof(anatomy / 'ANATOMY_READY.json', sources, generators)
    require(anatomy_proof['ready'] and anatomy_proof['poses'] == 172 and
            anatomy_proof['source_character_id'] == identity and
            anatomy_proof['pose_origin_policy'] == 'preserve', 'Character anatomy not coherently finalized')
    header = (anatomy / 'anatomy.bin').read_bytes()[:20]
    require(header[:8] == b'VWANA002' and struct.unpack('<III', header[8:]) == (172, meshes, 8),
            'Actual character anatomy header mismatch')
    cfg = (body / 'combat_presentation.cfg').read_text().split()
    profile = anatomy_proof['collision_profile']
    require(cfg[:2] == ['VALWOOD_COMBAT_PRESENTATION', '1'] and cfg[-1] == 'end',
            'Presentation config is not native format')
    for field in ('body_radius_m', 'body_height_m'):
        require(cfg.count(field) == 1 and abs(float(cfg[cfg.index(field) + 1]) - profile[field]) < 1e-6,
                'Visible body and measured collision profile differ')
    require(abs(float(cfg[cfg.index('walk_stride_m') + 1]) - walk['walk_stride_m']) < 1e-8,
            'Runtime stride differs from actual source Run measurement')
    require((bank / 'ORC_MATERIALS.cfg').read_text().split() ==
            ['VALWOOD_ORC_MATERIALS', '1', 'original_textures', '1', 'end'],
            'Original texture production is not explicitly complete')
    rigid = motion['weapon_primitives']
    require(rigid and all(mapping['gltf']['materials'][mapping['primitive_materials'][index]]['name'] ==
                         'Owned Unity Orc roster weapon' for index in rigid),
            'Real held weapon lost its exact production marker')
    # Preserve the known immutable original archives and their actual identities.
    archives = checked(read(BANKS / 'gnur/ORC_ASSETS.json')['source_archives'])
    destination = bank / 'ORC_ASSETS.json'
    outputs = {str(path.resolve()): sha(path) for path in sorted(bank.rglob('*'))
               if path.is_file() and path != destination and not path.name.endswith('.tmp')}
    result = dict(
        schema='valwood.owned-orc.arena-assets.v2',
        created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_name=original['display_name'], source_model=original['display_name'],
        source_character_id=identity, source_prefab_guid=original['source_prefab_guid'],
        source_collection='Characters Orcs (Pack)', source_archives=archives,
        source_files=sources, generator=generators, outputs=outputs,
        body_directory='body', anatomy_directory='anatomy', body_pose_count=172, cap_pose_count=172,
        rigid_weapon_primitives=rigid, rigid_weapon_material_name='Owned Unity Orc roster weapon',
        source_bones=motion['source_actual_bones'], explicit_virtual_weapon_frames=1,
        actual_geometry_sha256={str(path.relative_to(bank)): sha(path) for path in poses + caps},
        actual_source_to_cooked_uv=uv_ref, source_to_cooked_uv_receipt=uv_ref,
        retarget_and_layout_receipt=retarget_ref, original_material_receipt=material_ref,
        material_mapping_receipt=mapping_ref, original_texture_receipt=texture_ref,
        normal_basis_receipt=normal_ref, original_alpha_import_receipt=alpha_ref,
        anatomy_receipt=anatomy_ref, motion_curve_receipt=curve_ref,
        locomotion_source_receipt=walk_ref, guard_fit_receipt=guard_ref,
        measured_collision_profile=profile, materials_provisional=False, original_textures_present=True,
        producer_complete=True, visual_ready=False, accepted=False,
        authored_locomotion='Run', source_locomotion_phase_offset=.125, pose_origin_policy='preserve',
        limitations=['Producer completion requires separate native, contact, resource and visual review.'],
        source_rights_note='User-provided owned commercial sources; retain the purchased license.')
    destination.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps(dict(producer_complete=True, character_id=identity,
                          manifest=str(destination), sha256=sha(destination))), flush=True)


def emit_roster():
    entries = []
    for row in inventory()['characters']:
        identity = row['id']
        path = BANKS / identity / 'ORC_ASSETS.json'
        child = read(path)
        require(child.get('producer_complete') and child.get('original_textures_present') and
                not child.get('materials_provisional') and child['body_pose_count'] == 172 and
                child['cap_pose_count'] == 172, 'Character not finalized: ' + identity)
        require(identity == 'gnur' or (child.get('source_character_id') == identity and
                child.get('source_prefab_guid') == row['source_prefab_guid']), 'Character source identity mismatch')
        entries.append(dict(id=identity, display_name=row['display_name'],
                            body_directory=identity + '/body', anatomy_directory=identity + '/anatomy',
                            materials_config=identity + '/ORC_MATERIALS.cfg',
                            asset_manifest=identity + '/ORC_ASSETS.json', asset_manifest_sha256=sha(path),
                            source_prefab_guid=row['source_prefab_guid']))
    require(len(entries) == 14 and {row['id'] for row in entries} == IDS, 'Missing roster character')
    doc = dict(schema='valwood.owned-orc.roster.v1', default_id='gnur', entries=entries)
    (BANKS / 'ORC_ROSTER.json').write_text(json.dumps(doc, indent=2) + '\n')
    lines = ['VALWOOD_ORC_ROSTER 1', 'default gnur']
    for row in entries:
        lines.append('character ' + row['id'] + ' ' + ' '.join(json.dumps(row[key]) for key in
                     ('display_name', 'body_directory', 'anatomy_directory', 'materials_config')))
    (BANKS / 'ORC_ROSTER.cfg').write_text('\n'.join(lines + ['end', '']))
    print('Produced full14character Orc roster; independent acceptance is still required.', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', choices=sorted(IDS - {'gnur'}))
    parser.add_argument('--roster', action='store_true')
    args = parser.parse_args()
    require(args.character or args.roster, 'Choose --character or --roster')
    if args.character:
        finalize(args.character)
    if args.roster:
        emit_roster()


if __name__ == '__main__':
    main()
