#!/usr/bin/env python3
"""Native apartment/TV acceptance in a fresh private project, ending in transport."""
import argparse
import fcntl
import json
import math
from pathlib import Path
import subprocess
import time

from PIL import Image, ImageStat
from benchmark_graphics_settings import native_environment
from check_first_journey import isolate, sha, source_snapshot
from check_opening_prologue import (
    GAME_START_CHECKS, GAME_START_CAPTURES, cinematic_captures,
    game_start_story, home_cinematic,
)
from check_playtest_slices import timings

ROOT = Path(__file__).resolve().parents[1]
OUTDOOR_CHECKS = {
    'transport_skip', 'controller_cinematic_skip', 'meadow_reveal',
    'meadow_jump_landing', 'meadow_walk', 'waystone_interaction',
    'checkpoint_retry', 'controller_interaction',
}
CHECKS = (GAME_START_CHECKS - OUTDOOR_CHECKS - {'house_skip'}) | {
    'house_cinematic_natural', 'house_cinematic_all_shots',
    'house_cinematic_match_handoff', 'console_only_complete',
}


def expected_captures(cinematic):
    return (GAME_START_CAPTURES - {'house-establish', 'meadow-reveal', 'waystone-complete'}) | cinematic_captures(cinematic) | {'console-objective'}


