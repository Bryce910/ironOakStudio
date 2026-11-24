#!/usr/bin/env python3
"""Verify the isolated fresh-opening repair without freezing player progress."""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import subprocess

N = Path(__file__).resolve().parent
ENGINE = N.parents[1]
ROOT = ENGINE.parent
ARENA = N.parent / 'arena-settings-restoration-2026-11-15'
ROAD = N.parent / 'road-polish-2026-11-15'
S, P, R = N / 'source-v1', N / 'demo-project', N / 'runtime-native-v1'
READY = N / 'OPENING_LOAD_FIX_READY.json'
ARENA_CHECK_SHA = 'c44508e18e442d1f2da152defea58f7027a2578133bdc6ad9c205405a434d34a'
ARENA_READY_SHA = '12743496cfdddd5b1534471c43190d83ddcba2a00359cddc8fa6431584dd4843'
ROAD_CHECK_SHA = '2b74fd65bf869ea7315531ecdacc5405d932b1996dd2b09a87fcd2f839687194'
ROAD_READY_SHA = 'd1644385e6dd8582ade40ee611425d648b18b4cb5213fac976f4932e372f07d8'
CPU = ('review/new-journey/NEW_JOURNEY_CPU_REVIEW.json',
       'review/cutscene-neutral/OPENING_CUTSCENE_NEUTRAL_CPU_REVIEW.json')
FORMAL = N / 'review/startup/OPENING_STARTUP_NATIVE_REVIEW.json'
PUBLIC = (Path(__file__).resolve(), N / 'launch_demo.sh', N / 'OPENING_FIX_NOTES.txt',
          ROOT / 'Launch_Valwood_Demo_Fixed.sh')
ALLOWED = {'engine/' + name for name in (
    'world_viewer.cpp', 'world_cutscenes_runtime.inc', 'world_opening_prologue_runtime.inc',
    'world_opening_prologue_qa.inc', 'world_opening_startup_qa.inc')}


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


require(sha(ARENA / 'check_arena_ready.py') == ARENA_CHECK_SHA, 'Inherited arena checker changed')
spec = importlib.util.spec_from_file_location('accepted_arena_checker', ARENA / 'check_arena_ready.py')
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)
B = A.B


def inherited(pins):
    for stage, checker, expected, ready, ready_sha in (
        (ARENA, 'check_arena_ready.py', ARENA_CHECK_SHA, 'ARENA_SETTINGS_READY.json', ARENA_READY_SHA),
        (ROAD, 'check_road_polish_ready.py', ROAD_CHECK_SHA, 'ROAD_POLISH_READY.json', ROAD_READY_SHA),
    ):
        require(sha(stage / checker) == expected and sha(stage / ready) == ready_sha,
                'Inherited acceptance changed: ' + str(stage))
        subprocess.run([sys.executable, str(stage / checker), '--check-only'], check=True)
        pins.add(stage / checker, expected)
        pins.add(stage / ready, ready_sha)


def native(pins):
    ready_path, plan_path = R / 'READY.json', R / 'BUILD_PLAN.json'
    ready, plan = B.read(ready_path), B.read(plan_path)
    for value in (ready, plan):
        require(value.get('native_only') is True and value.get('Windows_work_performed') is False
                and value.get('full_application_dependency_closure_verified') is True
                and value.get('source') == str(S), 'Native closure/source is not the current isolated build')
    require(ready.get('ready') is True and ready.get('application_units') == 104,
            'Expected a passed 104-unit native build')
    require(not any(p.is_symlink() for p in S.rglob('*')), 'Source tree contains a symlink')
    source = {str(p.relative_to(S)): sha(p) for p in S.rglob('*') if p.is_file()}
    require(source == ready['source_sha256'] == plan['source_sha256'], 'Current native source inventory is stale')
    origin = B.read(N / 'SOURCE_ORIGIN.json')
    require(origin['baseline'] == str(ARENA / 'source-v1') and origin['baseline_ready_sha256'] == ARENA_READY_SHA
            and origin.get('canonical_source_edited') is False, 'Unexpected source origin')
    previous = origin['baseline_sha256']
    require(set(previous) <= set(source), 'Baseline source file removed')
    delta = {name for name in source if source[name] != previous.get(name)}
    require({'engine/world_viewer.cpp', 'engine/world_cutscenes_runtime.inc',
             'engine/world_opening_prologue_runtime.inc', 'engine/world_opening_prologue_qa.inc'} <= delta
            and delta <= ALLOWED, 'Source delta exceeds the reviewed opening repair: ' + str(sorted(delta)))
    for name, digest in source.items():
        pins.add(S / name, digest)
    units = plan['units']
    parent_plan = B.read(ARENA / 'runtime-native-v1/BUILD_PLAN.json')
    require(len(units) == 104 and len({v['logical_source'] for v in units}) == 104
            and {v['logical_source'] for v in units} == {v['logical_source'] for v in parent_plan['units']},
            'Native unit set differs from the accepted application')
    for unit in units:
        obj, dep = Path(unit['target']), Path(unit['dependency_file'])
        require(obj.is_relative_to(R / 'native-objects') and dep == obj.with_suffix('.d'), 'Foreign native object')
        receipt_path = obj.with_suffix('.json')
        receipt = B.read(receipt_path)
        for key in ('logical_source', 'target', 'dependency_file', 'command'):
            require(receipt[key] == unit[key], 'Compiler receipt mismatch: ' + unit['logical_source'])
        cmd = unit['command']
        require(cmd[cmd.index('-c') + 1] == str(S / unit['logical_source'])
                and cmd[cmd.index('-o') + 1] == str(obj), 'Compiler source/object mismatch')
        deps = receipt['actual_dependency_sha256']
        require(str(S / unit['logical_source']) in deps and A.dep_paths(dep) == set(deps),
                'Incomplete actual compiler dependency closure')
        for path, digest in deps.items():
            require(any(Path(path).is_relative_to(root) for root in (S, Path('/usr/include'), Path('/usr/lib'))),
                    'Foreign native dependency: ' + path)
            pins.add(path, digest)
        for path in (obj, dep, receipt_path):
            pins.add(path)
    require({u['target'] for u in units} <= set(plan['link']), 'Native link misses an application unit')
    jolt = [Path(p) for p in plan['link'] if p.endswith('/libJolt.a')]
    require(len(jolt) == 1, 'Native physics library missing')
    pins.add(jolt[0], plan['jolt_library_sha256'])
    require(ready['binary'] == str(R / 'native-bin/rpg'), 'Unexpected native binary')
    pins.add(ready['binary'], ready['sha256'])
    for p in (ready_path, plan_path, N / 'SOURCE_ORIGIN.json', N / 'build_native_candidate.py'):
        pins.add(p)
    return ready['sha256'], sorted(delta)


