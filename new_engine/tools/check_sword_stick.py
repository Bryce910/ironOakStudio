#!/usr/bin/env python3
"""Check directional sword/guard control through an actual SDL virtual gamepad.

The native game uses ordinary E/R/Esc/V controls and controller axes/buttons in
a private copy of an existing city playtest. No combat health, phase, pose, or
story data are assigned by the driver. Physical controller hardware is outside
this regression's scope. Only one GPU game may be running while this is used.
"""
import argparse
import fcntl
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image, ImageStat

from benchmark_graphics_settings import native_environment
from check_opening_prologue import isolate_project, mutable_snapshot
from check_playtest_slices import timings

ROOT = Path(__file__).resolve().parents[1]
CAPTURES = {f'third-attack-{direction}' for direction in ('left', 'right', 'up', 'down')} | {
    f'{mode}-guard-{direction}' for mode in ('first', 'third') for direction in ('center', 'left', 'right', 'up', 'down')
} | {'first-attack-up'}
REQUIRED_CHECKS = {
    'grounded third-person practice lane', 'exclusive real SDL virtual controller',
    'ordinary E starts fresh countdown', 'countdown held stick cannot queue an attack',
    'countdown handoff requires neutral stick',
    'LT neutral selects center thrust guard without attack',
    'LT release with deflected stick never attacks',
    'direction change after LT release still requires neutral',
    'fresh neutral return rearms attack after guard',
    'same-frame LT stick RT X RB gives guard priority',
    'releasing alternate attack controls cannot replay guarded attack',
    'simultaneous guard release consumes held stick',
    'pause freezes combat and consumes simultaneous stick motion',
    'resume with held stick requires neutral before attack',
    'ordinary V selects first-person combat',
    'first-person upward flick performs rising cut after neutral',
    'first-person held stick does not repeat',
    'ordinary R restores grounded third-person exploration',
    'all four attacks five guards and first-person stroke captured',
}
for direction in ('left', 'right', 'up', 'down'):
    REQUIRED_CHECKS.update({
        f'direct flick {direction} starts matching attack without RT',
        f'held {direction} stick produces exactly one swing',
        f'LT {direction} selects matching guard without attack',
    })
