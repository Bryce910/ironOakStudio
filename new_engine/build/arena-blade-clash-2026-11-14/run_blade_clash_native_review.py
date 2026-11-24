#!/usr/bin/env python3
"""Marked private arena starts, real geometry/events, and bounded framebuffer proof."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time
from PIL import Image, ImageDraw, ImageStat

N = Path(__file__).resolve().parent
S, P, O = N / 'source-v1', N / 'clash-review', N / 'arena-project'
D, PROJECT = P / 'native-cases', P / 'native-private-project'
B, READY = N / 'runtime-native-v1/native-bin/rpg', N / 'runtime-native-v1/READY.json'
REPORT = P / 'BLADE_CLASH_NATIVE.json'


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path, values, expected=None):
    path = Path(path).resolve()
    value = sha(path)
    require(expected is None or expected == value, 'Changed dependency: ' + str(path))
    values[str(path)] = value


def prepare(inputs):
    marker = PROJECT / 'PRIVATE_CAPTURE_PROJECT.json'
    require(not PROJECT.exists() or marker.is_file(), 'Refusing unmarked project')
    PROJECT.mkdir(exist_ok=True)
    marker.write_text(json.dumps({'schema': 'valwood.private-blade-clash.v1',
                                 'origin': str(O), 'private_saves_only': True}) + '\n')
    for source in O.iterdir():
        if source.name in {'saves', 'settings', 'player.progress', 'studio-layout.ini',
                           'world.studio.workspace', 'arena_player_character.cfg', 'graphics.settings'}:
            continue
        target = PROJECT / source.name
        if source.is_dir() or source.is_symlink():
            require(not target.exists() or target.is_symlink() and target.resolve() == source.resolve(),
                    'Private asset link differs')
            if not target.is_symlink():
                target.symlink_to(source.resolve(), target_is_directory=source.is_dir())
        elif source.is_file():
            pin(source, inputs)
            shutil.copy2(source, target)
    (PROJECT / '.valwood-blade-clash-qa').write_text('VALWOOD_PRIVATE_BLADE_CLASH_V1\n')
    original = N.parent / 'arena-character-roster-2026-11-14/roster-review'
    for name in ('write', 'read'):
        helper = P / (name + '_capture_profile.cpp')
        shutil.copy2(original / helper.name, helper)
        binary = helper.with_suffix('')
        command = ['/usr/bin/g++', '-std=c++17', '-O2', '-I' + str(S), str(helper), '-o', str(binary)]
        deps = subprocess.check_output(command[:-2] + ['-MM'], text=True).replace('\\\n', ' ')
        for path in shlex.split(deps.split(':', 1)[1]):
            pin(path, inputs)
        subprocess.run(command, check=True)
        pin(binary, inputs)
    subprocess.run([str(P / 'write_capture_profile'), str(PROJECT), 'human'], check=True)
    (PROJECT / 'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight01\nend\n')
    before = json.loads(subprocess.check_output([str(P / 'read_capture_profile'), str(PROJECT)], text=True))
    require(before['balance'] == before['wins'] == 0 and before['next_run_id'] == 1, 'Private profile is not fresh')
    return before


def ordered_names():
    values = [enemy + '-sword-' + mode + '-' + view for enemy in ('knight01', 'gnur')
              for mode in ('clash', 'guard') for view in ('third', 'first')]
    values.extend('gnur-' + weapon + '-' + mode + '-' + view
                  for weapon, mode in [('sword', 'air'), ('staff', 'body'), ('sword', 'parry'),
                                       ('shield', 'clash'), ('axe', 'clash'), ('staff', 'clash')]
                  for view in ('third', 'first'))
    for enemy in ('knight01', 'gnur'):
        for index, direction in enumerate(('left', 'right', 'high', 'low', 'thrust')):
            values.append(enemy + '-sword-matching-' + direction + ('-first' if index % 2 == 0 else '-third'))
    return values


def expected_names():
    return set(ordered_names())


def main():
    global D, REPORT
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--review-existing', action='store_true')
    parser.add_argument('--first-case', type=int, help='Private diagnostic first index, zero-based inclusive')
    parser.add_argument('--last-case', type=int, help='Private diagnostic last index, exclusive')
    args = parser.parse_args()
    require(not (args.prepare_only and args.review_existing), 'Choose one mode')
    require((args.first_case is None) == (args.last_case is None), 'Specify both diagnostic boundaries')
    diagnostic = args.first_case is not None
    begin, end = (args.first_case, args.last_case) if diagnostic else (0, 30)
    require(0 <= begin < end <= 30, 'Invalid bounded diagnostic range')
    if diagnostic:
        D = P / f'native-diagnostic-{begin}-{end}'
        REPORT = P / 'BLADE_CLASH_DIAGNOSTIC.json'
    selected_count = end - begin
    P.mkdir(exist_ok=True)
    D.mkdir(exist_ok=True)
    inputs = {}
    pin(__file__, inputs)
    for path in (N / 'HERO_ASSET.json', N / 'loadout-rigs/loadouts.manifest',
                 N / 'knights_roster/KNIGHT_ROSTER.cfg', N / 'arena_orcs_roster/ORC_ROSTER.cfg',
                 N / 'arena_orcs_roster/ORC_ROSTER.json'):
        pin(path, inputs)
    hero = Path(json.loads((N / 'HERO_ASSET.json').read_text())['path'])
    pin(hero, inputs)
    for index in (3, 6, 9):
        pin(shlex.split((N / 'loadout-rigs/loadouts.manifest').read_text())[index], inputs)
    # Only these two enemy appearances and the actual selected player are used.
    # Other accepted roster banks are inherited unchanged through the main gate.
    for relative in ('knights_roster/knight01', 'npc_knights/knight01',
                     'compact_orcs/gnur', 'arena_orcs_roster/gnur', 'shaders'):
        for path in sorted((N / relative).rglob('*')):
            if path.is_file():
                pin(path, inputs)
    for name in ('slash', 'impact', 'parry', 'dash'):
        pin(O / ('audio/presentation/' + name + '.wav'), inputs)
    if not args.prepare_only:
        ready = json.loads(READY.read_text())
        require(ready.get('ready') is True, 'Native build not ready')
        pin(READY, inputs)
        pin(B, inputs, ready['sha256'])
        for path, digest in ready['source_sha256'].items():
            pin(S / path, inputs, digest)
    preparation = D / 'PREPARATION.json'
    if args.review_existing:
        prep = json.loads(preparation.read_text())
        before = prep['profile_before']
        for path, digest in prep['inputs'].items():
            pin(path, inputs, digest)
    else:
        before = prepare(inputs)
        preparation.write_text(json.dumps({'profile_before': before, 'inputs': inputs}, indent=2) + '\n')
    pin(preparation, inputs)
    if args.prepare_only:
        print('PASS marked private clash profile prepared; no game launched')
        return
    env = dict(os.environ)
    for key in list(env):
        if key.startswith(('VALWOOD_QA_', 'VALWOOD_TEST_', 'VALWOOD_PLAYTEST_', 'VALWOOD_DISABLE_', 'VALWOOD_CAPTURE_')) or key.endswith('_SEQUENCE') or key in {
                'VALWOOD_OPENING_PROLOGUE', 'VALWOOD_SHOWCASE', 'VALWOOD_MODULAR_KNIGHT_THIRD_PERSON'}:
            env.pop(key, None)
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', LP_NUM_THREADS='2',
               MESA_DEBUG='1', LIBGL_ALWAYS_SOFTWARE='1', MALLOC_ARENA_MAX='2', MALLOC_TRIM_THRESHOLD_='131072',
               VALWOOD_PRESENTATION='1', VALWOOD_PLAYTEST_SLICE='city', VALWOOD_COMBAT_ARENA='1',
               VALWOOD_QA_BLADE_CLASH='1', VALWOOD_QA_BLADE_CLASH_DIR=str(D),
               VALWOOD_MODULAR_KNIGHT=str(hero), VALWOOD_MODULAR_KNIGHT_SHADER=str(N / 'shaders/world_skeletal.vs.glsl'),
               VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(N / 'shaders/rig_first_person.fs.glsl'),
               VALWOOD_SKELETAL_LOADOUTS=str(N / 'loadout-rigs/loadouts.manifest'))
    if diagnostic:
        env.update(VALWOOD_QA_CLASH_FIRST_CASE=str(begin), VALWOOD_QA_CLASH_LAST_CASE=str(end))
    command = [str(B), '--project', str(PROJECT), '--play', '--scenic', 'city-walk', '--frames', '3600',
               '--hidden', '--width', '1280', '--height', '720', '--graphics-preset', 'performance',
               '--auto-quality', 'off', '--render-scale', '0.70']
    log, raw_path = D / 'native-clash.log', D / 'NATIVE_CLASH_RAW.json'
    started = time.monotonic()
    if not args.review_existing:
        require(all(sha(path) == digest for path, digest in inputs.items()), 'Inputs changed before startup')
        raw_path.unlink(missing_ok=True)
        with log.open('w') as stream:
            subprocess.run(command, cwd=N.parents[1], env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
    raw = json.loads(raw_path.read_text())
    require(raw.get('passed') is True and raw['reviewed_cases'] == raw['expected_cases'] == selected_count
            and raw['captures'] == 4 * selected_count, 'Native clash rehearsal failed: ' + raw.get('error', ''))
    require(raw.get('partial_scope') is diagnostic and raw.get('first_case') == begin
            and raw.get('last_case_exclusive') == end, 'Native diagnostic/full scope differs')
    require(raw['actual_training_events'] and raw['actual_owned_geometry'] and raw['events_injected'] is False,
            'Rehearsal bypassed production geometry/events')
    require({row['name'] for row in raw['cases']} == set(ordered_names()[begin:end]), 'Native cases differ')
    outputs, captures, sheets = {}, [], []
    for trace in D.glob('*-incoming-trace.json'):
        value = json.loads(trace.read_text())
        require(value['production_result_unchanged'] is True and value['dropped'] == 0,
                'Incoming diagnostic trace changed results or overflowed')
        pin(trace, outputs)
    for path in (log, raw_path):
        pin(path, outputs)
    for row in raw['cases']:
        mode = row['mode']
        matching_air = row.get('air_from_actual_clear_window') is True
        require((row['event_serial'] == 0 if matching_air else row['event_serial'] > 0)
                and row['attack_serial'] > 0 and row['audio_played'] > 0
                and row['one_outcome_through_recovery'], 'Real event/audio/recovery proof missing')
        if mode == 5:
            require(row['direction'] == row['guard'] and row['distance'] == 1.25
                    and row['unblocked_probe_count'] > 0, 'Matching direction/body probe missing')
            require(diagnostic or not matching_air, 'Full acceptance requires actual steel in all ten primary matching guards')
        if mode == 0 or matching_air:
            require(all(row[key] == 0 for key in ('metal_bursts', 'body_bursts', 'metal_audio', 'body_audio',
                                                 'haptic_dispatches', 'damage')), 'Air fabricated an impact')
            if matching_air:
                require(mode == 5 and row['unblocked_body_threat'] is False,
                        'Incoming air accepted despite a real unblocked body threat')
        elif mode == 2:
            require(row['anatomical'] and row['body_bursts'] > 0 and row['body_audio'] > 0 and row['damage'] > 0
                    and row['metal_bursts'] == row['metal_audio'] == 0, 'Body contact used wrong feedback')
        else:
            require(row['anatomical'] and row['metal_bursts'] > 0 and row['metal_audio'] > 0
                    and row['body_bursts'] == row['body_audio'] == row['damage'] == 0
                    and row['haptic_dispatches'] > 0, 'Physical steel used wrong feedback')
            if mode == 1:
                require(row['combo_cleared'], 'Blocked swing retained a queued combo')
        sheet = Image.new('RGB', (4 * 320, 205), (16, 21, 27))
        draw = ImageDraw.Draw(sheet)
        for index, phase in enumerate(('before', 'contact', 'recoil', 'recovery')):
            bmp = D / (row['name'] + '-' + phase + '.bmp')
            pin(bmp, outputs)
            with Image.open(bmp) as image:
                require(image.size == (1280, 720), 'Native capture dimensions differ')
                rgb = image.convert('RGB')
                require(sum(ImageStat.Stat(rgb).stddev) > 12, 'Blank native scene')
                png = bmp.with_suffix('.png')
                rgb.save(png)
                pin(png, outputs)
                captures.append(str(png))
                rgb.thumbnail((320, 180))
                sheet.paste(rgb, (index * 320, 25))
                draw.text((index * 320 + 5, 6), row['name'] + ' / ' + phase, fill='white')
        path = D / (row['name'] + '-sheet.jpg')
        sheet.save(path, quality=94)
        pin(path, outputs)
        sheets.append(str(path))
    text = log.read_text()
    require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|Blade clash QA\] FAIL', text, re.I),
            'Native GL/clash failure logged')
    after = json.loads(subprocess.check_output([str(P / 'read_capture_profile'), str(PROJECT)], text=True))
    require(after['balance'] == after['wins'] == 0 and after['next_run_id'] == selected_count + 1,
            'Private normal run/reward allocation differs')
    pin(PROJECT / 'saves/player.progress', outputs)
    pin(PROJECT / 'arena_player_character.cfg', outputs)
    require(all(sha(path) == digest for path, digest in inputs.items()), 'Frozen input changed during native review')
    report = {'schema': 'valwood.blade-clash.native-review.v1', 'passed': True, 'inputs': inputs,
              'outputs': outputs, 'raw': raw, 'captures': captures, 'contact_sheets': sheets,
              'profile_before': before, 'profile_after': after, 'command': command,
              'wall_seconds': None if args.review_existing else time.monotonic() - started,
              'actual_owned_geometry': True, 'actual_training_events': True, 'events_injected': False,
              'private_saves_only': True, 'diagnostic_only': diagnostic,
              'all_thirty_cases_accepted': not diagnostic,
              'matching_actual_steel_cases': sum(row['mode'] == 5 and not row.get('air_from_actual_clear_window') for row in raw['cases']), 'user_session_untouched': True, 'hardware_fps_accepted': False,
              'controller_feel_accepted': False, 'audio_hearing_accepted': False,
              'parry_input_pattern': 'Held physical guard, ordinary release+press at NPC windup time100ms.',
              'limits': ['Software GL, Performance preset and0.70 render scale.',
                         'Dummy audio verifies successful playback, not physical hearing.',
                         'Motor commands are verified; physical vibration is not certified.',
                         'Placement and a single NPC windup are fixtures; contact and recoil remain production behavior.',
                         'Recovery captures show the actual core-recovery state; the separate GL proof checks the complete NPC visual return.']}
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS', selected_count, 'private diagnostic cases' if diagnostic else 'actual clash cases', '/', 4 * selected_count, 'captures:', REPORT)


if __name__ == '__main__':
    main()
