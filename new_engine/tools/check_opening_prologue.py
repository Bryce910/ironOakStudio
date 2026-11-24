#!/usr/bin/env python3
"""Run the opening through ordinary SDL input in a fresh isolated project.

The native workflow must walk the house and meadow routes. It never writes the
player pose or quest stage; the authored transport and explicit save/retry are
the only position discontinuities accepted by the workflow. An SDL virtual
Xbox-compatible pad validates the engine controller path, not physical hardware.
"""
import argparse
import fcntl
import hashlib
import json
import math
import shlex
from pathlib import Path
import shutil
import subprocess
import time

from PIL import Image
from benchmark_graphics_settings import native_environment
from check_playtest_slices import timings
from prepare_playtest_slices import BANKS, COPIED_TREES, excluded, inventory

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_CHECKS = {
    'title_begin', 'house_cinematic_control_lock', 'house_skip',
    'first_person_father', 'controller_look', 'pause_freezes_player',
    'resume', 'house_walk', 'notebook_interaction', 'checkpoint_save',
    'checkpoint_restore', 'front_door_walk', 'front_door_interaction',
    'transport_control_lock', 'transport_skip', 'meadow_reveal',
    'meadow_jump_landing', 'meadow_walk', 'waystone_interaction',
    'checkpoint_retry', 'controller_cinematic_skip', 'controller_interaction', 'neutral_handoff',
    'menu_keyboard_confirm', 'controller_pause_menu',
}
REQUIRED_CAPTURES = {'house-establish', 'house-notebook', 'transport', 'meadow-reveal', 'waystone-complete'}
GAME_START_CHECKS = (REQUIRED_CHECKS - {'notebook_interaction', 'front_door_walk', 'front_door_interaction'}) | {
    'bills_interaction', 'console_walk', 'game_menu_open', 'game_menu_held_confirm',
    'game_menu_control_lock', 'game_menu_cancel', 'game_menu_reopen', 'game_menu_fresh_start',
}
GAME_START_CAPTURES = (REQUIRED_CAPTURES - {'house-notebook'}) | {'house-bills', 'game-menu', 'transport-glow'}
ROOM_LIGHT_CHECKS = {
    'room_lights_living_off_on_held', 'room_lights_kitchen_off_on_held',
    'room_lights_bedroom_off_on_held', 'room_lights_bathroom_off_on_held',
    'room_lights_pause_and_held_resume', 'room_lights_checkpoint_save_restore',
    'room_lights_checkpoint_retry', 'room_lights_walked_all_rooms',
}
ROOM_LIGHT_CAPTURES = {f'{room}-light-{state}' for room in ('living', 'kitchen', 'bedroom', 'bathroom') for state in ('on', 'off')}
ARRIVAL_CAMP_CHECKS = {
    'arrival_three_campers', 'arrival_controller_start', 'arrival_held_confirm',
    'arrival_dialogue_control_lock', 'arrival_cancel_no_credit',
    'arrival_rowan_complete', 'arrival_bram_complete', 'arrival_ellis_complete',
    'arrival_checkpoint_save', 'arrival_checkpoint_restore',
}
ARRIVAL_CAMP_CAPTURES = {'arrival-rowan', 'arrival-rowan-reopen', 'arrival-bram', 'arrival-ellis', 'arrival-camp-met'}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def windows(path):
    return 'Z:'+str(Path(path).resolve()).replace('/', '\\')


def game_start_story(project):
    """Read the bounded, explicit story opt-in; reject ambiguous inputs."""
    marker = Path(project)/'opening/game-start.settings'
    if marker.is_symlink():
        raise ValueError('Game-start marker must be an ordinary file')
    if not marker.exists():
        return False
    expected = b'VALWOOD_GAME_START 1\n'
    if not marker.is_file() or marker.stat().st_size != len(expected) or marker.read_bytes() != expected:
        raise ValueError('Invalid game-start marker; expected exact VALWOOD_GAME_START 1 record')
    return True


