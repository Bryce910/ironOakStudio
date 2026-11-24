#!/usr/bin/env python3
"""Bounded actual arena event/audio/VFX rehearsal in a marked private save."""
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
BASE = N.parent / 'arena-orc-roster-2026-11-14'
P = N / 'feedback-review'
D = P / 'native-cases'
S = N / 'source-v1'
ORIGIN = N / 'arena-project'
PROJECT = P / 'private-feedback-project'
BINARY = N / 'runtime-native-v1/native-bin/rpg'
READY = N / 'runtime-native-v1/READY.json'
REPORT = P / 'COMBAT_FEEDBACK_NATIVE.json'

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
    return value

def prepare(inputs):
    marker = PROJECT / 'PRIVATE_CAPTURE_PROJECT.json'
    require(not PROJECT.exists() or marker.is_file(), 'Refusing an unmarked project')
    PROJECT.mkdir(exist_ok=True)
    marker.write_text(json.dumps({'schema': 'valwood.private-contact-feedback.v1',
                                 'origin': str(ORIGIN), 'private_saves_only': True}) + '\n')
    links = {}
    for original in sorted(ORIGIN.iterdir()):
        if original.name in {'saves', 'player.progress', 'studio-layout.ini',
                             'world.studio.workspace', 'arena_orc_character.cfg'}:
            continue
        target = PROJECT / original.name
        if original.is_symlink() or original.is_dir():
            resolved = original.resolve()
            require(not target.exists() or target.is_symlink() and target.resolve() == resolved,
                    'Private asset path differs')
            if not target.is_symlink():
                target.symlink_to(resolved, target_is_directory=original.is_dir())
            links[str(target)] = str(resolved)
        elif original.is_file():
            pin(original, inputs)
            shutil.copy2(original, target)
    (PROJECT / '.valwood-contact-feedback-qa').write_text('VALWOOD_PRIVATE_CONTACT_FEEDBACK_V1\n')
    (PROJECT / 'arena_orc_character.cfg').write_text('VALWOOD_ORC_CHARACTER 1 gnur end\n')
    helper_outputs = {}
    for name in ('write', 'read'):
        original = BASE / 'roster-review' / (name + '_capture_profile.cpp')
        helper = P / (name + '_capture_profile.cpp')
        require(not helper.exists() or helper.read_bytes() == original.read_bytes(), 'Private helper changed')
        if not helper.exists():
            shutil.copy2(original, helper)
        pin(helper, inputs)
        exe = helper.with_suffix('')
        command = ['/usr/bin/g++', '-std=c++17', '-O2', '-I' + str(S), str(helper), '-o', str(exe)]
        dependencies = subprocess.check_output(command[:-2] + ['-MM'], text=True).replace('\\\n', ' ')
        for item in shlex.split(dependencies.split(':', 1)[1]):
            pin(item, inputs)
        subprocess.run(command, check=True)
        pin(exe, helper_outputs)
    subprocess.run([str(P / 'write_capture_profile'), str(PROJECT), 'orc'], check=True)
    before = json.loads(subprocess.check_output([str(P / 'read_capture_profile'), str(PROJECT)], text=True))
    require(before['balance'] == before['wins'] == before['active_run_id'] == 0 and before['next_run_id'] == 1,
            'Private production save did not reset cleanly')
    preparation = {'schema': 'valwood.combat-feedback.native-project.v1', 'passed': True,
                   'inputs': inputs.copy(), 'outputs': helper_outputs, 'private_project': str(PROJECT),
                   'asset_links': links, 'profile_before': before,
                   'origin_saves_not_read_or_written': True}
    path = P / 'COMBAT_FEEDBACK_NATIVE_PROJECT.json'
    path.write_text(json.dumps(preparation, indent=2, sort_keys=True) + '\n')
    pin(path, inputs)
    inputs.update(helper_outputs)
    return before

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--review-existing', action='store_true')
    args = parser.parse_args()
    require(not (args.prepare_only and args.review_existing), 'Choose one mode')
    P.mkdir(exist_ok=True)
    D.mkdir(exist_ok=True)
    inputs = {}
    pin(__file__, inputs)
    if not args.prepare_only:
        ready = json.loads(READY.read_text())
        require(ready.get('ready') is True and ready['sha256'] == sha(BINARY), 'Native binary is not current')
        pin(READY, inputs)
        pin(BINARY, inputs)
        for path, digest in ready['source_sha256'].items():
            pin(S / path, inputs, digest)
    roster = (ORIGIN / 'arena_orcs_roster').resolve()
    for path in (roster / 'ORC_ROSTER.json', roster / 'ORC_ROSTER.cfg'):
        pin(path, inputs)
    # Only the selected frozen original Gnur geometry bank is exercised here.
    # The other13 accepted character assets are inherited, never rebaked.
    for path in sorted((roster / 'gnur').rglob('*')):
        if path.is_file():
            pin(path, inputs)
    hero_manifest = N / 'HERO_ASSET.json'
    pin(hero_manifest, inputs)
    hero = Path(json.loads(hero_manifest.read_text())['path'])
    pin(hero, inputs)
    loadouts = N / 'loadout-rigs/loadouts.manifest'
    pin(loadouts, inputs)
    tokens = shlex.split(loadouts.read_text())
    require(tokens[:2] == ['VALWOOD_SKELETAL_LOADOUTS', '1'] and len(tokens) == 11, 'Unexpected owned rig manifest')
    for i in (3, 6, 9):
        pin(tokens[i], inputs)
    for path in (N / 'shaders/world_skeletal.vs.glsl', N / 'shaders/rig_first_person.fs.glsl'):
        pin(path, inputs)
    for sound in ('slash', 'parry', 'impact', 'dash'):
        pin(ORIGIN / ('audio/presentation/' + sound + '.wav'), inputs)
    if args.review_existing:
        preparation_path = P / 'COMBAT_FEEDBACK_NATIVE_PROJECT.json'
        pin(preparation_path, inputs)
        preparation = json.loads(preparation_path.read_text())
        before = preparation['profile_before']
        for path, digest in preparation['inputs'].items():
            pin(path, inputs, digest)
        for path, digest in preparation['outputs'].items():
            pin(path, inputs, digest)
    else:
        before = prepare(inputs)
    if args.prepare_only:
        print('PASS private feedback project prepared; no game launched')
        return
    env = dict(os.environ)
    for key in list(env):
        if key.startswith(('VALWOOD_QA_', 'VALWOOD_TEST_', 'VALWOOD_PLAYTEST_',
                           'VALWOOD_DISABLE_', 'VALWOOD_CAPTURE_')) or key.endswith('_SEQUENCE') or key in {
                               'VALWOOD_OPENING_PROLOGUE', 'VALWOOD_SHOWCASE', 'VALWOOD_MODULAR_KNIGHT_THIRD_PERSON'}:
            env.pop(key, None)
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', LP_NUM_THREADS='4',
               MESA_DEBUG='1', LIBGL_ALWAYS_SOFTWARE='1', MALLOC_ARENA_MAX='2', MALLOC_TRIM_THRESHOLD_='131072',
               VALWOOD_PRESENTATION='1', VALWOOD_PLAYTEST_SLICE='city', VALWOOD_COMBAT_ARENA='1',
               VALWOOD_QA_CONTACT_FEEDBACK='1', VALWOOD_QA_CONTACT_FEEDBACK_DIR=str(D),
               VALWOOD_MODULAR_KNIGHT=str(hero), VALWOOD_MODULAR_KNIGHT_SHADER=str(N / 'shaders/world_skeletal.vs.glsl'),
               VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(N / 'shaders/rig_first_person.fs.glsl'),
               VALWOOD_SKELETAL_LOADOUTS=str(loadouts))
    command = [str(BINARY), '--project', str(PROJECT), '--play', '--scenic', 'city-walk', '--frames', '4000',
               '--hidden', '--width', '1280', '--height', '720', '--graphics-preset', 'performance',
               '--auto-quality', 'off', '--render-scale', '0.70']
    log = D / 'native-feedback.log'
    started = time.monotonic()
    if not args.review_existing:
        require(inputs == {path: sha(path) for path in inputs}, 'Capture inputs changed before startup')
        with log.open('w') as stream:
            subprocess.run(command, cwd=N.parents[1], env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
    elapsed = time.monotonic() - started
    text = log.read_text()
    raw_path = D / 'NATIVE_FEEDBACK_RAW.json'
    raw = json.loads(raw_path.read_text())
    require(raw.get('passed') is True and raw['reviewed_cases'] == raw['expected_cases'] == 28 and raw['captures'] == 84,
            'Actual native feedback scenarios failed: ' + raw.get('error', ''))
    require(raw['actual_training_events'] and raw['actual_owned_geometry'] and not raw['injected_outcome_events'],
            'Native run did not use actual production contact events')
    expected_names = {weapon + '-' + outcome + '-' + view for weapon in ('sword', 'shield', 'axe', 'staff')
                      for outcome in ('miss', 'blocked', 'body') for view in ('first', 'third')}
    expected_names.update('sword-' + outcome + '-' + view for outcome in ('block', 'parry') for view in ('first', 'third'))
    require({row['name'] for row in raw['cases']} == expected_names, 'Case identities differ')
    for row in raw['cases']:
        mode = row['mode']
        require(row['event_serial'] > 0 and row['audio_played'] > 0, 'No real event/successful nonzero audio playback')
        require(row['cue_kind'] == (1, 5, 2, 4, 6)[mode], 'Wrong outcome caption/shape')
        if mode == 0:
            require(all(row[key] == 0 for key in ('metal_bursts', 'body_bursts', 'metal_audio_played',
                        'body_audio_played', 'haptic_dispatches', 'camera_impulse', 'damage')), 'Air miss produced an impact')
        elif mode == 2:
            require(row['anatomical'] and row['body_bursts'] > 0 and row['body_audio_played'] > 0
                    and row['metal_bursts'] == row['metal_audio_played'] == 0 and row['damage'] > 0,
                    'Real body contact did not use body feedback')
        else:
            require(row['metal_bursts'] > 0 and row['metal_audio_played'] > 0
                    and row['body_bursts'] == row['body_audio_played'] == row['damage'] == 0,
                    'Steel contact did not use metal feedback')
            if mode == 1:
                require(row['anatomical'], 'Outgoing weapon block lacked real weapon geometry')
        if mode:
            require(row['haptic_dispatches'] > 0 and row['camera_impulse'] > 0, 'Actual impact missed feedback dispatch')
    require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|Combat feedback QA\] FAIL', text, re.I),
            'Native renderer/feedback failure logged')
    require(re.search(r'\[Skeletal hero candidate\].*draws=[1-9][0-9]*.*failures=0', text)
            and re.search(r'\[Skeletal first person\] draws=[1-9][0-9]* failures=0', text),
            'Actual first/third-person owned player rigs did not render cleanly')
    after = json.loads(subprocess.check_output([str(P / 'read_capture_profile'), str(PROJECT)], text=True))
    require(before['balance'] == after['balance'] == before['wins'] == after['wins'] == 0
            and after['next_run_id'] == 29, 'Private normal run allocation/purse changed unexpectedly')
    outputs = {}
    for path in (raw_path, log, PROJECT / 'saves/player.progress', PROJECT / 'arena_orc_character.cfg'):
        pin(path, outputs)
    images = []
    sheets = []
    for row in raw['cases']:
        sheet = Image.new('RGB', (3 * 426, 264), (18, 23, 29))
        draw = ImageDraw.Draw(sheet)
        for i, phase in enumerate(('before', 'impact', 'after')):
            bmp = D / (row['name'] + '-' + phase + '.bmp')
            pin(bmp, outputs)
            with Image.open(bmp) as image:
                require(image.size == (1280, 720), 'Wrong native capture resolution')
                rgb = image.convert('RGB')
                require(sum(ImageStat.Stat(rgb).stddev) > 12, 'Blank actual native scene')
                png = bmp.with_suffix('.png')
                rgb.save(png)
                pin(png, outputs)
                images.append(str(png))
                rgb.thumbnail((426, 239))
                sheet.paste(rgb, (i * 426, 25))
                draw.text((i * 426 + 8, 7), row['name'] + ' / ' + phase, fill='white')
        path = D / (row['name'] + '-contact-sheet.jpg')
        sheet.save(path, quality=92)
        pin(path, outputs)
        sheets.append(str(path))
    require(inputs == {path: sha(path) for path in inputs}, 'Frozen production/bank inputs changed during capture')
    report = {'schema': 'valwood.combat-feedback.native-review.v1', 'passed': True,
              'inputs': inputs, 'outputs': outputs, 'native_cases': raw, 'command': command,
              'environment': {key: value for key, value in env.items() if key.startswith('VALWOOD_') or key in {
                  'SDL_VIDEODRIVER', 'SDL_AUDIODRIVER', 'LP_NUM_THREADS', 'LIBGL_ALWAYS_SOFTWARE',
                  'MALLOC_ARENA_MAX', 'MALLOC_TRIM_THRESHOLD_'}},
              'profile_before': before, 'profile_after': after, 'captures': images, 'contact_sheets': sheets,
              'wall_seconds': None if args.review_existing else elapsed, 'actual_owned_geometry': True,
              'actual_training_events': True, 'injected_outcome_events': False,
              'successful_audio_playback_checked': True, 'miss_has_no_impact': True,
              'own_guard_and_enemy_block_distinct': True, 'private_saves_only': True,
              'live_game_or_editor_not_touched': True, 'hardware_performance_accepted': False,
              'controller_feel_accepted': False, 'human_and_roster_assets_inherited_unchanged': True,
              'limits': ['Software OpenGL, Performance preset and0.70render scale preserve1280x720 output.',
                         'Dummy SDL audio proves source playback and mixing success, not physical hearing.',
                         'Haptic requests are checked; no physical controller vibration is certified.',
                         'Outgoing body/steel contacts use actual owned mesh geometry; incoming guard/parry use ordinary AI resolution.',
                         'Only source Gnur is needed to test the unchanged shared feedback path; other13 asset banks are inherited.']}
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS actual28-case feedback native rehearsal:', REPORT)

if __name__ == '__main__':
    main()