def mutable(path):
    path = Path(path).absolute()
    for root in (P, ROAD / 'demo-project', ARENA / 'arena-project'):
        if path.is_relative_to(root) and B.mutable_relative(path.relative_to(root)):
            return True
    if path.is_relative_to(N / 'review'):
        return any(q.name.startswith('private-project') or (q / 'project.valwood').is_file()
                   for q in path.parents if q.is_relative_to(N / 'review'))
    return False


def project(pins):
    require(P.is_dir() and not P.is_symlink() and (P / 'saves').is_dir()
            and not (P / 'saves').is_symlink() and (P / 'saves').resolve().is_relative_to(P),
            'Demo project/saves must be independent')
    reference = ROAD / 'demo-project'
    routes = {}
    def visit(directory, rel=Path()):
        expected = {p.name for p in (reference / rel).iterdir() if not B.mutable_relative(rel / p.name)}
        actual = {p.name for p in directory.iterdir() if not B.mutable_relative(rel / p.name)}
        require(actual == expected, 'Demo metadata/root set differs: ' + str(rel))
        for p in directory.iterdir():
            name = rel / p.name
            if B.mutable_relative(name):
                continue
            old = reference / name
            if p.is_symlink():
                require(old.is_symlink() and p.resolve(strict=True) == old.resolve(strict=True),
                        'Demo asset root differs: ' + str(name))
                routes[str(p)] = str(p.resolve())
            elif p.is_dir():
                require(old.is_dir() and not old.is_symlink(), 'Unexpected metadata directory')
                visit(p, name)
            else:
                require(old.is_file() and not old.is_symlink() and sha(p) == sha(old),
                        'Demo authored metadata differs: ' + str(name))
                pins.add(p)
    visit(P)
    require((P / 'player-profile.config').read_text() == 'VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n',
            'Demo progress must use its own local save')
    return routes


def review(pins, path):
    value = B.read(path)
    require(value.get('passed') is True and value.get('inputs') and value.get('outputs'),
            'Required proof lacks passed input/output closure: ' + str(path))
    pins.add(path)
    for filename, digest in B.hash_pairs(value):
        if not mutable(filename):
            pins.add(filename, digest)
    return value


def raw_proof(value, natural_mode, duration):
    require(value.get('passed') is True and value.get('enabled') is True
            and value.get('natural_mode') is natural_mode and value.get('controller_begin') is True,
            'Fresh controller opening result is missing')
    require(value.get('captures', 0) >= 3 and value.get('initial_held_frames') == 84
            and value.get('neutral_handoff_frames', 0) >= 6 and value.get('events', 0) > 0
            and value.get('qa_story_state_writes') == 0 and value.get('qa_player_pose_writes') == 0,
            'Native replay must preserve held-input/neutral/real-state/capture scope')
    if natural_mode:
        require(value.get('controller_skip') is False and value.get('shot_mask') == 15
                and value.get('natural_seconds', 0) >= duration - .10, 'Full natural four-shot timeline missing')
    else:
        require(value.get('controller_skip') is True and value.get('natural_seconds', 0) >= .8,
                'Fresh held controller skip/advancing clock missing')