def home_cinematic(project):
    """Read the optional bounded shot manifest before isolating or launching."""
    marker = Path(project)/'opening/cinematic.settings'
    if marker.is_symlink():
        raise ValueError('Cinematic marker must be an ordinary file')
    if not marker.exists():
        return None
    if not marker.is_file() or not 32 <= marker.stat().st_size <= 8192:
        raise ValueError('Invalid cinematic marker size')
    try:
        rows = marker.read_text(encoding='ascii').splitlines()
        if rows[0] != 'VALWOOD_HOME_CINEMATIC 1' or rows[-1] != 'end' or not 3 <= len(rows) <= 8:
            raise ValueError('Invalid cinematic records')
        duration = 0.
        for row in rows[1:-1]:
            fields = shlex.split(row)
            if len(fields) != 17 or fields[0] != 'shot' or len(fields[-1]) > 240 or any(ord(c) < 32 for c in fields[-1]):
                raise ValueError('Invalid cinematic shot record')
            values = [float(v) for v in fields[1:16]]
            if not all(math.isfinite(v) for v in values) or not 1 <= values[0] <= 12 or any(abs(v) > 1000000 for v in values[1:13]) or any(not .35 <= v <= 1.5 for v in values[13:]):
                raise ValueError('Invalid cinematic shot bounds')
            duration += values[0]
        if duration > 45:
            raise ValueError('Cinematic duration exceeds bound')
    except (UnicodeError, ValueError, IndexError) as error:
        raise ValueError('Invalid apartment cinematic marker: '+str(error)) from error
    return dict(shot_count=len(rows)-2, duration=duration)


def cinematic_captures(cinematic):
    return {f'house-shot-{i+1}' for i in range(cinematic['shot_count'])} | {'house-handoff'}


def mutable_snapshot(project):
    """Fingerprint user state while leaving immutable multi-GB banks shared."""
    result = {}
    for item in sorted(project.iterdir()):
        if item.name in BANKS:
            result[item.name+'/'] = {'link_target': str(item.resolve(strict=True))}
            # Local overlays contain a few authored files among bank links.
            # rglob does not follow directory symlinks, keeping this bounded.
            if not item.is_symlink():
                for path in sorted(item.rglob('*')):
                    if path.is_file() and not path.is_symlink():
                        result[path.relative_to(project).as_posix()] = sha(path)
        elif item.is_file():
            result[item.name] = sha(item)
        elif item.name in (*COPIED_TREES, 'saves', 'opening') and item.is_dir():
            for path in sorted(item.rglob('*')):
                if path.is_file():
                    result[path.relative_to(project).as_posix()] = sha(path)
    return result


def isolate_project(source, target):
    """Copy configs/AI/shaders and create empty private saves before launching."""
    source, target = Path(source), Path(target)
    if source.is_symlink() or not source.is_dir():
        raise ValueError('Opening source must be an ordinary project directory')
    if target.exists() or target.is_symlink():
        raise ValueError('QA project must not already exist')
    source = source.resolve(strict=True)
    if target.resolve().is_relative_to(source):
        raise ValueError('QA project must be outside the user project')
    game_start_story(source)
    home_cinematic(source)
    files, banks = inventory(source)
    config = source/'opening.config'
    if config.exists():
        if config.is_symlink() or not config.is_file():
            raise ValueError('Opening config must be an ordinary file')
        files['opening.config'] = config
    # New opening support data are ordinary, bounded private input files.
    support = source/'opening'
    if support.exists():
        if support.is_symlink() or not support.is_dir():
            raise ValueError('Opening support directory must be private')
        for path in sorted(support.rglob('*')):
            if path.is_symlink():
                raise ValueError('Opening support inputs cannot be links')
            if path.is_file() and not excluded(path.name):
                files[path.relative_to(source).as_posix()] = path
    if sum(path.stat().st_size for path in files.values()) > 64*1024*1024:
        raise ValueError('Opening mutable runtime exceeds 64 MiB')
    digests = {name: sha(path) for name, path in files.items()}
    target.mkdir(parents=True)
    for name, path in files.items():
        output = target/name
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, output)
        if sha(output) != digests[name]:
            raise ValueError('Opening input changed while copying: '+name)
    for name, path in banks.items():
        (target/name).symlink_to(path, target_is_directory=True)
    (target/'saves').mkdir()
    return digests


