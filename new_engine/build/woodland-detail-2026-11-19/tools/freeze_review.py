"""Freeze the actual reviewed Linux scene/shader inputs before guarded promotion."""
from pathlib import Path
import argparse, json, sys
sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
B = N.with_name('campsite-look-2026-11-19')
sys.path.insert(0, str(N))
from check_unified import (sha, need, verify_native_build, model_overlay, EXPECTED_SOURCE_CHANGES,
                           COLLISION_CATALOG, COLLISION_REVIEW, COLLISION_PREIMAGE,
                           collision_binding, allowed_scene_path, owned_project_leaf,
                           inherited_catalog_transition)


def state_file(p):
    relative = p.relative_to(N)
    return any(part in ('saves', 'settings', '__pycache__') for part in relative.parts) or (
        p.name in ('graphics.settings', 'studio-layout.ini', 'world.studio.workspace', 'player-profile.config')
        or p.name.startswith('arena_') and p.suffix == '.cfg'
        or '.recovery' in p.name or p.suffix in ('.log', '.lock', '.pyc', '.pyo', '.progress'))



INPUT_GROUPS = ('scene_input_sha256', 'shader_input_sha256', 'inputs_sha256',
                'input_sha256', 'private_input_sha256')


def native_scene_origin(result,generation):
    """Resolve only scene roots explicitly used by the known native harness."""
    if result.get('schema')=='valwood.forest-matched-views.execution.v1':
        relative='world-project'
    else:
        test=result.get('test')
        if test in ('arena','arena-orcs','equipment'):
            relative='arena-project'
        elif test=='walking-first':
            need(result.get('compact_scene_for_first_person_controls') is True,
                 'First-person native fixture must declare its compact arena scene')
            relative='arena-project'
        elif test=='joust':
            relative='joust-project'
        elif test in ('area-city','area-grandstone','area-showcase','area-atlas'):
            relative='area-projects/'+test[5:]
        else:
            need(test in ('apartment','walking-third','opening','skip','journey','story-swim',
                          'npc-intro','npc-intro-natural','editor','scenery','road'),
                 'Unrecognized native scene origin')
            relative='world-project'
    origin=generation/relative
    need(origin.is_dir() and not origin.is_symlink(),'Native scene origin must be an owned known project')
    return origin


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
        origin=native_scene_origin(result,generation)
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
        origin=native_scene_origin(result,generation)
        need(prep.get('scene_input_sha256'), 'Native preparation has no scene pins')
        scene=prep['scene_input_sha256']
        need(all(Path(name).is_relative_to(origin) and '..'not in Path(name).parts for name in scene),
             'Native scene pins use an unrelated origin')
        world_input=str(origin/'world.studio')
        need(scene.get(world_input)==sha(Path(world_input)),
             'Native preparation did not pin its actual known scene world')
        harness = Path(prep['harness_source_path'])
        need(harness.is_file() and sha(harness) == prep['harness_source_sha256'],
             'Native harness snapshot changed')
        pins[str(harness)] = sha(harness)
    need(result.get('binary_sha256') == sha(generation/'runtime/rpg'), 'Native binary changed')
    if generation == N:
        catalog=str(origin/COLLISION_CATALOG)
        need(any(prep.get(group,{}).get(catalog)==sha(Path(catalog)) for group in INPUT_GROUPS),
             'Native preparation did not pin final collision catalog')
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
    return generation == N and origin == N/'world-project' and any(
        prep.get(group, {}).get(world) == sha(Path(world)) for group in INPUT_GROUPS)


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
    binary, actual_source, bank = verify_native_build()
    overlays = model_overlay()
    collision_binding()
    shader_changes = sorted(p for p in bank if sha(B/'shared/shaders'/p) != bank[p])
    changes = []
    scene_names = ['world.studio', 'city.scene', 'city.sky', COLLISION_CATALOG] + [str(p.relative_to(B/'world-project'))
                     for p in sorted((B/'world-project/scenery').glob('*.txt'))]
    for name in scene_names:
        before = N/COLLISION_PREIMAGE if name == COLLISION_CATALOG else B/'world-project'/name
        after = owned_project_leaf(N/'world-project',name)
        if sha(before) != sha(after):
            changes.append(dict(path=name, before_sha256=sha(before), after_sha256=sha(after)))
    need(changes and all(allowed_scene_path(row['path']) for row in changes),
         'Only scoped woodland authoring and collision binding metadata may change')
    pins, aliases = {}, {}
    roots = ['source-v1', 'shared/shaders', 'world-project', 'arena-project', 'joust-project',
             'area-projects', 'assets', 'motion', 'tools', 'runtime', 'runtime-native-v1']
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
    for name in ('launch.py', 'run_native_review.py', 'check_unified.py', 'BASELINE.json', 'GUIDE.txt',
                 'SOURCE_ORIGIN.json', 'build_native_candidate.py', 'runtime/ORIGIN_BUILD.json',
                 'review/SOURCE_CHANGE_SCOPE.json', 'review/SHADER_CHANGE_SCOPE.json',
                 'review/foliage-optimization/LEAF_OVERLAY.json', COLLISION_PREIMAGE):
        pins[str(N/name)] = sha(N/name)
    receipt_pins = {}
    pin_receipt(N/COLLISION_REVIEW,pins)
    receipt_pins[str(N/COLLISION_REVIEW)]=sha(N/COLLISION_REVIEW)
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
    # Compiler system headers and the pinned physics library are real link
    # inputs outside the candidate tree; retain their reviewed digests too.
    import os
    from check_unified import read
    plan = read(N/'runtime-native-v1/BUILD_PLAN.json')
    for row in plan['units']:
        receipt = read(Path(row['target']).with_suffix('.json'))
        for name, digest in receipt['actual_dependency_sha256'].items():
            need(sha(name) == digest, 'Compiled dependency changed while freezing')
            pins[name] = digest
    physics = N.parents[1]/'build/physics/libJolt.a'
    pins[str(physics)] = sha(physics)
    pins[str(B/'UNIFIED_READY.json')] = sha(B/'UNIFIED_READY.json')
    live = N.parents[1]/'projects/valwood_current'
    asset_changes = []
    for overlay in overlays:
        relative = 'models/'+overlay['relative_path']; target = live/relative
        need(target.is_symlink() and target.is_file() and
             sha(target) == overlay['source_glb_sha256'], 'Live model changed since staging')
        need(all(not p.is_symlink() for p in target.parents if p != live and p.is_relative_to(live)),
             'Live model parent unexpectedly aliased')
        asset_changes.append(dict(path=relative, before_sha256=overlay['source_glb_sha256'],
            after_sha256=overlay['candidate_glb_sha256'], before_link=os.readlink(target),
            before_resolved=str(target.resolve()), target=str(N/'world-project'/relative)))
    manifest = dict(schema='valwood.woodland-detail.changeset.v1', reviewed=True,
                    runtime_reused=False, Windows_work_performed=False,
                    binary_sha256=binary, production_source_files_verified=len(actual_source),
                    source_changes=sorted(EXPECTED_SOURCE_CHANGES),
                    build_ready_sha256=sha(N/'runtime-native-v1/READY.json'),
                    scene_changes=changes, asset_changes=asset_changes,
                    collision_binding_review_sha256=sha(N/COLLISION_REVIEW),
                    shared_live_catalog_transition=inherited_catalog_transition(),
                    shader_changes=shader_changes, shader_sha256=bank,
                    pins=pins, aliases=aliases, evidence=receipt_pins)
    (N/'review/CHANGESET.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('Frozen', len(changes), 'scene documents,', len(asset_changes), 'model aliases,',
          len(pins), 'inputs and', len(aliases), 'aliases.')


if __name__ == '__main__':
    main()