REQUIRED_CHECKS.update(f'first-person LT {direction} reaches rendered guard without attack'
                       for direction in ('center', 'left', 'right', 'up', 'down'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def qa_failures(world):
    """Demand concrete scenario evidence rather than a standalone pass flag."""
    errors = []
    qa = world.get('sword_stick', {})
    if qa.get('qa') is not True or qa.get('passed') is not True:
        errors.append('directional sword SDL workflow incomplete')
    for name, minimum in {'checks': 43, 'events': 100}.items():
        value = qa.get(name)
        if type(value) is not int or value < minimum:
            errors.append('insufficient '+name)
    checks = qa.get('check_labels')
    if not isinstance(checks, list) or not all(isinstance(label, str) for label in checks) or not REQUIRED_CHECKS.issubset(checks):
        errors.append('missing attack/guard/neutral/pause scenario checks')
    names = qa.get('capture_names')
    if not isinstance(names, list) or len(names) != 15 or set(names) != CAPTURES or qa.get('captures') != 15:
        errors.append('missing four attacks ten first/third guard views and first-person strike')
    for name in ('combat_state_mutations', 'player_pose_writes', 'story_state_writes'):
        if qa.get(name) != 0:
            errors.append('driver bypassed ordinary gameplay: '+name)
    lane = world.get('playtest_combat', {})
    if lane.get('ready') is not True or lane.get('starts', 0) < 7:
        errors.append('ordinary E/R challenge fixtures did not run')
    for name in ('hero_draw_frames', 'enemy_visible_frames'):
        if lane.get(name, 0) < 1:
            errors.append('missing visible armed combatant: '+name)
    motion = world.get('combat_motion', {})
    if motion.get('actors') != 2 or motion.get('owned_pose_assets', 0) < 2 or motion.get('uploads', 0) < 1:
        errors.append('animated hero and opponent rigs missing')
    for name in ('gl_error', 'remaining_texture_references'):
        if world.get(name) != 0:
            errors.append(name)
    if world.get('asset_streaming', {}).get('failed') != 0:
        errors.append('asset streaming failed')
    if world.get('playtest_slice', {}).get('area') != 'city':
        errors.append('wrong playtest area')
    return errors


def capture_state_failures(name, state):
    """Bind a screenshot's label to the combat state at its render boundary."""
    errors = []
    if name not in CAPTURES or not isinstance(state, dict):
        return ['invalid rendered capture state: '+name]
    if state.get('name') != name or type(state.get('frame')) is not int or state['frame'] <= 0:
        errors.append('capture name/frame mismatch: '+name)
    third_person = name.startswith('third-')
    if state.get('third_person') is not third_person:
        errors.append('capture camera mode mismatch: '+name)
    direction = name.rsplit('-', 1)[-1]
    if '-guard-' in name:
        expected = {'left': 0, 'right': 1, 'up': 2, 'down': 3, 'center': 4}[direction]
        if state.get('selected') != expected or state.get('guard') is not True or state.get('player_phase') != 0:
            errors.append('capture is not the labeled active guard: '+name)
        pose = state.get('renderer_pose', {})
        if not isinstance(pose, dict): pose = {}
        weight, blend = pose.get('guard_weight'), pose.get('blend')
        if (pose.get('active') is not True or pose.get('slot') != 75+4*expected
                or pose.get('guard_direction') != expected or type(weight) not in (int, float)
                or not math.isfinite(weight) or not .95 <= weight <= 1
                or type(blend) not in (int, float) or not math.isfinite(blend) or not 0 <= blend <= 1):
            errors.append('renderer never reached labeled guard pose: '+name)
    else:
        expected = {'left': 0, 'right': 1, 'up': 3, 'down': 2}[direction]
        if (state.get('selected') != expected or state.get('attack_direction') != expected
                or state.get('guard') is not False or state.get('player_phase') != 2):
            errors.append('capture is not the labeled sword strike: '+name)
        stroke_time = state.get('player_time')
        if type(stroke_time) not in (int, float) or not math.isfinite(stroke_time) or not 0 <= stroke_time < .13:
            errors.append('capture strike timing invalid: '+name)
    for label in ('world_blade', 'viewmodel_blade'):
        blade = state.get(label, {})
        if not isinstance(blade, dict): blade = {}
        points = [blade.get('base'), blade.get('tip')]
        if (blade.get('available') is not True or
                not all(isinstance(point, list) and len(point) == 3 and
                        all(type(value) in (int, float) and math.isfinite(value) for value in point) for point in points)):
            errors.append('actual renderer blade sockets unavailable: '+name+' '+label)
    return errors


def isolate(source, destination):
    inputs = isolate_project(source, destination)
    # prepare_playtest_slices inventory intentionally excludes generated routes;
    # copy the already reviewed playtest route as an additional private input.
    path = source/'playtest.route'
    if path.is_symlink() or not path.is_file() or not 16 <= path.stat().st_size <= 1024*1024:
        raise ValueError('Supply an ordinary prepared city playtest with a bounded playtest.route')
    inputs['playtest.route'] = sha(path)
    shutil.copyfile(path, destination/'playtest.route')
    if sha(destination/'playtest.route') != inputs['playtest.route']:
        raise ValueError('Playtest route changed during private copy')
    # A playtest deliberately starts at its own route, not the apartment story.
    # Any opening opt-in is removed only from this new private QA copy.
    opening = destination/'opening.config'
    if opening.exists():
        opening.unlink()
        inputs.pop('opening.config', None)
    return inputs


def motion_inventory(directory):
    """Fingerprint the exact 216 immutable model inputs used by this runtime."""
    directory = Path(directory).resolve(strict=True)
    if not directory.is_dir(): raise ValueError('Motion override must be a pose-bank directory')
    result = {}
    for kind in ('body', 'arms'):
        for index in range(108):
            name = f'combat_{kind}_{index:03d}.glb'; path = directory/name
            if not path.is_file() or path.stat().st_size < 20:
                raise ValueError('Incomplete motion override: '+str(path))
            result[name] = sha(path)
    return result


def clone_motion_bank(project, source):
    """Install a private bank overlay without modifying either source bank.

    Model banks are read-only engine inputs. Linking individual models avoids
    duplicating the 1.1 GiB bank while allowing this copy alone to use new poses.
    """
    source = Path(source).resolve(strict=True)
    inputs = motion_inventory(source)
    models = project/'models'
    if not models.is_symlink(): raise ValueError('Expected fresh isolated model-bank link')
    original = models.resolve(strict=True)
    models.unlink(); models.mkdir()
    for child in sorted(original.iterdir()):
        if child.name == 'longsword_mocap': continue
        (models/child.name).symlink_to(child.resolve(strict=True), target_is_directory=child.is_dir())
    private = models/'longsword_mocap'; private.mkdir()
    for name in inputs:
        (private/name).symlink_to((source/name).resolve(strict=True))
    return private, inputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scale', type=float, default=.7)
    parser.add_argument('--timeout', type=int, default=400)
    parser.add_argument('--motion-directory', type=Path,
                        help='Immutable 216-pose candidate bank to install only in the private QA project')
    args = parser.parse_args()
    binary, project, out = args.binary.resolve(), args.project.resolve(), args.output.resolve()
    if not binary.is_file() or project.is_symlink() or not project.is_dir():
        parser.error('Supply an existing native runtime and ordinary project directory')
    if out == project or out.is_relative_to(project) or project.is_relative_to(out):
        parser.error('Evidence directory must be separate from the source project')
    if not .4 <= args.scale <= 1 or not 30 <= args.timeout <= 900:
        parser.error('Scale must be 0.4–1 and timeout 30–900 seconds')
    out.mkdir(parents=True, exist_ok=False)
    source_before = mutable_snapshot(project)
    binary_before = sha(binary)
    private = out/'project'
    inputs = isolate(project, private)
    motion_source, motion_before, motion_private = None, None, None
    if args.motion_directory:
        motion_source = args.motion_directory.resolve(strict=True)
        motion_private, motion_before = clone_motion_bank(private, motion_source)
        (out/'motion-inputs.json').write_text(json.dumps(motion_before, indent=2)+'\n')
    capture_dir = out/'captures'; capture_dir.mkdir()
    profile, final = out/'profile.json', out/'final.bmp'
    env, removed = native_environment()
    env.pop('VALWOOD_SHOWCASE', None)
    env.pop('VALWOOD_CAPTURE_VIEW', None)
    env.update(VALWOOD_PRESENTATION='1', VALWOOD_PLAYTEST_SLICE='city',
               VALWOOD_QA_SWORD_STICK='1', VALWOOD_QA_SWORD_STICK_CAPTURE_DIR=str(capture_dir),
               VALWOOD_QA_FRAME_TIMES_ONLY='1', SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    if motion_private:
        env['VALWOOD_QA_COMBAT_DIRECTORY'] = str(motion_private)
    command = [str(binary), '--project', str(private), '--play', '--scenic', 'city-walk',
               '--frames', '6000', '--warmup', '120', '--width', '1280', '--height', '720',
               '--auto-quality', 'off', '--render-scale', str(args.scale),
               '--profile', str(profile), '--capture', str(final)]
    (out/'command.json').write_text(json.dumps(dict(command=command,
        environment={name:value for name,value in env.items() if name.startswith('VALWOOD_') or name == 'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS'},
        removed_environment=removed), indent=2)+'\n')
    (out/'source-before.json').write_text(json.dumps(source_before, indent=2)+'\n')
    (out/'isolated-inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    started = time.monotonic()
    # Share the normal native preview lock; never interrupt an existing game.
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with (out/'native.log').open('w') as log:
            try:
                code = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                      stderr=subprocess.STDOUT, timeout=args.timeout).returncode
            except subprocess.TimeoutExpired:
                code = -1
    world_file = Path(str(profile)+'.world.json')
    world = json.loads(world_file.read_text()) if world_file.is_file() else {}
    metrics = json.loads(profile.read_text()) if profile.is_file() else {}
    errors = qa_failures(world)
    if code != 0: errors.append('native process exit '+str(code))
    renderer = str(metrics.get('renderer', ''))
    if not renderer or any(word in renderer.lower() for word in ('llvmpipe', 'softpipe', 'swrast', 'software')):
        errors.append('native GPU renderer unavailable')
    captures = {}
    for name in sorted(CAPTURES | {'final'}):
        source = final if name == 'final' else capture_dir/(name+'.bmp')
        if not source.is_file():
            errors.append('rendered capture missing: '+name); continue
        image = Image.open(source).convert('RGB')
        if image.width != 1280 or image.height != 720 or max(ImageStat.Stat(image).var) < 8:
            errors.append('invalid or blank rendered capture: '+name)
        output = out/'final.png' if name == 'final' else capture_dir/(name+'.png')
        image.save(output)
        captures[name] = dict(path=str(output), sha256=sha(output), width=image.width, height=image.height)
        if name != 'final':
            state_path = capture_dir/(name+'.json')
            try:
                state = json.loads(state_path.read_text())
            except (OSError, ValueError):
                errors.append('render-boundary capture state missing or invalid: '+name)
            else:
                errors += capture_state_failures(name, state)
                captures[name].update(state=state, state_path=str(state_path), state_sha256=sha(state_path))
    unchanged = mutable_snapshot(project) == source_before
    if not unchanged: errors.append('user project or saves changed')
    private_unchanged = all((private/name).is_file() and sha(private/name) == value for name,value in inputs.items())
    if not private_unchanged: errors.append('private authoring inputs changed during gameplay')
    binary_unchanged = sha(binary) == binary_before
    if not binary_unchanged: errors.append('runtime executable changed during regression')
    motion_unchanged = motion_before is None or motion_inventory(motion_source) == motion_before
    if not motion_unchanged: errors.append('candidate motion bank changed during regression')
    result = dict(passed=not errors, failures=errors, exit=code,
        elapsed_seconds=round(time.monotonic()-started, 2), binary=str(binary), binary_sha256=binary_before,
        source_project=str(project), isolated_project=str(private), renderer=renderer,
        sword_stick=world.get('sword_stick'), combat_motion=world.get('combat_motion'),
        playtest_combat=world.get('playtest_combat'), captures=captures,
        source_state_unchanged=unchanged, isolated_inputs_unchanged=private_unchanged,
        binary_unchanged=binary_unchanged, timing=timings(profile.with_suffix('.json.csv')),
        motion_source=str(motion_source) if motion_source else None,
        private_motion_bank=str(motion_private) if motion_private else None, motion_bank_unchanged=motion_unchanged,
        scope='Actual in-process SDL virtual controller, normal fight simulation, observed first/third-person captures; no combat/player/story field assignments. Physical controller and sustained performance are not certified by this run.')
    (out/'results.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print(json.dumps(dict(passed=result['passed'], failures=errors, checks=world.get('sword_stick', {}).get('checks'),
                         captures=len(captures), result=str(out/'results.json')), indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