def qa_failures(world, room_lights=False, game_start=False, cinematic=None, practical_count=6, arrival_camp=False, arrival_redesign=False):
    """Require specific movement/UI evidence instead of trusting a pass flag."""
    failures = []
    if arrival_redesign:
        creek = world.get('castle_creek', {})
        if (creek.get('legacy_surface_enabled') is not False or creek.get('source_scene_owns_water') is not True
                or creek.get('draw_frames') != 0 or creek.get('reflection_frames') != 0):
            failures.append('obsolete castle creek rendered over the authored arrival landscape')
    qa = world.get('opening_prologue_qa', {})
    if qa.get('enabled') is not True or qa.get('passed') is not True:
        failures.append('SDL opening workflow incomplete')
    checks = qa.get('checks', [])
    required_checks = GAME_START_CHECKS if game_start else REQUIRED_CHECKS
    required_captures = GAME_START_CAPTURES if game_start else REQUIRED_CAPTURES
    if arrival_camp:
        required_checks = required_checks | ARRIVAL_CAMP_CHECKS
        required_captures = required_captures | ARRIVAL_CAMP_CAPTURES
        arrival = world.get('opening_arrival_camp', {})
        if arrival.get('enabled') is not True or arrival.get('spoken_mask') != 7 or arrival.get('completed') != 3 or arrival.get('dialogue_active') is not False:
            failures.append('three first-contact conversations were not completed')
        driver = world.get('opening_arrival_qa', {})
        if any(driver.get(key) != value for key, value in dict(enabled=True, passed=True, phase=10, actors_completed=3, qa_player_pose_writes=0, qa_story_state_writes=0).items()):
            failures.append('arrival SDL dialogue/save workflow incomplete or bypassed gameplay')
        camp = world.get('arrival_camp_render', {})
        if any(camp.get(key) != value for key, value in dict(active=True, actors=3, pose_assets=12, failures=0, unsafe_roots=0).items()):
            failures.append('three animated arrival campers were not safely rendered')
        portraits = world.get('opening_arrival_portraits', {})
        errors = portraits.get('facing_errors_radians')
        if (portraits.get('qa_player_pose_writes') != 0 or portraits.get('maximum_allowed_radians') != .06
                or not isinstance(errors, list) or len(errors) != 3
                or not all(isinstance(value, (int, float)) and math.isfinite(value) and 0 <= value <= .06 for value in errors)):
            failures.append('all three speaking travelers did not face the player in their portraits')
    if cinematic:
        required_checks = (required_checks-{'house_skip'}) | {'house_cinematic_natural', 'house_cinematic_all_shots', 'house_cinematic_match_handoff'}
        required_captures = (required_captures-{'house-establish'}) | cinematic_captures(cinematic)
        entry = world.get('opening_entry_qa', {})
        if entry.get('enabled') is not True or entry.get('shot_count') != cinematic['shot_count'] or entry.get('shot_mask') != (1 << cinematic['shot_count'])-1:
            failures.append('natural apartment entry did not capture every authored shot')
        if entry.get('matched_handoff') is not True or not isinstance(entry.get('handoff_max_error'), (int, float)) or not 0 <= entry['handoff_max_error'] <= .0001:
            failures.append('cinematic endpoint did not match playable camera')
        for field, minimum in (('cinematic_frames', cinematic['duration']*50), ('observed_seconds', cinematic['duration']-.05)):
            if not isinstance(entry.get(field), (int, float)) or not math.isfinite(entry[field]) or entry[field] < minimum:
                failures.append('full natural cinematic runtime missing: '+field)
        if entry.get('duration_seconds') != cinematic['duration']:
            failures.append('runtime cinematic duration does not match authored settings')
        sound = world.get('opening_cinematic_entry', {})
        if sound.get('enabled') is not True or sound.get('shots') != cinematic['shot_count'] or sound.get('duration') != cinematic['duration']:
            failures.append('cinematic runtime opt-in does not match authored settings')
        if sound.get('audio_available') is not True:
            failures.append('cinematic audio output was unavailable')
        cue_mask = sound.get('cue_mask')
        cue_starts = sound.get('cue_starts')
        if not isinstance(cue_mask, int) or cue_mask & 3 != 3 or not isinstance(cue_starts, list) or len(cue_starts) != 4 or not all(isinstance(value, int) and value >= 0 for value in cue_starts) or cue_starts[1] < 1 or cue_starts[2] < 1 or cue_starts[3] < 2:
            failures.append('successful door bag and console audio playback missing')
    if not isinstance(checks, list) or not all(isinstance(check, str) for check in checks) or not required_checks.issubset(checks):
        failures.append('missing opening control/story checks')
    if game_start and qa.get('game_start_story') is not True:
        failures.append('game-start story mode not exercised')
    if game_start:
        game = world.get('opening_game_start', {})
        if game.get('enabled') is not True or not isinstance(game.get('menus'), int) or game['menus'] < 2:
            failures.append('console menu runtime events missing')
        if game.get('new_games') != 1 or game.get('screen_mode') != 0:
            failures.append('New Game did not start exactly one transport and reset its TV state')
    for field, minimum in {'events': 18, 'movement_frames': 30, 'walked_m': 15,
                           'controller_look_radians': .05, 'neutral_handoff_playable_frames': 6}.items():
        value = qa.get(field)
        if not isinstance(value, (int, float)) or value < minimum:
            failures.append('insufficient '+field)
    if qa.get('qa_player_pose_writes') != 0 or qa.get('qa_story_state_writes') != 0:
        failures.append('QA bypassed ordinary gameplay')
    if qa.get('checkpoint_restores', 0) < 1 or qa.get('checkpoint_retries', 0) < 1:
        failures.append('checkpoint restore/retry missing')
    if qa.get('capture_failures') != 0 or qa.get('captures', 0) < len(required_captures):
        failures.append('opening capture failure')
    story = world.get('opening_prologue', {})
    if story.get('enabled') is not True or story.get('stage') != 'complete':
        failures.append('opening story did not reach its waystone objective')
    if story.get('first_person') is not True:
        failures.append('ordinary father presentation was replaced by prototype hero')
    for field in ('gl_error', 'remaining_texture_references'):
        if world.get(field) != 0:
            failures.append(field)
    if world.get('asset_streaming', {}).get('failed') != 0:
        failures.append('asset streaming failure')
    if room_lights:
        electrical = world.get('opening_room_lights', {})
        if not isinstance(checks, list) or not ROOM_LIGHT_CHECKS.issubset(checks):
            failures.append('missing room-light SDL interaction/save/pause checks')
        if electrical.get('enabled') is not True or electrical.get('mask') != 15 or electrical.get('bindings') != practical_count:
            failures.append('room-light controls or circuit bindings incomplete')
        if electrical.get('toggles') != [2, 2, 2, 2]:
            failures.append('each room must switch off and on exactly once')
        powers = electrical.get('powers', [])
        if not isinstance(powers, list) or len(powers) != practical_count or not all(isinstance(power, (int, float)) and power > 0 for power in powers):
            failures.append('room practical powers were not restored')
    return failures


