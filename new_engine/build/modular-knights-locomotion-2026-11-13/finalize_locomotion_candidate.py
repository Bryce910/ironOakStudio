#!/usr/bin/env python3
"""Publish the private Linux test only after its current proofs agree."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
WORKSPACE = ROOT.parent
SOURCE = BASE / 'source-v1'
OLD = ROOT / 'build/modular-knights-2026-11-13'
BANK = OLD / 'enemy-guards-v5/bank/models'
ASSET = ROOT / 'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
EXPECTED_ASSET = '80687a8d3710c5e82feaa368b31b9f8b82721927b0bf874d7c8f191e0bc5077d'
DEPENDENCIES = {}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def pin(path, expected=None):
    path = Path(path).absolute()
    actual = sha(path)
    require(expected is None or expected == actual, f'Stale input: {path}')
    DEPENDENCIES[str(path)] = actual
    return actual


def proof(name, key=None):
    path = BASE / name
    data = json.loads(path.read_text())
    if key:
        require(data.get(key) is True, f'Unaccepted proof: {path}')
    pin(path)
    return data


def hash_map(data, root=None):
    require(bool(data), 'Missing proof inputs')
    for name, expected in data.items():
        path = Path(name)
        if not path.is_absolute():
            require(root is not None, f'Unrooted proof input: {name}')
            path = root / path
        pin(path, expected)


def proof_inputs(data):
    if isinstance(data.get('sha256'), dict):
        hash_map(data['sha256'])
    else:
        require(bool(data.get('dependencies')), 'Missing independent proof inputs')
        for item in data['dependencies']:
            pin(item['path'], item['sha256'])
    for item in data.get('raw_trace_dependencies', []):
        pin(item['path'], item['sha256'])


def main():
    native = proof('runtime-native-v1/READY.json', 'ready')
    require(native['native_only'] and native['application_units'] == 102,
            'Incomplete native application closure')
    require(Path(native['binary']).resolve() ==
            (BASE / 'runtime-native-v1/native-bin/rpg').resolve() and
            Path(native['source']).resolve() == SOURCE.resolve(),
            'Native proof does not certify the private launcher runtime')
    hash_map(native['source_sha256'], SOURCE)
    pin(native['binary'], native['sha256'])
    pin(ASSET, EXPECTED_ASSET)
    require((BASE / 'arena-project/models').resolve() == BANK.resolve(),
            'Private arena no longer uses the reviewed model bank')
    owned_path = OLD / 'enemy-guards-v5/OWNED_DEPENDENCIES.json'
    owned = json.loads(owned_path.read_text())
    hash_map(owned['sha256'], BANK)
    pin(owned_path, '8f45b5fce0b0c7755092eac4ea7812695e42276782dda8fec47753bb5d0e676c')
    pin(OLD / 'enemy-guards-v5/READY.json',
        '097a9fa7ce6061930b7eba045a228f2d267fc2666d0dd5574338390b9dedc079')

    animation = proof('hero-animation-polish-check.json', 'accepted')
    require(animation['grip_transition_samples'] >= 37453 and
            animation['support_samples'] >= 6300 and
            animation['support_segment_length_error_m'] < .00001 and
            animation['flat_boot_penetration_m'] < .001 and
            animation['settled_support_anchor_error_m'] < .001 and
            animation['paused_pose_delta'] == 0,
            'Animation regression checks incomplete')
    inputs = proof('ANIMATION_TEST_INPUTS.json')
    require(inputs['source_unchanged'] and inputs['returncode'] == 0 and
            inputs['before_sha256'] == inputs['after_sha256'],
            'Animation test inputs changed during execution')
    hash_map(inputs['before_sha256'])
    pin(BASE / 'test_skeletal_hero', inputs['test_executable_sha256'])
    pin(inputs['report'], inputs['report_sha256'])

    contact = proof('CONTACT_INTEGRATION.json', 'passed')
    hash_map(contact['source_sha256'], SOURCE)
    hash_map(contact['proofs_sha256'])
    require(contact['hero_asset']['sha256'] == EXPECTED_ASSET,
            'Contact proof used a different hero asset')
    ground = proof('host-foot-support-check.json', 'accepted')
    hash_map(ground['dependencies_sha256'])
    camera = proof('character-presentation-check.json', 'accepted')
    require(camera['checks'] >= 1515, 'Camera checks incomplete')
    hash_map(camera['dependencies_sha256'])

    support = proof('independent-review/FINAL_SUPPORT_REVIEW.json', 'passed')
    proof_inputs(support)
    host = proof('independent-review/HOST_REVIEW.json', 'passed')
    for key in ('host_source', 'helper_source', 'targeted_check', 'test_source'):
        pin(host[key]['path'], host[key]['sha256'])

    preview = proof('animation-review-v4/PREVIEW_RECEIPT.json')
    hash_map(preview['sha256'])
    require(len(preview['scenarios']) == 16, 'Missing animation preview scenarios')
    for scenario in preview['scenarios'].values():
        require(scenario['gpu_failures'] == 0, 'Preview GPU failures')
        pin(scenario['video'], scenario['video_sha256'])
    lateral = proof('lateral-reversal-review/SUPPLEMENTAL_PREVIEW_RECEIPT.json', 'passed')
    proof_inputs(lateral)
    require(lateral['source_frozen_before_after'] and
            lateral['asset_sha256'] == EXPECTED_ASSET,
            'Supplemental reversal inputs are not verified')
    pin(lateral['video'], lateral['video_sha256'])
    visual = proof('animation-review-v4/INDEPENDENT_REVIEW.json', 'accepted')
    proof_inputs(visual)
    smoke = proof('animation-review-v4/WORLD_SMOKE_ACCEPTED.json', 'accepted')
    require(smoke['binary_sha256'] == native['sha256'] and
            smoke['asset_sha256'] == EXPECTED_ASSET,
            'Native smoke tested a superseded candidate')
    pin(smoke['log'], smoke['log_sha256'])
    pin(smoke['capture'], smoke['capture_sha256'])
    performance = proof('CPU_PERFORMANCE_REVIEW.json', 'accepted')
    hash_map(performance['sha256'])

    asset_dir = ASSET.parent
    pin(asset_dir / 'lowerbody_clip_contract.json')
    pin(asset_dir / 'FROZEN_CAPTURE_RECEIPT.json')
    for name in ('absolute_floor_validation.json', 'production_import_report.json'):
        proof('capture-v4_1/' + name, 'passed')
    for path in (BASE / 'launch_knight_candidate.sh',
                 WORKSPACE / 'Launch_Valwood_Knight_Locomotion_Test.sh',
                 Path(__file__)):
        pin(path)

    result = {
        'schema': 'valwood.knight-locomotion-private-test-ready.v1',
        'accepted': True,
        'accepted_at_utc': datetime.now(timezone.utc).isoformat(),
        'scope': 'Private Linux Modular Knight third-person longsword evaluation.',
        'binary': native['binary'], 'binary_sha256': native['sha256'],
        'asset': str(ASSET), 'asset_sha256': EXPECTED_ASSET,
        'project_models': str(BASE / 'arena-project/models'),
        'verified_model_bank': str(BANK),
        'dependencies_sha256': DEPENDENCIES,
        'checks': {
            'animation': animation, 'contact': contact['checks'],
            'ground_checks': ground['checks'], 'camera_checks': camera['checks'],
            'preview_scenarios': len(preview['scenarios']) + 1,
            'preview_frames': sum(s['frames'] for s in preview['scenarios'].values()) + 270,
            'native_smoke': smoke,
        },
        'limits': [
            'Scripted CPU and offscreen OpenGL checks; interactive controls and hardware frame pacing still need player review.',
            'Captured turn-in-place clips are available but not integrated; stationary turning releases support anchors.',
            'Side/back movement and capture balance still need visual polish; full-body stair/balance adjustments and exact whole-pose frame-rate determinism are not claimed.',
            'First person, other weapon styles, sheathed/unarmed, swimming, dodge and death retain their existing animation paths.',
            'Canonical editor/game launchers and the previous accepted knight test are unchanged.',
        ],
        'windows_work_performed': False,
    }
    output = BASE / 'ANIMATION_POLISH_READY.json'
    temporary = output.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(output)
    print(json.dumps({'accepted': True, 'dependencies': len(DEPENDENCIES), 'receipt': str(output)}))


if __name__ == '__main__':
    main()
