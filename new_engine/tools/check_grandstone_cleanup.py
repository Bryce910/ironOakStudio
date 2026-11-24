#!/usr/bin/env python3
"""Check actual Grandstone stairs/doors sequentially on native hardware or Wine.

Uses a dedicated QA project and the normal move_camera/move_camera_vertical
runtime adapters. Independent case setup teleports are reported explicitly;
this is not a claim of one continuous journey through the whole castle.
"""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import math
import os
import re
import subprocess
import time

from PIL import Image
from benchmark_graphics_settings import native_environment
from check_playtest_slices import timings

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT/'build/grandstone-cleanup/qa/grandstone/project'
HEADER = ROOT/'engine/world_grandstone_qa_cases.h'
FAST = {0, 1, 2, 6}


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def windows(path):
    return 'Z:'+str(path.resolve()).replace('/', '\\')


def expected_paths():
    """Read the actual sealed, shared C++ point arrays for distance validation."""
    text = HEADER.read_text()
    paths = []
    for name, body in re.findall(r'inline constexpr Point path_(\w+)\[\]=\{(.*?)\n\};', text, re.S):
        points = [[float(value) for value in row.split(',')]
                  for row in re.findall(r'\{([^{}]+)\}', body)]
        if len(points) < 2 or any(len(point) != 3 or not all(math.isfinite(x) for x in point) for point in points):
            raise ValueError('Invalid sealed path '+name)
        distance = 2*sum(math.hypot(a[0]-b[0], a[2]-b[2]) for a, b in zip(points, points[1:]))
        paths.append(dict(name=name, nodes=len(points), expected_walked_m=distance))
    if len(paths) != 9:
        raise ValueError('Expected exactly nine sealed source paths')
    return paths


def protected_paths():
    baseline = ROOT/'docs/playtest-slices/2026-11-02/source-state-before.json'
    paths = [ROOT/name for name in json.loads(baseline.read_text())]
    # The user's playable projects and their private saves are also protected.
    for area in ('city', 'jungle', 'grandstone'):
        project = ROOT/'build/playtest-slices'/area/'project'
        for name in ('graphics.settings', 'world.living', 'playtest.route', 'playtest-hall.route'):
            if (project/name).is_file():
                paths.append(project/name)
        if (project/'saves').is_dir():
            paths.extend(path for path in (project/'saves').rglob('*') if path.is_file())
    return sorted(set(paths))


