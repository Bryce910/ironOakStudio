#!/usr/bin/env python3
"""Actual MP3 voice-over integration with live world shots and ordinary SDL controls.

The only story fixture is a sealed production-encoded MeadowStart checkpoint;
all interaction, skip, pause, credits and save/reload use production paths.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time

from PIL import Image, ImageStat

N = Path(__file__).resolve().parent
S = N / 'source-v1'
SOURCE = N / 'story-project'
R = N / 'review'
Q = R / 'native-intro-voice'
P = Q / 'private-project'
D = Q / 'captures'
B = N / 'runtime-native-v1/native-bin/rpg'
READY = N / 'runtime-native-v1/READY.json'
REPORT = R / 'NPC_INTRO_VOICE_NATIVE_REVIEW.json'
BANKS = {'models', 'assets', 'ground', 'surfaces', 'audio', 'environments', 'particles',
         'scenery', 'fonts', 'texture_blocks', 'collision', 'licenses', 'blood_assets'}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path, closure, expected=None):
    path = Path(path).resolve()
    value = sha(path)
    require(expected is None or value == expected, 'Changed dependency: ' + str(path))
    closure[str(path)] = value


def prepare(inputs):
    require(not P.exists() and not (D / 'PREPARATION.json').exists(), 'Previous private attempt exists; preserve/version it')
    P.mkdir(parents=True)
    D.mkdir(exist_ok=True)
    for item in SOURCE.iterdir():
        if item.name in {'saves', 'player-profile.config', 'studio-layout.ini', 'world.studio.workspace'} or '.recovery' in item.name:
            continue
        target = P / item.name
        if item.name in BANKS:
            require(item.is_dir() and not item.is_symlink(), 'Asset bank is not the independent snapshot')
            target.symlink_to(item, target_is_directory=True)
        elif item.is_dir():
            shutil.copytree(item, target)
        elif item.is_file():
            shutil.copy2(item, target)
    (P / 'PRIVATE_NPC_INTRO_PROJECT.json').write_text(json.dumps({
        'schema': 'valwood.npc-intro.private-project.v1', 'source_snapshot': str(SOURCE),
        'private_saves_only': True, 'assets_from_independent_stage_only': True}) + '\n')
    (P / '.valwood-npc-intro-qa').write_text('VALWOOD_PRIVATE_NPC_INTRO_V1\n')
    (P / 'saves').mkdir()
    fixture_root = R / 'native-voice-fixture'
    importer = S / 'tools/import_npc_intro_media.py'
    for source in (importer, fixture_root / 'synthetic-tone.mp3', fixture_root / 'supplied-story-captions.srt', fixture_root / 'FIXTURE.json'):
        pin(source, inputs)
    imported = subprocess.run(['/usr/bin/python3', str(importer), str(fixture_root / 'synthetic-tone.mp3'),
                               '--mode', 'voice', '--project', str(P),
                               '--subtitles', str(fixture_root / 'supplied-story-captions.srt')],
                              text=True, capture_output=True, check=True)
    (Q / 'MP3_IMPORT_RESULT.json').write_text(imported.stdout)
    pin(Q / 'MP3_IMPORT_RESULT.json', inputs)
    (P / 'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
    shutil.copy2(N / 'shared-profile/player.progress', P / 'saves/player.progress')
    helper = Q / 'write_intro_checkpoint.cpp'
    fixture = (R / 'check_story_config.cpp').read_text().replace('if(argc!=2)return 2;', 'if(argc!=2)return 2;')
    position = fixture.index(' std::cout<<')
    fixture = fixture[:position] + '''
 world_opening_prologue::Save saved=world_opening_prologue::checkpoint_save(world_opening_prologue::Checkpoint::MeadowStart,c);
 world_first_journey::State journey_state;
 if(!journey_state.configure(c.first_journey,error))return 1;
 saved.first_journey_present=true;saved.first_journey=journey_state.stable_save();
 saved.inventory_present=true;saved.inventory={};saved.npc_intro_seen=false;
 std::string bytes;
 if(!world_opening_prologue::encode_save(saved,c,bytes,error)){std::cerr<<error<<'\\n';return 1;}
 if(!std::filesystem::is_regular_file(root/"PRIVATE_NPC_INTRO_PROJECT.json"))return 2;
 std::ofstream seed(root/"opening/npc-intro.qa.seed",std::ios::binary),progress(root/"saves/opening.progress",std::ios::binary);
 seed<<bytes;progress<<bytes;seed.close();progress.close();if(!seed||!progress)return 1;
 std::cout<<"Production-encoded MeadowStart checkpoint, camp0, unseen intro, no quest inventory\\n";
}
'''
    helper.write_text(fixture)
    binary = helper.with_suffix('')
    command = ['/usr/bin/g++', '-std=c++17', '-O2', '-I' + str(S), str(helper), '-o', str(binary)]
    deps = subprocess.check_output(command[:-2] + ['-MM'], text=True).replace('\\\n', ' ')
    for path in shlex.split(deps.split(':', 1)[1]):
        pin(path, inputs)
    subprocess.run(command, check=True)
    subprocess.run([str(binary), str(P)], check=True)
    pin(binary, inputs)
    for path in sorted(P.rglob('*')):
        if path.is_file() and not path.is_relative_to(P / 'saves'):
            pin(path, inputs)
    # rglob deliberately does not recurse directory symlinks. Hash every actual
    # stage-owned immutable bank, caching hardlink aliases instead of rereading.
    cache = {}
    for name in sorted(BANKS):
        root = SOURCE / name
        if not root.is_dir():
            continue
        for path in sorted(root.rglob('*')):
            if path.is_file():
                st = path.stat()
                key = (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns)
                cache.setdefault(key, None)
                if cache[key] is None:
                    cache[key] = sha(path)
                inputs[str(path.resolve())] = cache[key]
    prep = {'schema': 'valwood.npc-intro.native-preparation.v1', 'inputs': inputs,
            'initial_profile_sha256': sha(P / 'saves/player.progress'),
            'initial_opening_checkpoint_sha256': sha(P / 'saves/opening.progress'),
            'live_state_injected': False, 'ordinary_checkpoint_fixture': True,
            'private_project': str(P), 'source_snapshot': str(SOURCE)}
    (D / 'PREPARATION.json').write_text(json.dumps(prep, indent=2, sort_keys=True) + '\n')
    return prep


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--review-existing', action='store_true')
    args = parser.parse_args()
    require(not (args.prepare_only and args.review_existing), 'Choose one mode')
    R.mkdir(exist_ok=True)
    Q.mkdir(exist_ok=True)
    inputs = {}
    pin(__file__, inputs)
    pin(R / 'STORY_PROJECT_SNAPSHOT.json', inputs)
    prep_path = D / 'PREPARATION.json'
    if prep_path.exists():
        prep = json.loads(prep_path.read_text())
        for path, value in prep['inputs'].items():
            pin(path, inputs, value)
    else:
        require(not args.review_existing, 'Missing original private preparation')
        prep = prepare(inputs)
    pin(prep_path, inputs)
    if args.prepare_only:
        print('PASS sealed private MeadowStart story prepared; no native process launched')
        return
    ready = json.loads(READY.read_text())
    require(ready.get('ready') is True, 'Current native is not ready')
    pin(READY, inputs)
    pin(B, inputs, ready['sha256'])
    for path, value in ready['source_sha256'].items():
        pin(S / path, inputs, value)
    env = dict(os.environ)
    for key in list(env):
        if key.startswith(('VALWOOD_QA_', 'VALWOOD_TEST_', 'VALWOOD_PLAYTEST_', 'VALWOOD_DISABLE_', 'VALWOOD_CAPTURE_', 'VALWOOD_MODULAR_KNIGHT', 'VALWOOD_SKELETAL_')) or key.endswith('_SEQUENCE') or key in {'VALWOOD_COMBAT_ARENA', 'VALWOOD_SHOWCASE'}:
            env.pop(key, None)
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', LIBGL_ALWAYS_SOFTWARE='1',
               LP_NUM_THREADS='2', MALLOC_ARENA_MAX='2', MALLOC_TRIM_THRESHOLD_='131072', MESA_DEBUG='1',
               VALWOOD_PRESENTATION='1', VALWOOD_OPENING_PROLOGUE='1', VALWOOD_QA_NPC_INTRO='1',
               VALWOOD_QA_NPC_INTRO_DIR=str(D), VALWOOD_QA_NPC_INTRO_FILM='1')
    command = [str(B), '--project', str(P), '--play', '--frames', '6000', '--hidden',
               '--width', '1280', '--height', '720', '--graphics-preset', 'performance',
               '--auto-quality', 'off', '--render-scale', '0.70']
    started = time.monotonic()
    log = D / 'native.log'
    if not args.review_existing:
        require(not log.exists() and not (D / 'NPC_INTRO_NATIVE_RAW.json').exists(), 'Previous native attempt exists')
        require(all(sha(path) == value for path, value in inputs.items()), 'Input changed before native startup')
        with log.open('w') as output:
            subprocess.run(command, cwd=N.parents[1], env=env, stdout=output, stderr=subprocess.STDOUT, check=True)
    raw_path = D / 'NPC_INTRO_NATIVE_RAW.json'
    raw = json.loads(raw_path.read_text())
    require(raw['passed'] and raw['scenarios'] == raw['intro_starts'] == raw['intro_completions'] == 3,
            'Three real playback scenarios did not pass: ' + raw.get('error', ''))
    require(raw['shot_mask'] == 31 and raw['actual_story_state'] and not raw['live_state_injected']
            and raw['ordinary_sdl_virtual_controller'] and raw['checkpoint_fixture_resets'] == 2,
            'Native workflow bypassed expected source/input paths')
    require(raw['maximum_player_root_delta_m'] < 1e-4 and raw['maximum_player_yaw_delta_rad'] < 1e-4,
            'Cinematic accepted gameplay capsule/look movement')
    common = {'fresh-interaction-held-input-controls-lock', 'completed-seen-no-quest-credit',
              'save-reload-once-only', 'ordinary-six-line-conversation-credit-only'}
    required = {case + ':' + check for case in ('natural', 'skip-keyboard', 'skip-controller') for check in common}
    required |= {'natural:' + check for check in ('pause-menu-freezes-clock', 'focus-loss-freezes-clock', 'resume-requires-neutral')}
    require(set(raw['checks']) == required, 'Missing/duplicate native checks')
    rows = [json.loads(line) for line in (D / 'NPC_INTRO_FRAMES.jsonl').read_text().splitlines()]
    require(rows and all(rows[i]['frame'] < rows[i+1]['frame'] for i in range(len(rows)-1)), 'Telemetry repeated/reordered a rendered frame')
    summaries = []
    for case in range(3):
        active = [row for row in rows if row['case'] == case and row['active']]
        complete = [row for row in rows if row['case'] == case and row['intro_completed_event']]
        require(active and len(complete) == 1, 'Missing real single intro-completion event')
        require(complete[0]['skipped_event'] is (case != 0), 'Natural/skip outcome differs from actual story event')
        require(all(row['spoken'] == 0 for row in active), 'Intro granted traveler credit')
        require(max(row['time'] for row in active) >= 35.9 if case == 0 else max(row['time'] for row in active) < 5,
                'Unexpected natural/skip timing')
        require(all(all(math.isfinite(x) for x in row['root'] + row['camera']) for row in active), 'Nonfinite camera or controller root')
        summaries.append({'case': case, 'rendered_intro_frames': len(active), 'maximum_observed_time': max(row['time'] for row in active),
                          'actual_skipped': complete[0]['skipped_event'], 'actual_completed_event_frame': complete[0]['frame'],
                          'shot_mask': sum(1 << shot for shot in {row['shot'] for row in active})})
    voice_rows = [row for row in rows if row['active']]
    require(all(row['media_mode'] == 1 and row['media_ready'] for row in voice_rows), 'Playback did not use the actual Voice media mode')
    require(max(row['media_audio_frames'] for row in voice_rows) > 48000, 'Actual PCM callback cursor never advanced')
    require(all(row['media_decoded_frames'] == 0 for row in voice_rows), 'Voice test unexpectedly used movie-frame decoding')
    require(any(row['subtitle'] == "Rowan: Easy. You're safe by the fire. Take a breath." for row in voice_rows), 'Supplied SRT caption did not reach the actual HUD')
    pause_audio = []
    for phase in (7, 10):
        tail = [row for row in rows if row['case'] == 0 and row['phase'] == phase][-8:]
        require(len(tail) == 8 and len({row['media_audio_frames'] for row in tail}) == 1,
                'MP3 voice cursor advanced while paused or unfocused')
        pause_audio.append({'phase': phase, 'stable_tail_frames': len(tail), 'audio_cursor': tail[0]['media_audio_frames']})
    outputs = {}
    for path in (log, raw_path, D / 'NPC_INTRO_FRAMES.jsonl', *sorted(D.glob('*.progress'))):
        pin(path, outputs)
    captures = []
    for path in sorted(D.glob('*.bmp')):
        with Image.open(path) as image:
            require(image.size == (1280, 720) and sum(ImageStat.Stat(image.convert('RGB')).stddev) > 12, 'Blank native capture')
            png = path.with_suffix('.png')
            image.save(png)
        pin(path, outputs)
        pin(png, outputs)
        captures.append(str(png))
    require(len(captures) == raw['captures'] and all((D / ('natural-shot-' + str(i) + '.png')).is_file() for i in range(5)),
            'Missing actual authored camera shot capture')
    film_paths = sorted((D / 'film').glob('frame-*.bmp'))
    require(len(film_paths) == raw['film_frames'] > 500, 'Natural actual-frame movie is incomplete')
    movie = D / 'NPC_INTRO_CONTINUOUS.mp4'
    subprocess.run(['/usr/bin/ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin', '-y',
                    '-framerate', '15', '-i', str(D / 'film/frame-%05d.bmp'), '-an', '-c:v', 'libx264',
                    '-threads', '2', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(movie)], check=True)
    pin(movie, outputs)
    # Keep all source readbacks as the actual continuous movie's evidence.
    for path in film_paths:
        pin(path, outputs)
    require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|NPC intro QA\] FAIL', log.read_text(), re.I),
            'Native rendering/workflow failure logged')
    # Every prepared source/shader/fixture/bank byte is retained through review.
    require(all(sha(path) == value for path, value in inputs.items()), 'Frozen input changed during playback')
    report = {'schema': 'valwood.npc-intro.mp3-voice-native-review.v1', 'passed': True,
              'inputs': inputs, 'outputs': outputs, 'raw': raw, 'scenarios': summaries,
              'command': command, 'captures': captures, 'continuous_movie': str(movie),
              'actual_mp3_input': True, 'synthetic_tone_not_actor_voice': True, 'supplied_subtitles_observed': True,
              'voice_media_mode': True, 'movie_decoding_frames': 0, 'paused_audio_tails': pause_audio,
              'audio_cursor_maximum': max(row['media_audio_frames'] for row in voice_rows),
              'gain_scope': 'Native default gain1; zero/mid/full gain and callback scaling are covered by the separate media CPU proof.',
              'wall_seconds': None if args.review_existing else time.monotonic() - started,
              'checkpoint_fixture': 'Production-encoded MeadowStart, then ordinary Continue/F8 and walking. Two explicit private-file reseeds between scenarios.',
              'limits': ['Software GL, dummy audio and a virtual controller do not certify hardware FPS, physical hearing or controller feel.',
                         'The source is an actual MP3 synthetic tone with authored story SRT; no actor recording or physical hearing is claimed.',
                         'Camera fallbacks are reported honestly; visual composition requires separate review.']}
    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print('PASS actual MP3 voice-over, live world shots and three real scenarios:', REPORT)


if __name__ == '__main__':
    main()