def film_evidence(raw, cinematic, entry):
    """Validate actual full-frame readbacks; leave owned raw frames for encoding."""
    raw = Path(raw)
    failures = []
    manifest = raw/'manifest.jsonl'
    records = []
    try:
        if manifest.is_symlink() or not manifest.is_file() or manifest.stat().st_size > 1024*1024:
            raise ValueError('missing or oversized film manifest')
        records = [json.loads(line) for line in manifest.read_text(encoding='ascii').splitlines()]
        if not 1 <= len(records) <= 700:
            raise ValueError('film frame count is outside its bound')
        expected_count = entry.get('film_frames')
        rendered_count = entry.get('film_rendered_frames')
        if entry.get('film_enabled') is not True or expected_count != len(records) or not isinstance(rendered_count, int) or (rendered_count+3)//4 != len(records):
            raise ValueError('runtime film counts do not match readbacks')
        if len(records) < cinematic['duration']*14.5 or len(records) > math.ceil(cinematic['duration']*15)+1:
            raise ValueError('film does not cover the authored duration at every fourth story frame')
        previous_story = -1.
        previous_wall = -1.
        shots = set()
        hashes = []
        for index, record in enumerate(records):
            filename = f'entry-{index:04d}.bmp'
            if record.get('file') != filename or record.get('output_frame') != index or record.get('rendered_frame') != index*4:
                raise ValueError('film readbacks are missing or reordered')
            if record.get('width') != 1600 or record.get('height') != 900:
                raise ValueError('film was not captured at full display resolution')
            story = record.get('story_seconds')
            wall = record.get('wall_seconds')
            progress = record.get('shot_progress')
            if not all(isinstance(value, (int, float)) and math.isfinite(value) for value in (story, wall, progress)) or not previous_story < story <= cinematic['duration'] or not previous_wall <= wall or not 0 <= progress <= 1:
                raise ValueError('film clocks or shot progress are invalid')
            shot = record.get('shot_index')
            if not isinstance(shot, int) or not 0 <= shot < cinematic['shot_count']:
                raise ValueError('film references an invalid shot')
            path = raw/filename
            if path.is_symlink() or not path.is_file():
                raise ValueError('film readback missing or linked')
            with Image.open(path) as image:
                if image.size != (1600, 900) or image.format != 'BMP':
                    raise ValueError('film readback is not a full native BMP')
            hashes.append(dict(file=filename, sha256=sha(path), bytes=path.stat().st_size))
            shots.add(shot)
            previous_story, previous_wall = story, wall
        if shots != set(range(cinematic['shot_count'])) or records[0]['story_seconds'] > .10 or records[-1]['story_seconds'] < cinematic['duration']-.10:
            raise ValueError('film does not include every shot from fade-in through handoff')
        if len(list(raw.glob('entry-*.bmp'))) != len(records):
            raise ValueError('film has extra unmanifested frames')
    except (ValueError, OSError, TypeError, KeyError) as error:
        failures.append(str(error))
    result = dict(passed=not failures, failures=failures, frames=len(records),
                  sample_every_rendered_frames=4, nominal_story_fps=15,
                  source_resolution=[1600, 900], raw_directory=str(raw),
                  manifest=str(manifest), silent=True,
                  scope='Actual native display readbacks during the natural apartment entry. Preview has no audio. Nominal playback follows the deterministic story clock; readback and BMP I/O timings are not a gameplay performance benchmark.')
    if not failures:
        result.update(first_story_seconds=records[0]['story_seconds'], last_story_seconds=records[-1]['story_seconds'],
                      captured_wall_seconds=records[-1]['wall_seconds'], frame_hashes=hashes)
    return result


