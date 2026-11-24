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
Q = R / 'native-intro-bounded'
P = Q / 'private-project'
D = Q / 'captures'
B = N / 'runtime-native-v1/native-bin/rpg'
READY = N / 'runtime-native-v1/READY.json'
REPORT = R / 'NPC_INTRO_BOUNDED_NATIVE_REVIEW.json'
BANKS = {'models', 'assets', 'ground', 'surfaces', 'audio', 'environments', 'particles',
         'scenery', 'fonts', 'texture_blocks', 'collision', 'licenses', 'blood_assets'}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


# Reuse a hash only for aliases of the same unchanged physical file during
# one validation pass. Clearing before/after native means no cross-run trust.
_HASH_PASS = {}
def sha(path):
    path=Path(path)
    st=path.stat()
    key=(st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns)
    if key not in _HASH_PASS:
        with path.open('rb') as stream:
            value=hashlib.file_digest(stream,'sha256').hexdigest()
        end=path.stat()
        require((end.st_dev,end.st_ino,end.st_size,end.st_mtime_ns,end.st_ctime_ns)==key,'File changed while hashing: '+str(path))
        _HASH_PASS[key]=value
    return _HASH_PASS[key]


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
    env.update(SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1', SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', LIBGL_ALWAYS_SOFTWARE='1',
               LP_NUM_THREADS='2', MALLOC_ARENA_MAX='2', MALLOC_TRIM_THRESHOLD_='131072', MESA_DEBUG='1',
               VALWOOD_PRESENTATION='1', VALWOOD_OPENING_PROLOGUE='1', VALWOOD_QA_NPC_INTRO='1',
               VALWOOD_QA_NPC_INTRO_DIR=str(D), VALWOOD_QA_NPC_INTRO_BOUNDED='1')
    command = [str(B), '--project', str(P), '--play', '--frames', '1800', '--hidden',
               '--width', '1280', '--height', '720', '--graphics-preset', 'performance',
               '--auto-quality', 'off', '--render-scale', '0.70']
    started = time.monotonic()
    log = D / 'native.log'
    if not args.review_existing:
        require(not log.exists() and not (D / 'NPC_INTRO_NATIVE_RAW.json').exists(), 'Previous native attempt exists')
        _HASH_PASS.clear()
        require(all(sha(path) == value for path, value in inputs.items()), 'Input changed before native startup')
        with log.open('w') as output:
            subprocess.run(command, cwd=N.parents[1], env=env, stdout=output, stderr=subprocess.STDOUT, check=True)
    raw_path=D/'NPC_INTRO_NATIVE_RAW.json'
    raw=json.loads(raw_path.read_text())
    require(raw['passed'] and raw['intro_starts']==raw['intro_completions']==1 and raw['actual_skipped'],
            'Bounded actual control workflow failed: '+raw.get('error',''))
    require(raw['visual_preview_shot_mask']==31 and not raw['actual_natural_native_completion'] and raw['preview_does_not_advance_state'],
            'Pure previews were misrepresented as actual natural playback')
    require(raw['final_spoken_mask']==1 and not raw['weapon_unlocked'] and raw['actual_story_state'] and not raw['live_state_injected'],
            'Dialogue/quest credit differs from actual expected progression')
    required={'fresh-interaction-held-input-controls-lock','pause-menu-freezes-clock','focus-loss-freezes-clock',
              'five-pure-presentation-previews-no-state-advance','resume-requires-neutral',
              'actual-controller-skip-completed-seen-no-credit','save-reload-once-only',
              'ordinary-six-line-conversation-credit-only'}
    require(set(raw['checks'])==required and len(raw['checks'])==8,'Missing/duplicate bounded checks')
    rows=[json.loads(line) for line in (D/'NPC_INTRO_FRAMES.jsonl').read_text().splitlines()]
    require(rows and all(a['frame']<b['frame'] for a,b in zip(rows,rows[1:])),'Repeated/reordered actual rendered rows')
    active=[r for r in rows if r['active']]
    require(active and all(r['spoken']==0 and not r['opening_controls'] for r in active),'Intro granted credit/gameplay controls')
    require(raw['maximum_player_root_delta_m']<1e-4 and raw['maximum_player_yaw_delta_rad']<1e-4,'Intro moved playable capsule/look')
    require(all(all(math.isfinite(v) for v in r['root']+r['camera']) for r in rows),'Nonfinite rendered camera/player')
    previews=[r for r in rows if r['visual_preview']]
    require({r['preview_shot'] for r in previews}==set(range(5)),'Missing actual presentation preview')
    require(all(not r['focused'] and r['active'] and r['render_target']==r['display']==[1280,720] for r in previews),
            'Preview was not paused/native720p')
    require(max(r['time'] for r in previews)-min(r['time'] for r in previews)<1e-6,'Preview advanced actual core timeline')
    complete=[r for r in rows if r['intro_completed_event']]
    require(len(complete)==1 and complete[0]['skipped_event'] and not complete[0]['visual_preview'],
            'Actual skip completion event missing/duplicated/faked')
    require(all(r['media_mode']==1 and r['media_ready'] and r['media_channel']>=0 and r['media_decoded_frames']==0 for r in active),
            'Actual owned Voice audio unavailable or movie-mode frames decoded')
    require(max(r['media_audio_frames'] for r in active)>48000 and max(r['media_callbacks'] for r in active)>0,
            'Actual PCM callback/cursor did not advance')
    require(any(r['subtitle']=="Rowan: Easy. You're safe by the fire. Take a breath." for r in active),'Supplied actual SRT caption missing')
    for phase in (7,10):
        tail=[r for r in rows if r['phase']==phase][-6:]
        require(len(tail)==6 and len({r['media_audio_frames'] for r in tail})==1,'Pause/focus advanced actual audio cursor')
    expected={'bounded-before','bounded-paused-menu','bounded-completed','bounded-reloaded-dialogue'}|{'visual-preview-shot-'+str(i) for i in range(5)}
    require({Path(p).stem for p in raw['capture_paths']}==expected and raw['captures']==9,'Missing bounded/native-preview stills')
    outputs={};captures=[]
    for path in sorted(D.glob('*.bmp')):
        with Image.open(path) as image:
            size=(1280,720) if path.stem.startswith('visual-preview-') else (640,360)
            require(image.size==size and sum(ImageStat.Stat(image.convert('RGB')).stddev)>12,'Blank/wrongsize actual still')
            png=path.with_suffix('.png');image.save(png)
        pin(path,outputs);pin(png,outputs);captures.append(str(png))
    for path in [log,raw_path,D/'NPC_INTRO_FRAMES.jsonl',D/'actual-completed.progress',P/'saves/opening.progress']:
        pin(path,outputs)
    require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|NPC intro QA\] FAIL',log.read_text(),re.I),
            'Native rendering/workflow failure logged')
    cpu=N/'npc-intro-review/NPC_INTRO_CPU_REVIEW.json';media_cpu=N/'media-review/NPC_INTRO_MEDIA_CPU_REVIEW.json'
    for receipt in [cpu,media_cpu,R/'STORY_CONFIG_CPU_REVIEW.json',R/'STORY_PROJECT_REPAIR_REVIEW.json']:
        require(json.loads(receipt.read_text())['passed'],'Required CPU/data preflight failed')
        pin(receipt,inputs)
    _HASH_PASS.clear()
    require(all(sha(path)==value for path,value in inputs.items()),'Frozen inputs changed during actual playback')
    report={'schema':'valwood.npc-intro.bounded-native-review.v1','passed':True,'inputs':inputs,'outputs':outputs,
            'raw':raw,'command':command,'captures':captures,'actual_mp3_input':True,'synthetic_tone_not_actor_voice':True,
            'actual_core_skip_pause_reload_credit':True,'actual_full_natural_native_timeline':False,
            'natural36s_timeline_acceptance':'Separate production CPU state test; no full natural Mesa replay claimed.',
            'visual_previews_only':True,'preview_sample_times_seconds':sorted({r['preview_time'] for r in previews}),
            'maximum_actual_intro_time':max(r['time'] for r in active),'maximum_actual_media_resyncs':max(r['media_resyncs'] for r in active),
            'maximum_observed_media_drift_seconds':max(r['media_maximum_drift_seconds'] for r in active),
            'native_pcm_callbacks':max(r['media_callbacks'] for r in active),'native_owned_audio_channel':active[0]['media_channel'],
            'movie_decoder_frames':0,'wall_seconds':None if args.review_existing else time.monotonic()-started,
            'render_scope':'640x360 real controls; five pure authored shot samples rendered1280x720 while actual timeline/audio paused.',
            'hash_policy':'Every path exactSHA checked. Same physical unchanged hardlinks share hash only within each pass, caches cleared pre/post native.',
            'limits':['Software GL, dummy audio and tagged virtual controller do not certify hardwareFPS, hearing, real-time audio smoothness or physical feel.',
                      'The five highres images are explicitly pure presentation previews; they do not claim native natural36s chronology.',
                      'Voice uses an actual MP3 synthetic tone and suppliedSRT, not a recorded actor performance.']}
    REPORT.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print('PASS bounded actual Voice controls/save/once/credit +five pure shotpreviews:',REPORT)

if __name__=='__main__':main()