def qa_failures(world, expected, fast, capture_count):
    failures = []
    qa = world.get('grandstone_obstacle_qa', {})
    selected = [path for index, path in enumerate(expected) if not fast or index in FAST]
    if qa.get('enabled') is not True or qa.get('passed') is not True:
        failures.append('native obstacle workflow incomplete')
    if qa.get('fast_subset') is not fast:
        failures.append('incorrect selected-case scope')
    for field, required in {'paths_passed': len(selected), 'directions_passed': len(selected)*2,
                            'gate_tests': 4, 'gate_blocks': 4, 'captures': len(selected),
                            'capture_failures': 0, 'blocked_steps': 0, 'slope_steps': 0, 'dropped_steps': 0}.items():
        if qa.get(field) != required:
            failures.append(f'{field}: expected {required}, got {qa.get(field)}')
    if capture_count != len(selected):
        failures.append('missing or extra per-path native captures')
    if abs(qa.get('near_plane_m', 0)-.12) > .0001:
        failures.append('source camera near-plane correction inactive')
    wanted_distance = sum(path['expected_walked_m'] for path in selected)
    if abs(qa.get('walked_m', 0)-wanted_distance) > .02:
        failures.append('measured route distance disagrees with sealed paths')
    # Nine/four independent paths + four closed-gate sides + one jump setup.
    if qa.get('setup_teleports') != len(selected)+5:
        failures.append('independent case setup count')
    if qa.get('jump_landed') is not True or not 10 <= qa.get('jump_steps', 0) <= 180 or not .6 < qa.get('jump_rise_m', 0) < 1.3:
        failures.append('ordinary gravity jump and supported landing')
    rows = qa.get('cases', [])
    if len(rows) != len(expected):
        failures.append('per-path evidence missing')
    else:
        for index, (row, path) in enumerate(zip(rows, expected)):
            active = not fast or index in FAST
            if row.get('name') != path['name'] or row.get('selected') is not active:
                failures.append('per-path identity/scope '+path['name'])
                continue
            if not active:
                if row.get('forward') or row.get('reverse') or row.get('walked_m'):
                    failures.append('unselected path falsely counted '+path['name'])
                continue
            if row.get('forward') is not True or row.get('reverse') is not True:
                failures.append('missing actual bidirectional traversal '+path['name'])
            if row.get('steps', 0) < path['expected_walked_m']/(3.5*1.7/60)-1:
                failures.append('insufficient real move_camera steps '+path['name'])
            if row.get('nodes') != 2*(path['nodes']-1) or row.get('support_samples') != 4*(path['nodes']-1):
                failures.append('node support/idempotence sample count '+path['name'])
            if abs(row.get('walked_m', 0)-path['expected_walked_m']) > .01:
                failures.append('per-path distance '+path['name'])
            if row.get('maximum_node_error_m', 100) > .7 or row.get('maximum_resample_drift_m', 100) > .03:
                failures.append('source floor drift '+path['name'])
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True, help='Explicit owner-built staged runtime')
    parser.add_argument('--output', type=Path, required=True, help='New, non-existing evidence directory')
    parser.add_argument('--wine', action='store_true')
    parser.add_argument('--fast', action='store_true', help='Only two hall doorways, right first landing and north Barbican stair; both gates/jump remain tested')
    parser.add_argument('--scale', type=float, default=.7)
    parser.add_argument('--foliage-fragment', type=Path, help='Explicit isolated shader candidate; original projects remain untouched')
    args = parser.parse_args()
    if not .5 <= args.scale <= 1.5:
        parser.error('Choose render scale 0.5–1.5')
    binary = args.binary.resolve(strict=True)
    project = PROJECT.resolve(strict=True)
    if project == (ROOT/'build/playtest-slices/grandstone/project').resolve():
        parser.error('The isolated QA project must differ from the user playtest')
    if (project/'graphics.settings').is_symlink() or (project/'saves').is_symlink() or (project/'ai').is_symlink():
        parser.error('QA graphics, saves and AI must be private ordinary files/directories')
    out = args.output.resolve()
    if out.is_relative_to(project):
        parser.error('Evidence output must be outside the QA project')
    out.mkdir(parents=True, exist_ok=False)
    capture_dir = out/'captures'
    capture_dir.mkdir()
    expected = expected_paths()
    protected = {str(path): sha(path) for path in protected_paths()}
    inputs = [project/name for name in ('project.valwood', 'world.studio', 'playtest.route', 'playtest-hall.route',
              'playtest/approach.settings', 'playtest/approach.physics', 'playtest/hall.settings', 'playtest/hall.physics')]
    private_inputs = {str(path): sha(path) for path in inputs}
    binary_before = sha(binary)
    shader_candidate = args.foliage_fragment.resolve(strict=True) if args.foliage_fragment else None
    shader_before = sha(shader_candidate) if shader_candidate else None
    environment, removed = native_environment()
    environment.update(VALWOOD_PRESENTATION='1', VALWOOD_PLAYTEST_SLICE='grandstone', VALWOOD_QA_GRANDSTONE_OBSTACLES='1')
    environment.pop('VALWOOD_SHOWCASE', None)
    if args.fast:
        environment['VALWOOD_QA_GRANDSTONE_FAST'] = '1'
    translate = windows if args.wine else str
    if shader_candidate:
        environment['VALWOOD_QA_FOLIAGE_FRAGMENT'] = translate(shader_candidate)
    environment['VALWOOD_QA_GRANDSTONE_CAPTURE_DIR'] = translate(capture_dir)
    profile = out/'grandstone.json'
    final_capture = out/'grandstone.bmp'
    command = [translate(binary), '--project', translate(project), '--play', '--scenic', 'stronghold-town',
               '--frames', '20000', '--warmup', '120', '--width', '1600', '--height', '900',
               '--auto-quality', 'off', '--render-scale', str(args.scale), '--profile', translate(profile),
               '--capture', translate(final_capture)]
    if args.wine:
        environment.update(WINEPREFIX=str(ROOT/'build/windows/wine-prefix'), WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
        command = ['/usr/lib/wine/wine64']+command
    (out/'command.json').write_text(json.dumps(dict(command=command, removed_environment=removed,
        environment={key: value for key, value in environment.items() if key.startswith('VALWOOD_') or key == 'WINEPREFIX'}), indent=2)+'\n')
    (out/'protected-before.json').write_text(json.dumps(protected, indent=2)+'\n')
    started = time.monotonic()
    print('Grandstone actual movement QA: '+('Windows/Wine' if args.wine else 'native Linux')+(' selected 4 paths' if args.fast else ' all 9 paths'), flush=True)
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
        with (out/'grandstone.log').open('w') as log:
            try:
                code = subprocess.run(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=900).returncode
            except subprocess.TimeoutExpired:
                code = -1
    failures = []
    if code:
        failures.append('process exit '+str(code))
    metrics = json.loads(profile.read_text()) if profile.is_file() else {}
    world_path = Path(str(profile)+'.world.json')
    world = json.loads(world_path.read_text()) if world_path.is_file() else {}
    renderer = metrics.get('renderer', '')
    if not renderer or any(name in renderer.lower() for name in ('llvmpipe', 'swrast', 'softpipe')):
        failures.append('hardware GPU renderer missing')
    for field in ('gl_error', 'remaining_texture_references'):
        if world.get(field) != 0:
            failures.append(field)
    for field in ('failed', 'critical_pending'):
        if world.get('asset_streaming', {}).get(field, 1) != 0:
            failures.append('asset streaming '+field)
    if world.get('playtest_slice', {}).get('area') != 'grandstone' or world.get('playtest_slice', {}).get('enabled') is not True:
        failures.append('wrong playable slice')
    if world.get('camera_final', {}).get('fly') is not False:
        failures.append('final camera is not grounded')
    captures = []
    for path in sorted(capture_dir.glob('*.bmp')):
        target = path.with_suffix('.png')
        with Image.open(path) as image:
            if image.size != (1600, 900):
                failures.append('unexpected capture dimensions '+path.name)
            image.save(target)
        path.unlink()
        captures.append(dict(file=target.name, bytes=target.stat().st_size, sha256=sha(target)))
    if final_capture.is_file():
        with Image.open(final_capture) as image:
            image.save(final_capture.with_suffix('.png'))
        final_capture.unlink()
    else:
        failures.append('missing ordinary final-frame capture')
    failures += qa_failures(world, expected, args.fast, len(captures))
    source_unchanged = all(Path(path).is_file() and sha(Path(path)) == value for path, value in protected.items())
    inputs_unchanged = all(Path(path).is_file() and sha(Path(path)) == value for path, value in private_inputs.items())
    binary_unchanged = sha(binary) == binary_before
    shader_unchanged = not shader_candidate or sha(shader_candidate) == shader_before
    if not source_unchanged:
        failures.append('protected original/user state changed')
    if not inputs_unchanged:
        failures.append('sealed QA source inputs changed')
    if not binary_unchanged:
        failures.append('runtime changed during acceptance')
    if not shader_unchanged:
        failures.append('shader candidate changed during acceptance')
    timing = timings(profile.with_suffix('.json.csv'))
    timing['scope'] = 'Whole selected-path run including source streaming, independent case loading/warmup, held endpoints and actual per-path/final captures. Not a strict frame-floor certification.'
    result = dict(passed=not failures, failures=failures, exit=code,
        platform='Windows executable under Wine on AMD' if args.wine else 'Linux native on AMD',
        binary=str(binary), binary_sha256=binary_before, binary_unchanged=binary_unchanged,
        shader_candidate=str(shader_candidate) if shader_candidate else None, shader_candidate_sha256=shader_before, shader_candidate_unchanged=shader_unchanged,
        project=str(project), source_state_unchanged=source_unchanged, sealed_qa_inputs_unchanged=inputs_unchanged,
        sealed_header_sha256=sha(HEADER), elapsed_seconds=round(time.monotonic()-started, 2), renderer=renderer,
        qa=world.get('grandstone_obstacle_qa', {}), expected_paths=expected, captures=captures,
        timing=timing,
        scope='Actual move_camera source stair/door paths in both directions, real solid-gate blocking from both sides, source-ceiling query and ordinary move_camera_vertical gravity jump; explicit setup teleports isolate cases.',
        limitations='Per-path endpoint captures and a bounded movement regression; not a full-castle navigation, strict frame-floor, audible-audio or physical-controller acceptance.')
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Grandstone: '+('PASS' if result['passed'] else 'FAIL '+', '.join(failures)), flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