def write_qa_paths(project):
    """Seal the authored route into a bounded text input for the native driver."""
    scene = json.loads((project/'opening.scene').read_text())
    home, meadow = scene['home']['walk_path'], scene['meadow']['walk_path']
    if not 5 <= len(home) <= 128 or not 2 <= len(meadow) <= 512:
        raise ValueError('Opening QA route node count is outside its bounds')
    rows = ['VALWOOD_OPENING_QA 1']
    for name, points in (('home', home), ('meadow', meadow)):
        rows.append(f'{name} {len(points)}')
        for point in points:
            if len(point) != 3 or not all(isinstance(x, (int, float)) and abs(x) <= 1000000 for x in point):
                raise ValueError('Opening QA route has an invalid coordinate')
            rows.append(' '.join(format(x, '.17g') for x in point))
    rows.append('end')
    path = project/'opening/qa.walk'
    path.write_text('\n'.join(rows)+'\n')
    return sha(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--project', type=Path, default=ROOT/'build/opening-prologue/project')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--wine', action='store_true')
    parser.add_argument('--room-lights', action='store_true', help='Walk all apartment switches and exercise E/A, hold, pause, save/load and retry')
    parser.add_argument('--cinematic-entry', action='store_true', help='Play every apartment shot naturally, capture each, and verify exact playable camera handoff')
    parser.add_argument('--cinematic-film', action='store_true', help='Keep bounded actual 1600x900 intro readbacks every fourth rendered frame for a silent preview; requires --cinematic-entry')
    parser.add_argument('--keep-focus', action='store_true', help='Keep this isolated QA game window active so normal focus-loss pause cannot consume its deadline')
    parser.add_argument('--scale', type=float, default=.7)
    args = parser.parse_args()
    if args.cinematic_film and not args.cinematic_entry:
        parser.error('--cinematic-film requires --cinematic-entry')
    if not .5 <= args.scale <= 1.5:
        parser.error('Render scale must be 0.5–1.5')
    binary = args.binary.resolve(strict=True)
    source = args.project
    if source.is_symlink():
        parser.error('Opening project cannot be a linked directory')
    source = source.resolve(strict=True)
    try:
        game_start = game_start_story(source)
        arrival_camp = (source/'opening/arrival-camp.settings').is_file()
        arrival_redesign = (source/'opening/arrival-redesign.json').is_file()
        cinematic = home_cinematic(source) if args.cinematic_entry else None
        if args.cinematic_entry and not cinematic:
            raise ValueError('Natural entry acceptance requires opening/cinematic.settings')
    except ValueError as error:
        parser.error(str(error))
    output = args.output.resolve()
    if output.is_relative_to(source):
        parser.error('Evidence output must be outside the playable project')
    output.mkdir(parents=True, exist_ok=False)
    before = mutable_snapshot(source)
    inputs = isolate_project(source, output/'project')
    project = output/'project'
    inputs['opening/qa.walk'] = write_qa_paths(project)
    captures = output/'captures'
    captures.mkdir()
    film_raw = output/'film/raw' if args.cinematic_film else None
    if film_raw:
        film_raw.mkdir(parents=True)
    profile, final = output/'opening.json', output/'opening.bmp'
    binary_before = sha(binary)
    environment, removed = native_environment()
    environment.pop('VALWOOD_VILLAGE_LIGHTING', None)
    environment.update(VALWOOD_PRESENTATION='1', VALWOOD_OPENING_PROLOGUE='1',
                       VALWOOD_QA_OPENING_PROLOGUE='1', SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    environment.pop('VALWOOD_QA_OPENING_FILM_DIR', None)
    environment.pop('VALWOOD_QA_OPENING_CINEMATIC', None)
    if args.cinematic_entry:
        environment['VALWOOD_QA_OPENING_CINEMATIC'] = '1'
    environment.pop('VALWOOD_QA_OPENING_ROOM_LIGHTS', None)
    if args.room_lights:
        environment['VALWOOD_QA_OPENING_ROOM_LIGHTS'] = '1'
    environment.pop('VALWOOD_PLAYTEST_SLICE', None)
    environment.pop('VALWOOD_SHOWCASE', None)
    translate = windows if args.wine else str
    environment['VALWOOD_QA_OPENING_CAPTURE_DIR'] = translate(captures)
    if film_raw:
        environment['VALWOOD_QA_OPENING_FILM_DIR'] = translate(film_raw)
    command = [translate(binary), '--project', translate(project), '--play', '--scenic', 'city-meadow',
               '--frames', '16000', '--warmup', '120', '--width', '1600', '--height', '900',
               '--auto-quality', 'off', '--render-scale', str(args.scale), '--profile', translate(profile),
               '--capture', translate(final)]
    if args.wine:
        environment.update(WINEPREFIX=str(ROOT/'build/windows/wine-prefix'), WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
        command = ['/usr/lib/wine/wine64']+command
    (output/'command.json').write_text(json.dumps(dict(command=command, removed_environment=removed,
        environment={key: value for key, value in environment.items() if key.startswith('VALWOOD_') or key == 'WINEPREFIX'}), indent=2)+'\n')
    (output/'source-before.json').write_text(json.dumps(before, indent=2)+'\n')
    (output/'isolated-inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    started = time.monotonic()
    focus_actions=[]
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
        with (output/'opening.log').open('w') as log:
            try:
                process = subprocess.Popen(command, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT)
                deadline=time.monotonic()+900
                try:
                    while process.poll() is None:
                        if time.monotonic()>deadline:raise subprocess.TimeoutExpired(command,900)
                        if args.keep_focus:
                            windows_out=subprocess.run(['wmctrl','-lp'],env=environment,capture_output=True,text=True,timeout=5)
                            for row in windows_out.stdout.splitlines():
                                fields=row.split(None,4)
                                if len(fields)!=5 or not fields[4].startswith('Legends of Valwood'):continue
                                # The exclusive game lock is held. Native windows
                                # must also belong to this exact child; Wine uses
                                # its internal Windows PID in the X11 property.
                                if not args.wine and fields[2]!=str(process.pid):continue
                                active=subprocess.run(['xprop','-root','_NET_ACTIVE_WINDOW'],env=environment,capture_output=True,text=True,timeout=5)
                                try:active_id=int(active.stdout.rsplit(' ',1)[-1].strip(),16)
                                except ValueError:active_id=-1
                                # Re-activating an already active Wine window
                                # can itself create needless focus transitions.
                                if active_id==int(fields[0],16):continue
                                action=subprocess.run(['wmctrl','-ia',fields[0]],env=environment,capture_output=True,timeout=5)
                                focus_actions.append(dict(seconds=round(time.monotonic()-started,2),window=fields[0],pid=fields[2],returncode=action.returncode))
                        time.sleep(.5)
                    code=process.returncode
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:process.wait(timeout=10)
                        except subprocess.TimeoutExpired:process.kill();process.wait()
            except subprocess.TimeoutExpired:
                code = -1
    (output/'focus-maintenance.json').write_text(json.dumps(dict(requested=args.keep_focus,actions=focus_actions),indent=2)+'\n')
    metrics = json.loads(profile.read_text()) if profile.is_file() else {}
    path = Path(str(profile)+'.world.json')
    world = json.loads(path.read_text()) if path.is_file() else {}
    # Require the authored fixture count for source-scene layouts; legacy
    # projects retain their exact six-fixture acceptance requirement.
    controls = source/'opening/room-controls.settings'
    practical_count = 6
    if controls.exists():
        if controls.is_symlink() or not controls.is_file() or controls.stat().st_size > 65536:
            raise ValueError('Invalid authored room-controls input')
        rows = controls.read_text(encoding='ascii').splitlines()
        if not rows or rows[0] != 'VALWOOD_OPENING_ROOM_CONTROLS 1':
            raise ValueError('Invalid authored room-controls header')
        practical_count = sum(row.startswith('practical ') for row in rows)
        if not 4 <= practical_count <= 32:
            raise ValueError('Invalid authored room practical count')
    failures = qa_failures(world, room_lights=args.room_lights, game_start=game_start, cinematic=cinematic,
                           practical_count=practical_count, arrival_camp=arrival_camp, arrival_redesign=arrival_redesign)
    film = film_evidence(film_raw, cinematic, world.get('opening_entry_qa', {})) if film_raw else None
    if film and not film['passed']:
        failures.extend('cinematic film: '+failure for failure in film['failures'])
    if film:
        (output/'film/results.json').write_text(json.dumps(film, indent=2)+'\n')
    if code:
        failures.append('process exit '+str(code))
    renderer = metrics.get('renderer', '')
    if not renderer or any(name in renderer.lower() for name in ('llvmpipe', 'softpipe', 'swrast')):
        failures.append('hardware GPU renderer missing')
    captures_written = []
    for path in sorted(captures.glob('*.bmp')) + ([final] if final.is_file() else []):
        Image.open(path).convert('RGB').save(path.with_suffix('.png'))
        captures_written.append(path.with_suffix('.png').relative_to(output).as_posix())
        path.unlink()
    expected_captures = (GAME_START_CAPTURES if game_start else REQUIRED_CAPTURES) | (ROOM_LIGHT_CAPTURES if args.room_lights else set())
    if arrival_camp:
        expected_captures = expected_captures | ARRIVAL_CAMP_CAPTURES
    if cinematic:
        expected_captures = (expected_captures-{'house-establish'}) | cinematic_captures(cinematic)
    if not expected_captures.issubset({Path(path).stem for path in captures_written}):
        failures.append('per-stage rendered captures missing')
    if not final.with_suffix('.png').is_file():
        failures.append('final rendered capture missing')
    unchanged = before == mutable_snapshot(source)
    if not unchanged:
        failures.append('user opening project changed')
    if sha(binary) != binary_before:
        failures.append('runtime changed during acceptance')
    result = dict(passed=not failures, failures=failures, platform='Windows/Wine' if args.wine else 'Linux native',
                  exit=code, elapsed_seconds=round(time.monotonic()-started, 2), binary_sha256=binary_before,
                  binary_unchanged=sha(binary) == binary_before, source_state_unchanged=unchanged,
                  renderer=renderer, opening=world.get('opening_prologue', {}), qa=world.get('opening_prologue_qa', {}),
                  room_lights=world.get('opening_room_lights', {}), room_lights_requested=args.room_lights,
                  cinematic_film_requested=args.cinematic_film, cinematic_film=film,
                  cinematic_entry_requested=args.cinematic_entry,
                  cinematic_entry=world.get('opening_entry_qa', {}),
                  cinematic_runtime=world.get('opening_cinematic_entry', {}),
                  game_start_story_requested=game_start,
                  game_start=world.get('opening_game_start', {}),
                  arrival_camp_requested=arrival_camp,
                  arrival_camp=world.get('opening_arrival_camp', {}),
                  arrival_qa=world.get('opening_arrival_qa', {}),
                  arrival_render=world.get('arrival_camp_render', {}),
                  arrival_portraits=world.get('opening_arrival_portraits', {}),
                  arrival_redesign_requested=arrival_redesign,
                  castle_creek=world.get('castle_creek', {}),
                  captures=captures_written, timing=timings(profile.with_suffix('.json.csv')),
                  scope='Actual SDL virtual-pad movement and tagged keyboard/UI actions; authored transport and explicit save/retry; no QA pose or story mutations. Physical controller hardware is not validated.')
    if film_raw:
        result['performance_scope'] = 'Functional acceptance with synchronous 1600x900 film readbacks and BMP writes; timings include capture I/O and must not be used as a gameplay performance benchmark.'
    (output/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
