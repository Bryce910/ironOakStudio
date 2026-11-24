#!/usr/bin/env python3
"""Verify the bounded attack-timing revision and its immutable arena baseline."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
ENGINE = BASE.parents[1]
WORKSPACE = ENGINE.parent
ANCESTOR = BASE.parent / 'arena-motion-polish-2026-11-13'
SOURCE = BASE / 'source-v1'
BINARY = BASE / 'runtime-native-v1/native-bin/rpg'
PUBLIC = WORKSPACE / 'Launch_Valwood_Arena_Attack_Flow_Test.sh'
READY = BASE / 'ATTACK_FLOW_READY.json'
SCHEMA = 'valwood.arena-attack-flow.ready.v1'
BASELINE_LAUNCHER = WORKSPACE / 'Launch_Valwood_Arena_Motion_Test.sh'


def require(value, message):
    if not value:
        raise RuntimeError(message)


class Closure:
    def __init__(self):
        self.values = {}
        self.resolved = {}
        self.cache = {}

    def pin(self, name, expected=None):
        path = Path(name).absolute()
        require(path.is_file(), 'Missing input: ' + str(path))
        target = str(path.resolve(strict=True))
        stat = path.stat()
        stamp = (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)
        key = (target, stamp)
        actual = self.cache.get(key)
        if actual is None:
            with path.open('rb') as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            after = path.stat()
            require(stamp == (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) and
                    target == str(path.resolve(strict=True)), 'Input changed during verification: ' + str(path))
            self.cache[key] = actual
        require(expected is None or actual == expected, 'Changed input: ' + str(path))
        require(str(path) not in self.values or self.values[str(path)] == actual,
                'Conflicting input: ' + str(path))
        self.values[str(path)] = actual
        self.resolved[str(path)] = target
        return actual

    def hashes(self, rows):
        require(isinstance(rows, dict) and rows, 'Missing hash map')
        for path, digest in rows.items():
            require(Path(path).is_absolute(), 'Relative dependency: ' + path)
            self.pin(path, digest)

    def proof(self, name, field):
        path = BASE / name
        self.pin(path)
        data = json.loads(path.read_text())
        require(data.get(field) is True, 'Failed proof: ' + str(path))
        for key in ('dependencies_sha256', 'source_sha256', 'before_sha256', 'after_sha256', 'output_sha256', 'outputs_sha256', 'reports_sha256'):
            rows = data.get(key)
            if isinstance(rows, dict) and rows:
                if key == 'source_sha256' and not all(Path(path).is_absolute() for path in rows):
                    require(name == 'runtime-native-v1/READY.json', 'Unbound relative source digest map')
                    continue  # Native source inventory is bound explicitly to SOURCE below.
                self.hashes(rows)
        for key in ('log', 'capture', 'report', 'binary'):
            if key in data and key + '_sha256' in data:
                self.pin(data[key], data[key + '_sha256'])
        return data


def bootstrap():
    mutable = {'graphics.settings', 'player-profile.config', 'studio-layout.ini', 'world.studio.workspace'}
    files = {}
    for dirname, dirs, names in os.walk(BASE / 'arena-project', followlinks=False):
        here = Path(dirname)
        dirs[:] = [name for name in dirs if name not in {'saves', 'profiles', 'models'} and not (here / name).is_symlink()]
        for name in names:
            if name in mutable or name.endswith(('.progress', '.tmp', '.workspace')):
                continue
            path = here / name
            if path.is_file():
                files[str(path.relative_to(BASE / 'arena-project'))] = path
    return files


def baseline(c):
    origin = json.loads((BASE / 'SOURCE_ORIGIN.json').read_text())
    require(Path(origin['baseline']) == ANCESTOR / 'source-v1', 'Unexpected source ancestor')
    c.pin(BASE / 'SOURCE_ORIGIN.json')
    c.pin(ANCESTOR / 'ARENA_MOTION_READY.json', origin['baseline_ready_sha256'])
    c.pin(BASELINE_LAUNCHER)
    result = subprocess.run([str(BASELINE_LAUNCHER), '--check-only'], capture_output=True, text=True, timeout=180)
    require(result.returncode == 0, 'Accepted baseline no longer verifies: ' + result.stderr.strip())
    require((BASE / 'arena-project/models').resolve(strict=True) == (ANCESTOR / 'pose-cook/bank/models').resolve(strict=True),
            'Attack revision selected another model bank')
    for path in (BASE / 'arena-project').rglob('*'):
        if path.is_symlink():
            previous = ANCESTOR / 'arena-project' / path.relative_to(BASE / 'arena-project')
            require(previous.is_symlink() and path.resolve(strict=True) == previous.resolve(strict=True),
                    'Private shared resource changed: ' + str(path))


def refresh():
    c = Closure()
    baseline(c)
    native = c.proof('runtime-native-v1/READY.json', 'ready')
    require(native['native_only'] is True and native['Windows_work_performed'] is False and
            native['application_units'] == 102 and native['full_application_closure_rebuilt'] is True,
            'Incomplete native closure')
    require(Path(native['source']) == SOURCE and Path(native['binary']) == BINARY, 'Wrong native candidate')
    paths = {str(p.relative_to(SOURCE)): p for p in SOURCE.rglob('*') if p.is_file()}
    require(set(paths) == set(native['source_sha256']), 'Changed source inventory')
    for name, digest in native['source_sha256'].items():
        c.pin(paths[name], digest)
    c.pin(BINARY, native['sha256'])
    plan = json.loads((BASE / 'runtime-native-v1/BUILD_PLAN.json').read_text())
    c.pin(BASE / 'runtime-native-v1/BUILD_PLAN.json')
    require(len(plan['units']) == 102 and not plan['skip_player'], 'Partial native plan')
    c.pin(ENGINE / 'build/physics/libJolt.a', plan['jolt_library_sha256'])
    for unit in plan['units']:
        path = Path(unit['target']).with_suffix('.json')
        c.pin(path)
        receipt = json.loads(path.read_text())
        require(receipt['command'] == unit['command'], 'Stale native command')
        c.hashes(receipt['actual_dependency_sha256'])
    for report, inputs in [('hero-animation-polish-check.json', 'ANIMATION_TEST_INPUTS.json'),
                           ('COMBAT_SEAMS.json', 'COMBAT_SEAMS_INPUTS.json')]:
        c.proof(report, 'accepted')
        path = BASE / inputs
        c.pin(path)
        data = json.loads(path.read_text())
        require(data['returncode'] == 0 and data['source_unchanged'] and data['before_sha256'] == data['after_sha256'],
                'CPU regression used changing source')
        c.hashes(data['before_sha256'])
        c.pin(data['report'], data['report_sha256'])
        c.pin(data['execution_command'][0], data['test_executable_sha256'])
    timing = c.proof('timing-review/COMBAT_TIMING_REVIEW.json', 'accepted')
    require(timing['schema'] == 'valwood.shared-combat-timing.review.v1' and timing['passed'] and
            timing['rates_hz'] == [30, 60, 120] and timing['production_timing']['checks'] >= 38801 and
            timing['production_timing']['combo_cases'] >= 150 and timing['production_timing']['contact_queries'] >= 510 and
            timing['first_person_selector']['checks'] >= 67916 and
            timing['first_person_selector']['production_selector_frames'] >= 8988 and
            timing['first_person_selector']['maximum_source_contact_error_m'] < .00001,
            'Timing, combo buffering or first-person selector checks are incomplete')
    flow = c.proof('attack-flow-review/ATTACK_FLOW_CPU.json', 'accepted')
    require(flow['passed'] and flow['all_25_pairs_each_rate'] and flow['rates_hz'] == [30, 60, 120] and
            flow['production_pairs'] == 75 and flow['fresh_pre_render_queries'] >= 675 and
            flow['current_queries'] >= 675 and flow['pure_query_pose_matrix_delta'] == 0 and
            flow['pre_render_blade_error_m'] < .00001, 'Production attack/contact synchronization is incomplete')
    for row in flow['speeds']:
        require(row['new_peak_mps'] < row['old_peak_mps'] if row['direction'] in (0, 1, 4)
                else row['new_peak_mps'] == row['old_peak_mps'], 'Attack speed comparison failed')
    gpu = c.proof('attack-flow-review/ATTACK_FLOW_GPU.json', 'passed')
    require(gpu['visual_review']['accepted'] is True, 'Attack rendering not reviewed')
    c.hashes(gpu['visual_review']['viewed_outputs_sha256'])
    geometry = c.proof('attack-geometry-review/CONTACT_CHECK.json', 'passed')
    require(geometry['schema'] == 'valwood.purchased-knight-attack-flow-contact-check.v1' and
            all(geometry.get(key) is True for key in ('actual_new_anatomy_used', 'actual_committed_query_timings',
                'pure_pre_render_explicit_duration_queries', 'pre_render_queries_preserve_pose')) and
            geometry['body_and_steel_checks'] == 320 and geometry['normal']['passed'] and geometry['normal']['checks'] == 160 and
            geometry['combo']['passed'] and geometry['combo']['checks'] == 160 and
            geometry['before_sha256'] == geometry['after_sha256'] and geometry['guard_endpoint_samples'] == 120 and
            geometry['guard_endpoint_max_error_m'] < .00001, 'Actual purchased-model contact acceptance is incomplete')
    c.pin(BASE / 'attack-geometry-review/contact-review/contact-fixture', geometry['binary_sha256'])
    smoke = c.proof('attack-flow-review/WORLD_SMOKE_ACCEPTED.json', 'accepted')
    require(smoke['binary_sha256'] == native['sha256'] and smoke['hero_failures'] == 0 and
            smoke['gl_error'] == 0 and smoke['variants']['available_mask'] == 15 and smoke['updates'] == 24,
            'Native integration proof used another build or failed')
    c.pin(smoke['binary'], smoke['binary_sha256'])
    c.pin(smoke['asset'], smoke['asset_sha256'])
    for path in bootstrap().values():
        c.pin(path)
    for path in [BASE / 'launch_knight_candidate.sh', PUBLIC, Path(__file__)]:
        c.pin(path)
    # Re-read each dependency to reject a report or source changing mid-release.
    for path, digest in list(c.values.items()):
        c.pin(path, digest)
    data = dict(schema=SCHEMA, accepted=True, accepted_utc=datetime.now(timezone.utc).isoformat(),
                binary=str(BINARY), binary_sha256=native['sha256'], dependencies_sha256=c.values,
                resolved_paths=c.resolved, bootstrap_inventory=sorted(bootstrap()),
                baseline_launcher=str(BASELINE_LAUNCHER), windows_work_performed=False,
                limits=['Private eight-opponent Linux arena test.',
                        'Scripted CPU and offscreen rendering; interactive controller feel requires playtesting.'])
    temporary = READY.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(READY)
    print('Attack-flow build ready: ' + str(len(c.values)) + ' verified dependencies.')


def check():
    require(READY.is_file(), 'Attack-flow build is still being checked')
    data = json.loads(READY.read_text())
    require(data['schema'] == SCHEMA and data['accepted'] is True and not data['windows_work_performed'], 'Not accepted')
    c = Closure()
    c.hashes(data['dependencies_sha256'])
    require(c.resolved == data['resolved_paths'] and data['bootstrap_inventory'] == sorted(bootstrap()), 'Changed release bindings')
    baseline(c)
    require(data['binary'] == str(BINARY) and c.values[str(BINARY)] == data['binary_sha256'], 'Wrong binary')
    print('Attack-flow arena preflight passed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--refresh', action='store_true')
    mode.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    try:
        refresh() if args.refresh else check()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print('Attack-flow arena preflight: ' + str(error), file=sys.stderr)
        sys.exit(1)
