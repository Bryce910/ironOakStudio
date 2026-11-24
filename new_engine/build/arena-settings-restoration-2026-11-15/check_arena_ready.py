#!/usr/bin/env python3
"""Check/publish the isolated arena restoration; never freeze player progress."""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import argparse
import importlib.util
import json
import os
import shlex
import subprocess
import tempfile

N = Path(__file__).resolve().parent
ENGINE = N.parents[1]
ROOT = ENGINE.parent
BASE = N.parent / 'demo-final-pass-2026-11-15'
SOURCE_BASE = N.parent / 'road-polish-2026-11-15'
S = N / 'source-v1'
P = N / 'arena-project'
R = N / 'runtime-native-v1'
READY = N / 'ARENA_SETTINGS_READY.json'
BASE_CHECK_SHA = 'c4bb5f1fe1372fb807c2a76acef1da170e53210f07d5ed242d6b2944df9ad849'
BASE_READY_SHA = '205a8585492825cf7f95521e8fc01b61a52b32e60f0f633a6700afbb0b3d449c'
CPU = ('review/gameplay/ARENA_GAMEPLAY_CPU_REVIEW.json',
       'review/render-capacity/RENDER_CAPACITY_CPU_REVIEW.json',
       'review/difficulty/ARENA_DIFFICULTY_CPU_REVIEW.json',
       'review/progress/ARENA_PROGRESS_CPU_REVIEW.json')
NATIVE = N / 'review/native/ARENA_SETTINGS_NATIVE_REVIEW.json'
RAW = N / 'review/native/NATIVE_ARENA_SETTINGS_RAW.json'
PUBLIC = (Path(__file__).resolve(), N / 'launch_arena.sh', N / 'ARENA_NOTES.txt',
          ROOT / 'Launch_Valwood_Arena_Restored.sh')
