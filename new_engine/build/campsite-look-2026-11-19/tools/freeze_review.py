"""Freeze the actual reviewed Linux scene/shader inputs before guarded promotion."""
from pathlib import Path
import argparse, json, sys
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
B = N.with_name('river-swim-2026-11-19')
sys.path.insert(0, str(N))
from check_unified import sha, need


def state_file(p):
    relative = p.relative_to(N)
    return any(part in ('saves', 'settings', '__pycache__') for part in relative.parts) or (
        p.name in ('graphics.settings', 'studio-layout.ini', 'world.studio.workspace', 'player-profile.config')
        or p.name.startswith('arena_') and p.suffix == '.cfg'
        or '.recovery' in p.name or p.suffix in ('.log', '.lock', '.pyc', '.pyo', '.progress'))



INPUT_GROUPS = ('scene_input_sha256', 'shader_input_sha256', 'inputs_sha256',
                'input_sha256', 'private_input_sha256')


def pin_inputs(document, pins):
    """Verify every recorded input; retain stable resolved resources."""
    for group in INPUT_GROUPS:
        values = document.get(group, {})
        need(isinstance(values, dict), 'Invalid receipt input group: '+group)
        for name, h in values.items():
            p = Path(name)
            need(p.is_file() and sha(p) == h, 'Receipt inputs changed: '+name)
            resolved = p.resolve()
            # Temporary private scene copies are historical test fixtures;
            # stable shaders reached through a /tmp alias must still be pinned.
            if not resolved.is_relative_to('/tmp'):
                pins[str(resolved)] = h


def preparation_inputs(result, receipt, pins):
    """Bind native execution to its actual preparation and complete shader bank.

    Static forest receipts carry a PREPARATION digest. The older ordinary-input
    harness repeats its complete command/environment/input maps instead; require
    those exact duplicates before accepting its sibling preparation.
    """
    path = receipt.with_name('PREPARATION.json')
    need(path.is_file() and not path.is_symlink() and
         path.resolve().is_relative_to(N/'review'), 'Native preparation missing/escaped')
    prep = json.loads(path.read_text())
    if result.get('schema') == 'valwood.forest-matched-views.execution.v1':
        need(result.get('preparation_sha256') == sha(path), 'Execution preparation changed')
        need(prep.get('schema') == 'valwood.forest-matched-views.preparation.v1',
             'Wrong static preparation schema')
        for name in ('stage', 'view', 'binary_sha256', 'quality_frozen'):
            need(result.get(name) == prep.get(name), 'Native preparation mismatch: '+name)
        stage = prep.get('stage')
        need(stage in ('candidate', 'baseline'), 'Unexpected native preparation stage')
        generation = N if stage == 'candidate' else B
        need(prep.get('generation') == str(generation), 'Wrong prepared generation')
        need(prep.get('input_sha256'), 'Static preparation has no actual input pins')
        world_input = str(generation/'world-project/world.studio')
        need(prep['input_sha256'].get(world_input) == sha(Path(world_input)),
             'Static preparation did not pin the current world')
        private = Path(prep['private_project'])/'world.studio'
        need(prep.get('private_input_sha256', {}).get(str(private)) == sha(private) ==
             sha(generation/'world-project/world.studio'), 'Static private world differs from reviewed generation')
    else:
        need(result.get('test') and result.get('shader_input_sha256'), 'Not a recognized native execution')
        for name in ('test', 'binary_sha256', 'command', 'environment', 'private_project',
                     'harness_source_path', 'harness_source_sha256',
                     'scene_input_sha256', 'shader_input_sha256'):
            need(name in result and result[name] == prep.get(name),
                 'Native preparation mismatch: '+name)
        generation = B if result.get('baseline_comparison') else N
        need(prep.get('scene_input_sha256'), 'Native preparation has no scene pins')
        harness = Path(prep['harness_source_path'])
        need(harness.is_file() and sha(harness) == prep['harness_source_sha256'],
             'Native harness snapshot changed')
        pins[str(harness)] = sha(harness)
    need(result.get('binary_sha256') == sha(generation/'runtime/rpg'), 'Native binary changed')
    command = prep.get('command', [])
    need(command and Path(command[0]).resolve() == (generation/'runtime/rpg').resolve(),
         'Native command used another runtime')
    pin_inputs(prep, pins)
    shader_root = (generation/'shared/shaders').resolve()
    loaded = {}
    for group in INPUT_GROUPS:
        for name, h in prep.get(group, {}).items():
            resolved = Path(name).resolve()
            if resolved.is_relative_to(shader_root):
                relative = str(resolved.relative_to(shader_root))
                need(relative not in loaded or loaded[relative] == h, 'Conflicting shader receipt aliases')
                loaded[relative] = h
    actual = {str(p.relative_to(shader_root)): sha(p) for p in shader_root.rglob('*') if p.is_file()}
    need(loaded == actual, 'Native preparation did not use the complete final shader bank')
    pins[str(path)] = sha(path)
    # Anchor timing/raw-world files through the report digest recorded at exit.
    if 'frame_pacing_report_sha256' in result:
        pacing_path, pacing_hash = receipt.with_name('FRAME_PACING_REVIEW.json'), result['frame_pacing_report_sha256']
    else:
        pacing = result.get('frame_pacing_review', {})
        pacing_path, pacing_hash = Path(pacing.get('path', '')), pacing.get('sha256')
    need(pacing_path.is_file() and pacing_path.resolve().is_relative_to(N/'review') and
         sha(pacing_path) == pacing_hash, 'Native timing report changed/missing')
    pacing = json.loads(pacing_path.read_text())
    pin_inputs({'inputs_sha256': pacing.get('inputs', {})}, pins)
    pins[str(pacing_path)] = pacing_hash
    world = str(N/'world-project/world.studio')
    return generation == N and any(prep.get(group, {}).get(world) == sha(Path(world))
                                  for group in INPUT_GROUPS)