def qa_failures(world, cinematic):
    if not isinstance(world, dict):
        return ['Missing apartment world report']
    errors = []
    qa = world.get('opening_prologue_qa', {})
    labels = qa.get('checks')
    if (qa.get('enabled') is not True or qa.get('passed') is not True or
            qa.get('game_start_story') is not True or qa.get('phase') != 15):
        errors.append('Apartment-only SDL workflow did not finish at the transport capture')
    if not isinstance(labels, list) or not all(type(v) is str for v in labels) or len(labels) != len(CHECKS) or set(labels) != CHECKS:
        errors.append('Missing, duplicate or unexpected apartment/menu checks')
    for key in ('qa_player_pose_writes', 'qa_story_state_writes', 'capture_failures'):
        if type(qa.get(key)) is not int or qa[key] != 0:
            errors.append('Apartment replay bypassed ordinary input or failed capture: '+key)
    for key, minimum in [('movement_frames', 30), ('walked_m', 3), ('neutral_handoff_playable_frames', 6), ('checkpoint_restores', 1)]:
        value = qa.get(key)
        if type(value) not in (int, float) or not math.isfinite(value) or value < minimum:
            errors.append('Missing actual apartment control evidence: '+key)
    if qa.get('captures') != len(expected_captures(cinematic)):
        errors.append('Apartment captures incomplete')
    opening = world.get('opening_prologue', {})
    game = world.get('opening_game_start', {})
    if opening.get('stage') != 'transport' or opening.get('arrivals') != 0 or opening.get('skips') != 0:
        errors.append('Apartment rehearsal skipped or relocated outdoors')
    if game.get('new_games') != 1 or game.get('menus') != 2:
        errors.append('Real console cancel/reopen/fresh New Game sequence missing')
    entry = world.get('opening_entry_qa', {})
    error = entry.get('handoff_max_error')
    if (entry.get('enabled') is not True or entry.get('shot_count') != cinematic['shot_count'] or
            entry.get('shot_mask') != (1 << cinematic['shot_count'])-1 or entry.get('matched_handoff') is not True or
            type(error) not in (int, float) or not math.isfinite(error) or not 0 <= error <= .0001):
        errors.append('Natural apartment cinematic or matched camera handoff missing')
    for key in ('gl_error', 'remaining_texture_references'):
        if world.get(key) != 0:
            errors.append(key)
    if world.get('asset_streaming', {}).get('failed') != 0:
        errors.append('Apartment asset streaming failed')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scale', type=float, default=.8)
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    source = args.project.resolve(strict=True)
    binary = args.binary.resolve(strict=True)
    output = args.output.resolve()
    if (args.project.is_symlink() or not source.is_dir() or not binary.is_file() or
            output == source or output.is_relative_to(source) or source.is_relative_to(output)):
        parser.error('Use a regular source project, existing binary and separate fresh output')
    if not .5 <= args.scale <= 1 or not 90 <= args.timeout <= 600:
        parser.error('Scale must be 0.5..1 and timeout 90..600 seconds')
    cinematic = home_cinematic(source)
    if not game_start_story(source) or not cinematic:
        parser.error('Requires the actual apartment TV/New Game story and natural cinematic')
    output.mkdir(parents=True, exist_ok=False)
    before, binary_before = source_snapshot(source), sha(binary)
    project = output/'project'
    inputs = isolate(source, project)
    captures = output/'captures'
    captures.mkdir()
    profile, final = output/'profile.json', output/'final.bmp'
    env, removed = native_environment()
    for key in ('VALWOOD_VILLAGE_LIGHTING', 'VALWOOD_PLAYTEST_SLICE', 'VALWOOD_SHOWCASE', 'VALWOOD_CAPTURE_VIEW'):
        env.pop(key, None)
    env.update(VALWOOD_PRESENTATION='1', VALWOOD_OPENING_PROLOGUE='1',
               VALWOOD_QA_OPENING_PROLOGUE='1', VALWOOD_QA_OPENING_CINEMATIC='1',
               VALWOOD_QA_OPENING_CONSOLE_ONLY='1', VALWOOD_QA_OPENING_CAPTURE_DIR=str(captures),
               SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    command = [str(binary), '--project', str(project), '--play', '--scenic', 'city-meadow',
               '--frames', '9000', '--warmup', '120', '--width', '1280', '--height', '720',
               '--auto-quality', 'off', '--render-scale', str(args.scale),
               '--profile', str(profile), '--capture', str(final)]
    (output/'command.json').write_text(json.dumps(dict(command=command, removed_environment=removed,
        environment={k: v for k, v in env.items() if k.startswith(('VALWOOD_', 'SDL_'))}), indent=2)+'\n')
    (output/'source-before.json').write_text(json.dumps(before, indent=2)+'\n')
    (output/'isolated-inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    started = time.monotonic()
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('rb') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (output/'native.log').open('x') as log:
            try:
                code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                      stderr=subprocess.STDOUT, timeout=args.timeout).returncode
            except subprocess.TimeoutExpired:
                code = -1
    world_path = Path(str(profile)+'.world.json')
    world = json.loads(world_path.read_text()) if world_path.is_file() else {}
    metrics = json.loads(profile.read_text()) if profile.is_file() else {}
    errors = qa_failures(world, cinematic)
    if code != 0:
        errors.append('Native process exit '+str(code))
    renderer = str(metrics.get('renderer', ''))
    if not renderer or any(name in renderer.lower() for name in ('llvmpipe', 'softpipe', 'software', 'swrast')):
        errors.append('Native GPU renderer missing')
    images = {}
    for name in sorted(expected_captures(cinematic) | {'final'}):
        path = final if name == 'final' else captures/(name+'.bmp')
        if not path.is_file():
            errors.append('Actual apartment capture missing: '+name)
            continue
        with Image.open(path) as raw:
            image = raw.convert('RGB')
        target = output/'final.png' if name == 'final' else captures/(name+'.png')
        if image.size != (1280, 720) or max(ImageStat.Stat(image).var) < 8:
            errors.append('Blank/invalid apartment image: '+name)
        image.save(target)
        images[name] = dict(path=str(target), sha256=sha(target))
    unchanged = before == source_snapshot(source)
    inputs_unchanged = all((project/name).is_file() and sha(project/name) == digest for name, digest in inputs.items())
    binary_unchanged = sha(binary) == binary_before
    for valid, message in [(unchanged, 'Source project or saves changed'), (inputs_unchanged, 'Isolated inputs/shared banks changed'), (binary_unchanged, 'Runtime binary changed')]:
        if not valid:
            errors.append(message)
    result = dict(passed=not errors, failures=errors, exit=code, elapsed_seconds=round(time.monotonic()-started, 2),
        binary=str(binary), binary_sha256=binary_before, source_project=str(source), renderer=renderer,
        opening=world.get('opening_prologue', {}), qa=world.get('opening_prologue_qa', {}),
        game_start=world.get('opening_game_start', {}), captures=images,
        timing=timings(profile.with_suffix('.json.csv')), source_state_unchanged=unchanged,
        isolated_inputs_unchanged=inputs_unchanged, binary_unchanged=binary_unchanged,
        scope='Apartment-only real SDL input, natural opening, bills, save/load, TV held/cancel/reopen/fresh New Game and transport lock; stops before outdoor relocation. No QA pose/story writes. Functional acceptance, not a frame-rate benchmark.')
    (output/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(passed=result['passed'], failures=errors, result=str(output/'results.json')), indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