ALLOWED = set(('world_arena_appearance_assignment.h world_arena_appearance_runtime.inc '
 'world_arena_camera.h world_arena_combat.h world_arena_options.h world_arena_progress.h '
 'world_arena_roster_qa.inc world_arena_settings_qa.inc world_arena_team_ai.inc '
 'world_arena_team_contact.h world_combat_anatomy_actions.inc world_combat_anatomy_runtime.inc '
 'world_combat_arena_runtime.inc world_combat_capacity.h world_combat_compact_draw.inc '
 'world_combat_death_runtime.inc world_combat_difficulty.h world_combat_fatality.h '
 'world_combat_motion.cpp world_combat_motion.h world_combat_vfx.cpp world_combat_vfx.h '
 'world_model_texture_blocks.h world_model_texture_budget.h world_models.cpp world_models.h '
 'world_neck_blood.h world_sever_blood.h world_training_combat.cpp world_training_combat.h '
 'world_viewer.cpp').split())


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def parent_module():
    path = BASE / 'check_demo_final_ready.py'
    # Verify the consumer before importing executable code.
    import hashlib
    require(hashlib.sha256(path.read_bytes()).hexdigest() == BASE_CHECK_SHA,
            'Inherited checker changed')
    spec = importlib.util.spec_from_file_location('accepted_demo_ready', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    require(module.sha(BASE / 'DEMO_FINAL_READY.json') == BASE_READY_SHA,
            'Inherited final-demo acceptance changed')
    return module


B = parent_module()


def private_or_mutable(path):
    """A QA process may legitimately change its own disposable copied saves."""
    path = Path(path).absolute()
    if B.inside(path, P):
        return B.mutable_relative(path.relative_to(P))
    if B.inside(path, BASE / 'arena-project'):
        return B.mutable_relative(path.relative_to(BASE / 'arena-project'))
    if B.inside(path, N / 'review'):
        return any(q.name == 'private-project' or (q / 'project.valwood').is_file()
                   for q in path.parents if B.inside(q, N / 'review'))
    return False


def dep_paths(path):
    # -MP appends phony rules: only the first logical rule is a dependency list.
    text = Path(path).read_text().replace('\\\n', ' ')
    first = text.splitlines()[0]
    require(':' in first, 'Malformed compiler dependency file: ' + str(path))
    return {str(Path(p).resolve()) for p in shlex.split(first.split(':', 1)[1])}


def native_closure(pins):
    ready_path, plan_path = R / 'READY.json', R / 'BUILD_PLAN.json'
    ready, plan = B.read(ready_path), B.read(plan_path)
    for value in (ready, plan):
        require(value.get('native_only') is True and value.get('Windows_work_performed') is False
                and value.get('full_application_dependency_closure_verified') is True,
                'Native full dependency closure is not verified')
        require(Path(value['source']) == S, 'Native build uses another source stage')
    require(ready.get('ready') is True and ready.get('application_units') == 104,
            'Expected the current 104-unit native build')
    require(ready['source_sha256'] == plan['source_sha256'], 'Native source inventories differ')
    current = {str(p.relative_to(S)): B.sha(p) for p in S.rglob('*') if p.is_file()}
    require(not any(p.is_symlink() for p in S.rglob('*')), 'Source contains a symlink')
    require(current == ready['source_sha256'], 'Native source inventory is stale')
    origin = B.read(N / 'SOURCE_ORIGIN.json')
    require(Path(origin['baseline']) == SOURCE_BASE / 'source-v1', 'Unexpected source parent')
    require(B.sha(SOURCE_BASE / 'ROAD_POLISH_READY.json') == origin['baseline_ready_sha256'],
            'Source parent acceptance changed')
    old = origin['baseline_sha256']
    require(set(old) <= set(current), 'A baseline source file was removed')
    delta = {name for name in current if current[name] != old.get(name)}
    require(delta and delta <= {'engine/' + p for p in ALLOWED},
            'Source changes exceed the reviewed arena scope: ' + str(sorted(delta)))
    for name, digest in current.items():
        pins.add(S / name, digest)
    units = plan['units']
    parent_plan = B.read(SOURCE_BASE / 'runtime-native-v1/BUILD_PLAN.json')
    require(len(units) == 104 and len({v['logical_source'] for v in units}) == 104
            and {v['logical_source'] for v in units} == {v['logical_source'] for v in parent_plan['units']},
            'Native application unit set differs from the accepted 104-unit application')
    for unit in units:
        obj, dep = Path(unit['target']), Path(unit['dependency_file'])
        require(B.inside(obj, R / 'native-objects') and dep == obj.with_suffix('.d'),
                'Native object escaped the isolated runtime')
        proof_path = obj.with_suffix('.json')
        proof = B.read(proof_path)
        for key in ('logical_source', 'target', 'dependency_file', 'command'):
            require(proof[key] == unit[key], 'Object compiler receipt mismatch: ' + unit['logical_source'])
        command = unit['command']
        require(command[command.index('-c') + 1] == str(S / unit['logical_source'])
                and command[command.index('-o') + 1] == str(obj), 'Wrong compiled source/object')
        deps = proof['actual_dependency_sha256']
        require(str(S / unit['logical_source']) in deps and dep_paths(dep) == set(deps),
                'Incomplete actual compiler dependency closure: ' + unit['logical_source'])
        for path, digest in deps.items():
            require(B.inside(Path(path), S) or B.inside(Path(path), Path('/usr/include'))
                    or B.inside(Path(path), Path('/usr/lib')), 'Foreign compiler dependency: ' + path)
            pins.add(path, digest)
        for path in (obj, dep, proof_path):
            pins.add(path)
    require({str(Path(u['target'])) for u in units} <= set(plan['link']), 'Native link misses application objects')
    jolt = [Path(p) for p in plan['link'] if p.endswith('/libJolt.a')]
    require(len(jolt) == 1, 'Native physics library missing')
    pins.add(jolt[0], plan['jolt_library_sha256'])
    binary = Path(ready['binary'])
    require(binary == R / 'native-bin/rpg', 'Native binary escaped the isolated runtime')
    pins.add(binary, ready['sha256'])
    for p in (ready_path, plan_path, N / 'SOURCE_ORIGIN.json', N / 'build_native_candidate.py'):
        pins.add(p)
    return ready['sha256'], sorted(delta)


def public_project(pins):
    require(P.is_dir() and not P.is_symlink(), 'Arena project must be independent')
    require((P / 'saves').is_dir() and not (P / 'saves').is_symlink()
            and (P / 'saves').resolve().is_relative_to(P), 'Arena saves are shared')
    reference = BASE / 'arena-project'
    routes, files = {}, {}
    def visit(directory, relative=Path()):
        for p in directory.iterdir():
            rel = relative / p.name
            if B.mutable_relative(rel):
                continue
            old = reference / rel
            if p.is_symlink():
                require(old.is_symlink() and p.resolve(strict=True) == old.resolve(strict=True),
                        'Arena asset binding differs: ' + str(rel))
                routes[str(p)] = str(p.resolve())
            elif p.is_dir():
                require(old.is_dir() and not old.is_symlink(), 'Unexpected arena metadata directory')
                visit(p, rel)
            else:
                require(old.is_file() and not old.is_symlink() and B.sha(p) == B.sha(old),
                        'Arena authored metadata differs from its accepted copy: ' + str(rel))
                pins.add(p)
                files[str(rel)] = B.sha(p)
        expected = {p.name for p in (reference / relative).iterdir()
                    if not B.mutable_relative(relative / p.name)}
        actual = {p.name for p in directory.iterdir() if not B.mutable_relative(relative / p.name)}
        require(expected == actual, 'Arena metadata/root set differs: ' + str(relative))
    visit(P)
    require((P / 'player-profile.config').read_text() == 'VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n',
            'Arena progress must remain local')
    return routes, files


def review(pins, path):
    value = B.read(path)
    require(value.get('passed') is True, 'Review did not pass: ' + str(path))
    require(value.get('inputs') and value.get('outputs'), 'Review lacks actual input/output closure: ' + str(path))
    pins.add(path)
    for filename, digest in B.hash_pairs(value):
        if not private_or_mutable(filename):
            pins.add(filename, digest)
    return value


def validate_raw(value):
    require(value.get('passed') is True and value.get('matches') == 2 and value.get('captures', 0) >= 4,
            'Native arena scope must include both full matches and four captures')
    require(value.get('private_profile') is True and value.get('ordinary_start_and_geometry') is True
            and value.get('events_injected') is False, 'Native proof must use ordinary private game/geometry')
    rows = value.get('rows', [])
    require(len(rows) == 2 and {r.get('species') for r in rows} == {'Knights', 'Orcs'},
            'Native proof must cover Knights and Orcs')
    for row in rows:
        require((row.get('opponents'), row.get('simultaneous'), row.get('teammates')) == (24, 24, 6)
                and row.get('visible_peak', 0) >= 24 and row.get('team_body_contacts', 0) > 0,
                'Native capacity/visible/actual team body contact requirement failed')


def collect(preflight=False):
    # Parent owns the large immutable asset catalog. It rehashes changed stats
    # and small files; this child never establishes a new baseline for assets.
    subprocess.run([sys.executable, str(BASE / 'check_demo_final_ready.py'), '--check-only', '--arena'], check=True)
    inherited = B.read(BASE / 'DEMO_FINAL_READY.json')
    cache = {str(Path(p).resolve()): (v, inherited.get('frozen_stats', {}).get(p))
             for p, v in inherited['inputs'].items()}
    pins = B.Pins(cache)
    pins.add(BASE / 'check_demo_final_ready.py', BASE_CHECK_SHA)
    pins.add(BASE / 'DEMO_FINAL_READY.json', BASE_READY_SHA)
    native_sha, delta = native_closure(pins)
    routes, metadata = public_project(pins)
    for name in CPU:
        review(pins, N / name)
    extra = list((N / 'review').rglob('ARENA_TEAM_SOURCE_INTEGRATION_REVIEW.json'))
    require(len(extra) <= 1, 'Ambiguous team source integration review')
    if extra:
        review(pins, extra[0])
    pending = []
    if NATIVE.is_file():
        native = review(pins, NATIVE)
        require(native.get('native_sha256') == native_sha and native.get('native_gl_errors') == 0,
                'Native proof is stale or reports GL errors')
        raw = B.read(RAW)
        validate_raw(raw)
        require(native['inputs'].get(str(RAW)) == B.sha(RAW)
                or native['outputs'].get(str(RAW)) == B.sha(RAW), 'Formal native review does not pin its raw result')
    elif preflight:
        pending.append(str(NATIVE))
    else:
        raise RuntimeError('Final native arena review is pending: ' + str(NATIVE))
    for path in PUBLIC:
        pins.add(path)
    pins.unchanged()
    require(not any(private_or_mutable(p) for p in pins.hashes), 'Mutable user/private state was launch-pinned')
    return dict(schema='valwood.arena-settings.ready.v1', ready=not pending,
                native_only=True, Windows_work_performed=False, application_units=104,
                native_sha256=native_sha, source_delta=delta, inputs=pins.hashes,
                frozen_stats=pins.stats, asset_routes=routes, authored_metadata=metadata,
                parent_ready_sha256=BASE_READY_SHA, pending=pending,
                actual_hash_bytes=pins.hashed_bytes, inherited_cache_hits=pins.accepted_cache_files,
                mutable_policy='Local saves, purse, settings and character preferences remain writable and are never pinned.',
                limits=['Large battles over eight NPCs use balanced 512px NPC textures; source pose/geometry banks stay unchanged.',
                        'Representative bounded native capacity tests certify actual geometry contacts, not all matches or controller feel.',
                        'No hardware frame-rate, AAA animation, or native mouse navigation certification.'])


def check():
    saved = B.read(READY)
    require(saved.get('ready') is True and saved.get('schema') == 'valwood.arena-settings.ready.v1',
            'Arena has not been published')
    current = collect()
    require(saved['inputs'] == current['inputs'] and saved['asset_routes'] == current['asset_routes']
            and saved['source_delta'] == current['source_delta'] and saved['native_sha256'] == current['native_sha256'],
            'Published arena dependency closure changed')
    print('PASS reviewed arena: 24 opponents, six allies, Easy / Medium / Hard; player progress stays mutable.')


def self_test():
    raw = dict(passed=True, matches=2, captures=4, private_profile=True,
               ordinary_start_and_geometry=True, events_injected=False,
               rows=[dict(species=s, opponents=24, simultaneous=24, teammates=6,
                          visible_peak=30, team_body_contacts=1) for s in ('Knights', 'Orcs')])
    validate_raw(raw)
    for field, wrong in [('events_injected', True), ('captures', 3), ('matches', 1)]:
        bad = dict(raw, **{field: wrong})
        try:
            validate_raw(bad)
        except RuntimeError:
            continue
        raise RuntimeError('Native negative control was accepted: ' + field)
    require(private_or_mutable(P / 'saves/player.progress')
            and private_or_mutable(P / 'arena_player_character.cfg')
            and not private_or_mutable(S / 'engine/world_viewer.cpp'), 'Mutable-state policy failed')
    with tempfile.TemporaryDirectory() as tmp:
        dep = Path(tmp) / 'x.d'
        dep.write_text('/tmp/x.o: /tmp/a.h \\\n /tmp/b.h\n/tmp/a.h:\n/tmp/b.h:\n')
        require(dep_paths(dep) == {'/tmp/a.h', '/tmp/b.h'}, 'Compiler -MP parser failed')
    print('PASS readiness negative controls and mutable-state/compiler-dependency policy')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check-only', action='store_true')
    mode.add_argument('--publish', '--write', action='store_true')
    mode.add_argument('--preflight', action='store_true')
    mode.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.preflight:
        value = collect(True)
        print(json.dumps({k: value[k] for k in ('ready', 'native_sha256', 'pending', 'actual_hash_bytes', 'inherited_cache_hits')}, indent=2))
    elif args.publish:
        value = collect()
        temporary = READY.with_suffix('.tmp')
        temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
        os.replace(temporary, READY)
        print('Published ' + str(READY) + ' SHA256 ' + B.sha(READY))
    else:
        check()


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit('Arena readiness: ' + str(error))