def collect(preflight=False):
    # Each parent checks its own accepted large-asset stat/SHA catalog. Only
    # current local source/build/proof files establish new actual hash pins.
    accepted = [B.read(ARENA / 'ARENA_SETTINGS_READY.json'), B.read(ROAD / 'ROAD_POLISH_READY.json')]
    cache = {}
    for doc in accepted:
        for path, digest in doc['inputs'].items():
            cache[str(Path(path).resolve())] = (digest, doc.get('frozen_stats', {}).get(path))
    pins = B.Pins(cache)
    inherited(pins)
    native_sha, delta = native(pins)
    routes = project(pins)
    for path in CPU:
        review(pins, N / path)
    pending = []
    if FORMAL.is_file():
        formal = review(pins, FORMAL)
        require(formal.get('native_sha256') == native_sha and formal.get('native_gl_errors') == 0,
                'Native startup proof is stale or has GL errors')
        pairs = dict(B.hash_pairs(formal))
        require(pairs.get(str(R / 'READY.json')) == sha(R / 'READY.json')
                and pairs.get(str(R / 'native-bin/rpg')) == native_sha, 'Formal native proof lacks current build/binary pins')
        duration = sum(float(line.split()[1]) for line in (P / 'opening/cinematic.settings').read_text().splitlines()
                       if line.startswith('shot '))
        for name, natural_mode in (('NATIVE_OPENING_STARTUP_RAW.json', False), ('NATIVE_OPENING_NATURAL_RAW.json', True)):
            candidates = [Path(p) for p in pairs if Path(p).name == name]
            require(len(candidates) == 1 and candidates[0].is_relative_to(N / 'review/startup'),
                    'Native result missing/ambiguous: ' + name)
            raw_proof(B.read(candidates[0]), natural_mode, duration)
    elif preflight:
        pending.append(str(FORMAL))
    else:
        raise RuntimeError('Final skip/natural native review is pending')
    for path in PUBLIC:
        pins.add(path)
    pins.unchanged()
    require(not any(mutable(p) for p in pins.hashes), 'Mutable progress was accidentally launch-pinned')
    return dict(schema='valwood.opening-load-fix.ready.v1', ready=not pending, native_only=True,
                Windows_work_performed=False, application_units=104, native_sha256=native_sha,
                source_delta=delta, inputs=pins.hashes, frozen_stats=pins.stats, asset_routes=routes,
                pending=pending, actual_hash_bytes=pins.hashed_bytes, inherited_cache_hits=pins.accepted_cache_files,
                mutable_policy='Local story checkpoints, purse, graphics/preferences and optional media remain mutable.',
                limits=['Linux-only repair. No Windows build or Windows reproduction certification.',
                        'Native proof covers fresh controller Begin, held-input release/skip, natural four-shot opening and neutral handoff.',
                        'No full new story playthrough, controller hardware feel or performance claim.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--check-only', action='store_true')
    group.add_argument('--publish', '--write', action='store_true')
    group.add_argument('--preflight', action='store_true')
    group.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        sample = dict(passed=True, enabled=True, natural_mode=False, controller_begin=True, controller_skip=True,
                      captures=3, initial_held_frames=84, neutral_handoff_frames=6, events=4,
                      qa_story_state_writes=0, qa_player_pose_writes=0, natural_seconds=1.5)
        raw_proof(sample, False, 16)
        for key, bad in [('controller_skip', False), ('qa_story_state_writes', 1), ('captures', 2)]:
            try:
                raw_proof(dict(sample, **{key: bad}), False, 16)
            except RuntimeError:
                continue
            raise RuntimeError('Native negative control accepted: ' + key)
        require(mutable(P / 'saves/player.progress') and not mutable(S / 'engine/world_viewer.cpp'),
                'Mutable-state policy failed')
        print('PASS startup proof negative controls and mutable progress policy')
        return
    if args.preflight:
        value = collect(True)
        print(json.dumps({k: value[k] for k in ('ready', 'native_sha256', 'pending', 'actual_hash_bytes')}, indent=2))
    elif args.publish:
        value = collect()
        temp = READY.with_suffix('.tmp')
        temp.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
        os.replace(temp, READY)
        print('Published ' + str(READY) + ' SHA256 ' + sha(READY))
    else:
        saved = B.read(READY)
        require(saved.get('ready') is True and saved.get('schema') == 'valwood.opening-load-fix.ready.v1',
                'Opening repair is not published')
        current = collect()
        require(all(saved[k] == current[k] for k in ('inputs', 'asset_routes', 'native_sha256', 'source_delta')),
                'Published opening dependency closure changed')
        print('PASS fixed fresh opening; independent local progress remains writable.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit('Opening readiness: ' + str(error))