def pin_receipt(path, pins):
    result = json.loads(path.read_text())
    need(result.get('passed') is True, 'Review failed: '+str(path))
    pin_inputs(result, pins)
    candidate_world_native = False
    if path.name == 'EXECUTION.json':
        candidate_world_native = preparation_inputs(result, path, pins)
    pins[str(path)] = sha(path)
    return candidate_world_native


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--evidence', action='append', required=True,
                    help='Owned relative path to a passing final receipt')
    a = ap.parse_args()
    need(not (N/'UNIFIED_READY.json').exists(), 'Published generations are immutable')
    need(sha(N/'runtime/rpg') == sha(B/'runtime/rpg'), 'This pass must reuse the approved executable')
    baseline_cpp = {str(p.relative_to(B/'source-v1')): sha(p) for p in (B/'source-v1').rglob('*')
                    if p.is_file() and p.relative_to(B/'source-v1').parts[0] != 'shaders'
                    and '__pycache__' not in p.parts and p.suffix not in ('.pyc', '.pyo')}
    actual_cpp = {str(p.relative_to(N/'source-v1')): sha(p) for p in (N/'source-v1').rglob('*')
                  if p.is_file() and p.relative_to(N/'source-v1').parts[0] != 'shaders'
                  and '__pycache__' not in p.parts and p.suffix not in ('.pyc', '.pyo')}
    need(actual_cpp == baseline_cpp, 'Unexpected production C++/header/build-input change')
    banks = [{str(p.relative_to(g/'shared/shaders')): sha(p) for p in (g/'shared/shaders').rglob('*')
              if p.is_file()} for g in (B, N)]
    need(set(banks[0]) == set(banks[1]), 'Shader inventory changed')
    shader_changes = sorted(p for p in banks[1] if banks[0][p] != banks[1][p])
    need(shader_changes == ['world_foliage.fs.glsl', 'world_water.fs.glsl'], 'Unexpected shader scope')
    for name in shader_changes:
        need(sha(N/'source-v1/shaders'/name) == banks[1][name], 'Source/runtime shader mismatch')
    changes = []
    scene_names = ['world.studio', 'city.scene', 'city.sky'] + [str(p.relative_to(B/'world-project'))
                     for p in sorted((B/'world-project/scenery').glob('*.txt'))]
    for name in scene_names:
        before, after = B/'world-project'/name, N/'world-project'/name
        need(after.is_file() and not after.is_symlink(), 'Scene not owned')
        if sha(before) != sha(after):
            changes.append(dict(path=name, before_sha256=sha(before), after_sha256=sha(after)))
    need(changes and all(row['path'] == 'world.studio' or row['path'].startswith('scenery/')
                         for row in changes), 'Only scoped woodland authoring may change')
    pins, aliases = {}, {}
    roots = ['source-v1', 'shared/shaders', 'world-project', 'arena-project', 'joust-project',
             'area-projects', 'assets', 'motion', 'tools', 'runtime']
    for name in roots:
        root = N/name
        if root.is_symlink():
            aliases[str(root)] = str(root.resolve())
        for p in sorted(root.rglob('*')):
            if state_file(p):
                continue
            if p.is_symlink():
                aliases[str(p)] = str(p.resolve())
            if p.is_file():
                pins[str(p)] = sha(p)
    for name in ('launch.py', 'run_native_review.py', 'check_unified.py', 'BASELINE.json', 'GUIDE.txt'):
        pins[str(N/name)] = sha(N/name)
    receipt_pins = {}
    candidate_world_native = False
    for relative in a.evidence:
        path = N/relative
        need('..' not in Path(relative).parts and path.resolve().is_relative_to(N/'review')
             and path.is_file() and not path.is_symlink(), 'Invalid receipt')
        candidate_world_native = pin_receipt(path, pins) or candidate_world_native
        receipt_pins[str(path)] = sha(path)
    need(candidate_world_native, 'A final candidate world native receipt is required')
    final = N/'review/FINAL_REVIEW.json'
    need(json.loads(final.read_text())['passed'], 'Final visual review missing')
    pins[str(final)] = sha(final)
    manifest = dict(schema='valwood.campsite-look.changeset.v1', reviewed=True,
                    runtime_reused=True, Windows_work_performed=False,
                    binary_sha256=sha(N/'runtime/rpg'), production_nonshader_files_verified=len(actual_cpp),
                    scene_changes=changes, shader_changes=shader_changes, shader_sha256=banks[1],
                    pins=pins, aliases=aliases, evidence=receipt_pins)
    (N/'review/CHANGESET.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('Frozen', len(changes), 'scene documents,', len(pins), 'inputs and', len(aliases), 'aliases.')


if __name__ == '__main__':
    main()
