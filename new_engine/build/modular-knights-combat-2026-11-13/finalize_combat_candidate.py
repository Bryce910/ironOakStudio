#!/usr/bin/env python3
"""Expose the private Linux combat candidate only with matching current proofs."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
SOURCE = BASE / 'source-v1'
OLD = ROOT / 'build/modular-knights-2026-11-13'
BANK = BASE / 'enemy-guards-v6/bank/models'
ASSET = ROOT / 'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
ASSET_SHA = '80687a8d3710c5e82feaa368b31b9f8b82721927b0bf874d7c8f191e0bc5077d'
PINS = {}

def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)

def pin(path, expected=None):
    path = Path(path).absolute()
    with path.open('rb') as stream:
        actual = hashlib.file_digest(stream, 'sha256').hexdigest()
    require(expected is None or expected == actual, 'Changed proof input: ' + str(path))
    PINS[str(path)] = actual
    return actual

def hashes(values, root=None):
    require(isinstance(values, dict) and bool(values), 'Missing proof dependency map')
    for name, expected in values.items():
        path = Path(name)
        if not path.is_absolute():
            require(root is not None, 'Missing dependency root')
            path = root / path
        pin(path, expected)

def proof(name, key=None):
    path = BASE / name
    result = json.loads(path.read_text())
    if key:
        require(result.get(key) is True, 'Unaccepted proof: ' + str(path))
    pin(path)
    return result

def test_run(name, executable):
    run = proof(name)
    require(run['source_unchanged'] and run['returncode'] == 0 and
            run['before_sha256'] == run['after_sha256'], 'Inputs changed during CPU test')
    hashes(run['before_sha256'])
    pin(BASE / executable, run['test_executable_sha256'])
    pin(run['report'], run['report_sha256'])

def main():
    native = proof('runtime-native-v1/READY.json', 'ready')
    require(native['native_only'] and native['application_units'] == 102 and
            Path(native['binary']).resolve() == (BASE / 'runtime-native-v1/native-bin/rpg').resolve() and
            Path(native['source']).resolve() == SOURCE.resolve(), 'Wrong native runtime closure')
    hashes(native['source_sha256'], SOURCE)
    pin(native['binary'], native['sha256'])
    pin(ASSET, ASSET_SHA)
    require((BASE / 'arena-project/models').resolve() == BANK.resolve(), 'Wrong private model bank')
    guard_ready = proof('enemy-guards-v6/READY.json', 'ready')
    require(Path(guard_ready['bank_models']).resolve() == BANK.resolve(), 'Guard proof used a different bank')
    hashes(guard_ready['dependencies_sha256'])
    owned = BASE / 'enemy-guards-v6/OWNED_DEPENDENCIES.json'
    hashes(json.loads(owned.read_text())['sha256'], BANK)
    pin(owned)
    ancestor_owned = OLD / 'enemy-guards-v5/OWNED_DEPENDENCIES.json'
    pin(ancestor_owned, '8f45b5fce0b0c7755092eac4ea7812695e42276782dda8fec47753bb5d0e676c')
    hashes(json.loads(ancestor_owned.read_text())['sha256'], OLD / 'enemy-guards-v5/bank/models')
    pin(OLD / 'enemy-guards-v5/READY.json', '097a9fa7ce6061930b7eba045a228f2d267fc2666d0dd5574338390b9dedc079')

    animation = proof('hero-animation-polish-check.json', 'accepted')
    require(animation['grip_transition_samples'] >= 37453 and animation['support_samples'] >= 6300 and
            animation['flat_boot_penetration_m'] < .001 and animation['paused_pose_delta'] == 0,
            'Animation regression coverage incomplete')
    test_run('ANIMATION_TEST_INPUTS.json', 'test_skeletal_hero')
    seams = proof('COMBAT_SEAMS.json', 'accepted')
    require(seams['fresh_serial_queries'] >= 225 and seams['reaction_samples'] >= 10650 and
            seams['pre_render_combo_edge_error_m'] < .00001 and seams['paused_pose_error'] == 0 and
            seams['physical_timing'] == 'unchanged', 'Combat seam coverage incomplete')
    test_run('COMBAT_SEAMS_INPUTS.json', 'test_combat_seams')
    contact = proof('CONTACT_INTEGRATION.json', 'passed')
    hashes(contact['source_sha256'], SOURCE)
    hashes(contact['proofs_sha256'])
    require(contact['hero_asset']['sha256'] == ASSET_SHA, 'Contact tested a different asset')
    for name in ('FIRST_PERSON_REVIEW.json', 'independent-review/COMBAT_REVIEW.json', 'combat-review-v1/VISUAL_REVIEW.json', 'enemy-guards-v6/ISOLATION_REVIEW.json', 'enemy-guards-v6/INDEPENDENT_GUARD_VISUAL_REVIEW.json'):
        report = proof(name, 'accepted')
        hashes(report['dependencies_sha256'])
    for name in ('host-foot-support-check.json', 'character-presentation-check.json'):
        report = proof(name, 'accepted')
        hashes(report['dependencies_sha256'])

    preview = proof('combat-review-v1/PREVIEW_RECEIPT.json')
    hashes(preview['sha256'])
    require(len(preview['scenarios']) == 12 and sum(s['frames'] for s in preview['scenarios'].values()) == 5340,
            'Incomplete combat preview coverage')
    for item in preview['scenarios'].values():
        require(item['gpu_failures'] == 0, 'Combat preview GPU failures')
        pin(item['video'], item['video_sha256'])
    smoke = proof('combat-review-v1/WORLD_SMOKE_ACCEPTED.json', 'accepted')
    require(smoke['binary_sha256'] == native['sha256'] and smoke['asset_sha256'] == ASSET_SHA,
            'Actual game smoke used superseded inputs')
    require(Path(smoke['model_bank']).resolve() == BANK.resolve(), 'Actual game smoke used a different model bank')
    pin(smoke['log'], smoke['log_sha256'])
    pin(smoke['capture'], smoke['capture_sha256'])
    cost = proof('CPU_PERFORMANCE_REVIEW.json', 'accepted')
    hashes(cost['sha256'])
    for path in (BASE / 'launch_knight_candidate.sh', ROOT.parent / 'Launch_Valwood_Knight_Combat_Test.sh', Path(__file__)):
        pin(path)
    result = {
        'schema': 'valwood.knight-combat-private-test-ready.v1', 'accepted': True,
        'accepted_at_utc': datetime.now(timezone.utc).isoformat(),
        'scope': 'Private Linux Modular Knight longsword combat evaluation; first-person guard-entry correction.',
        'binary': native['binary'], 'binary_sha256': native['sha256'],
        'asset': str(ASSET), 'asset_sha256': ASSET_SHA,
        'project_models': str(BASE / 'arena-project/models'), 'verified_model_bank': str(BANK),
        'dependencies_sha256': PINS,
        'checks': {'animation': animation, 'combat_seams': seams, 'contact': contact['checks'],
                   'preview_scenarios': 12, 'preview_frames': 5340, 'native_smoke': smoke},
        'limits': [
            'Scripted CPU and offscreen software OpenGL verification; interactive controller feel and hardware frame pacing need player review.',
            'Third-person sword/shield, axe, staff and other weapon styles retain their existing animation paths.',
            'Existing capture balance, stationary turning and occasional armor intersections still need art polish.',
            'Canonical game/editor and previous accepted knight test launchers are unchanged.',
        ],
        'windows_work_performed': False,
    }
    output = BASE / 'COMBAT_POLISH_READY.json'
    temporary = output.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(output)
    print(json.dumps({'accepted': True, 'dependencies': len(PINS), 'receipt': str(output)}))

if __name__ == '__main__':
    main()
