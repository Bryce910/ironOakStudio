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
Q = R / 'native-intro-ellis-camera'
P = Q / 'private-project'
D = Q / 'captures'
B = N / 'runtime-native-v1/native-bin/rpg'
READY = N / 'runtime-native-v1/READY.json'
REPORT = R / 'NPC_INTRO_ELLIS_CAMERA_NATIVE_REVIEW.json'
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
    # Explicitly versioned fixture changes only the copied authored spawn.
    # Production Save validation still requires this exact config pose.
    config=P/'opening.config'
    original=config.read_text()
    lines=original.splitlines(keepends=True)
    matches=[i for i,line in enumerate(lines) if line.startswith('meadow_spawn ')]
    require(len(matches)==1,'Private spawn config is ambiguous')
    i=matches[0];before=lines[i]
    lines[i]='meadow_spawn -11990.143414728496 11.803229331970215 5173.818987067658 0 -0.04\n'
    config.write_text(''.join(lines))
    (P/'PRIVATE_NEAR_ROWAN_FIXTURE.json').write_text(json.dumps({
        'schema':'valwood.npc-intro.near-rowan-preview-fixture.v1',
        'production_project_modified':False,'ordinary_production_checkpoint_parser':True,
        'purpose':'Single correctedEllis presentation preview; full movement/control proof is inherited from immutable5cdd run.',
        'only_story_configuration_delta':'opening.config meadow_spawn',
        'original_line':before,'fixture_line':lines[i],
        'proven_safe_pose_source':str(R/'native-intro-bounded/captures/NPC_INTRO_FRAMES.jsonl')})+'\n')
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



def audit_inherited_control(ready, inputs):
    archive_path=R/'control-proof-5cdd1c0f/ARCHIVE.json'
    archive=json.loads(archive_path.read_text())
    require(archive['passed'],'Historical archive failed')
    pin(archive_path,inputs)
    historical=Path(archive['original_review'])
    pin(historical,inputs,archive['original_review_sha256'])
    prior=json.loads(historical.read_text())
    require(prior['passed'] and prior['raw']['checks'] and prior['actual_core_skip_pause_reload_credit'],
            'Historical actual controls did not pass')
    overrides=archive['archived_overrides']
    for path,value in prior['inputs'].items():
        if path in overrides:
            saved=overrides[path]
            require(saved['sha256']==value,'Historical override is not the exact executed input')
            pin(saved['archived_path'],inputs,value)
        else:
            pin(path,inputs,value)
    for path,value in prior['outputs'].items():
        pin(path,inputs,value)
    # Preserve the original historical report, without recursively consuming
    # its obsolete absolute header/native hashes in the new readiness gate.
    old_ready=json.loads(Path(overrides[str(READY)]['archived_path']).read_text())
    before=old_ready['source_sha256'];after=ready['source_sha256']
    require(set(before)==set(after),'Source closure changed shape')
    changed={path for path in before if before[path]!=after[path]}
    expected={'engine/world_npc_intro.h','engine/world_npc_intro_qa.inc'}
    require(changed==expected,'Changes extend beyond onecamera and privateQA branch: '+str(changed))
    old_header=Path(overrides[str(S/'engine/world_npc_intro.h')]['archived_path']).read_text()
    expected_header=old_header.replace(
        '// Source yaw points along (sin(yaw),-cos(yaw)); side is the actor\'s right.',
        '// Resolve shot offsets in a stable yaw-relative X/Z basis.')
    expected_header=expected_header.replace(
        'case 3:eye0=detail::actor_point(ellis,.85,2.6,1.55);eye1=detail::actor_point(ellis,.72,2.45,1.52);',
        'case 3:eye0=detail::actor_point(ellis,-1.4,-2.4,1.65);eye1=detail::actor_point(ellis,-1.25,-2.3,1.62);')
    require((S/'engine/world_npc_intro.h').read_text()==expected_header,
            'Production header changes extend beyond case3eye offsets and acomment')
    require(sha(S/'engine/world_npc_intro_qa.inc')=='27bf212297ba34c5617da677f768fb8cf9e13c94679d4932584cf80f9988209c',
            'PrivateQA branch differs from frozen reviewed implementation')
    import difflib
    patch=''.join(difflib.unified_diff(
        Path(overrides[str(S/'engine/world_npc_intro_qa.inc')]['archived_path']).read_text().splitlines(keepends=True),
        (S/'engine/world_npc_intro_qa.inc').read_text().splitlines(keepends=True),
        fromfile='tested5cdd/engine/world_npc_intro_qa.inc',tofile='current/engine/world_npc_intro_qa.inc'))
    patch_path=R/'NPC_INTRO_SHOT3_QA_SOURCE.diff';patch_path.write_text(patch)
    pin(patch_path,inputs)
    audit={'schema':'valwood.npc-intro.camera-only-source-diff.v1','passed':True,
           'inputs':{str(historical):sha(historical),str(archive_path):sha(archive_path),
                     str(READY):sha(READY),str(S/'engine/world_npc_intro.h'):sha(S/'engine/world_npc_intro.h'),
                     str(S/'engine/world_npc_intro_qa.inc'):sha(S/'engine/world_npc_intro_qa.inc'),
                     str(patch_path):sha(patch_path)},
           'changed_source_files':sorted(changed),'unchanged_source_file_count':len(before)-len(changed),
           'production_change':'Case3eye offsets and comment only; target/title/State/media/input/control/save code identical.',
           'qa_change':'Extra marked, bounded, optional shot3-only render preview; ordinary defaultQAworkflow retained.',
           'historical_control_inputs_exactly_verified':True,'historical_outputs_unchanged':True,
           'archive_overrides_verified':sorted(overrides),
           'does_not_represent_old_native_as_current':True}
    audit_path=R/'NPC_INTRO_CAMERA_SOURCE_DIFF_REVIEW.json'
    audit_path.write_text(json.dumps(audit,indent=2,sort_keys=True)+'\n')
    pin(audit_path,inputs)
    return prior,audit_path


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--review-existing',action='store_true')
    args=parser.parse_args()
    require(not(args.prepare_only and args.review_existing),'Choose one mode')
    R.mkdir(exist_ok=True);Q.mkdir(exist_ok=True)
    inputs={};pin(__file__,inputs);pin(R/'STORY_PROJECT_SNAPSHOT.json',inputs)
    prep_path=D/'PREPARATION.json'
    if prep_path.exists():
        prep=json.loads(prep_path.read_text())
        for path,value in prep['inputs'].items():pin(path,inputs,value)
    else:
        require(not args.review_existing,'Missing original nearRowan preparation')
        prep=prepare(inputs)
    pin(prep_path,inputs)
    if args.prepare_only:
        print('PASS explicit nearRowan production-encoded private preview checkpoint prepared; no native launched')
        return
    ready=json.loads(READY.read_text())
    require(ready.get('ready') is True,'Current native not ready')
    pin(READY,inputs);pin(B,inputs,ready['sha256'])
    for path,value in ready['source_sha256'].items():pin(S/path,inputs,value)
    prior,audit_path=audit_inherited_control(ready,inputs)
    env=dict(os.environ)
    for key in list(env):
        if key.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_','VALWOOD_CAPTURE_','VALWOOD_MODULAR_KNIGHT','VALWOOD_SKELETAL_')) or key.endswith('_SEQUENCE') or key in {'VALWOOD_COMBAT_ARENA','VALWOOD_SHOWCASE'}:
            env.pop(key,None)
    env.update(SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1',SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',
               LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',MALLOC_ARENA_MAX='2',MALLOC_TRIM_THRESHOLD_='131072',MESA_DEBUG='1',
               VALWOOD_PRESENTATION='1',VALWOOD_OPENING_PROLOGUE='1',VALWOOD_QA_NPC_INTRO='1',
               VALWOOD_QA_NPC_INTRO_DIR=str(D),VALWOOD_QA_NPC_INTRO_BOUNDED='1',VALWOOD_QA_NPC_INTRO_SHOT3_PREVIEW='1')
    command=[str(B),'--project',str(P),'--play','--frames','800','--hidden','--width','1280','--height','720',
             '--graphics-preset','performance','--auto-quality','off','--render-scale','0.70']
    log=D/'native.log';started=time.monotonic()
    if not args.review_existing:
        require(not log.exists() and not (D/'NPC_INTRO_NATIVE_RAW.json').exists(),'Previous preview attempt exists')
        _HASH_PASS.clear();require(all(sha(p)==v for p,v in inputs.items()),'Input changed before startup')
        with log.open('w') as out:subprocess.run(command,cwd=N.parents[1],env=env,stdout=out,stderr=subprocess.STDOUT,check=True)
    raw_path=D/'NPC_INTRO_NATIVE_RAW.json';raw=json.loads(raw_path.read_text())
    require(raw['passed'] and raw['shot3_only_preview'] and raw['intro_starts']==1 and raw['intro_completions']==0,
            'Single actual intro/purepreview workflow failed: '+raw.get('error',''))
    require(raw['visual_preview_shot_mask']==8 and raw['captures']==1 and not raw['actual_skipped'] and
            not raw['actual_natural_native_completion'] and raw['preview_does_not_advance_state'],
            'Preview falsely claimed actual completion or different shots')
    require(raw['checks']==['shot3-pure-presentation-preview-no-state-advance'] and raw['final_spoken_mask']==0 and
            not raw['weapon_unlocked'] and raw['maximum_player_root_delta_m']==0 and raw['maximum_player_yaw_delta_rad']==0,
            'Preview changed root/control/quest state')
    require(raw['camera_fallbacks']==0,'Corrected portrait camera is obstructed')
    rows=[json.loads(line) for line in (D/'NPC_INTRO_FRAMES.jsonl').read_text().splitlines()]
    require(rows and all(a['frame']<b['frame'] for a,b in zip(rows,rows[1:])),'Reordered telemetry')
    previews=[r for r in rows if r['visual_preview']]
    require(previews and all(r['active'] and not r['focused'] and r['preview_shot']==3 and
            r['display']==r['render_target']==[1280,720] and not r['opening_controls'] and not r['spoken'] for r in previews),
            'Corrected shot did not use paused native720p real scene')
    require(max(r['time'] for r in previews)-min(r['time'] for r in previews)<1e-6 and
            len({r['media_audio_frames'] for r in previews})==1,'Preview advanced State or audio')
    require(all(all(math.isfinite(v) for v in r['root']+r['camera']) for r in rows),'Nonfinite scene/camera')
    require(all(r['media_mode']==1 and r['media_ready'] and r['media_decoded_frames']==0 for r in previews),
            'Voice scene became a fullscreen movie')
    bmp=D/'visual-preview-shot-3.bmp';png=bmp.with_suffix('.png')
    with Image.open(bmp) as image:
        require(image.size==(1280,720) and sum(ImageStat.Stat(image.convert('RGB')).stddev)>12,'Blank/wrongsize portrait')
        image.save(png)
    outputs={}
    for path in [log,raw_path,D/'NPC_INTRO_FRAMES.jsonl',bmp,png]:pin(path,outputs)
    require(not re.search(r'GL error|GL_INVALID|shader (?:compile|link).*fail|NPC intro QA\] FAIL',log.read_text(),re.I),
            'Rendering/QA failure logged')
    cpu=N/'npc-intro-review/NPC_INTRO_CPU_REVIEW.json'
    require(json.loads(cpu.read_text())['passed'],'Final sourceCPU failed');pin(cpu,inputs)
    _HASH_PASS.clear();require(all(sha(p)==v for p,v in inputs.items()),'Frozen inherited/current inputs changed during preview')
    proof={'schema':'valwood.npc-intro.ellis-native-preview.v1','passed':True,'inputs':inputs,'outputs':outputs,
           'raw':raw,'command':command,'capture':str(png),'actual_original_scene':True,'render_only_preview':True,
           'synthetic_mp3_not_actor_voice':True,'full_natural_native_timeline':False,
           'wall_seconds':None if args.review_existing else time.monotonic()-started}
    REPORT.write_text(json.dumps(proof,indent=2,sort_keys=True)+'\n')
    final_inputs=dict(inputs);pin(REPORT,final_inputs)
    # All prioroutputs are immutable; use newshot3only for the final visual set.
    captures=[str(png) if Path(p).name=='visual-preview-shot-3.png' else p for p in prior['captures']]
    final={'schema':'valwood.npc-intro.final-native-composite.v1','passed':True,
           'inputs':final_inputs,'outputs':outputs,'historical_control_review':str(R/'NPC_INTRO_BOUNDED_NATIVE_REVIEW.json'),
           'historical_native_sha256':'5cdd1c0f53828c0b2431729e4e4e56a124ee18dfccd8eba197a2a7a138528124',
           'current_native_sha256':ready['sha256'],'current_shot3_review':str(REPORT),'source_diff_review':str(audit_path),
           'captures':captures,'actual_mp3_input':True,'synthetic_tone_not_actor_voice':True,
           'actual_core_skip_pause_reload_credit':True,'actual_full_natural_native_timeline':False,
           'natural36s_timeline_acceptance':'Separate final-source production CPU test, no full natural Mesa replay claimed.',
           'visual_previews_only':True,'all_five_authored_shots_reviewed':True,
           'control_inheritance':'Exact immutable5cdd inputs/outputs and archivedchangedartifacts verified; State/input/save/media code unchanged.',
           'current_source_delta':'Onlycase3eye offsets/comment and markedQA-onlypreviewbranch.',
           'native_audio_scope':'OwnedPCM callbacks verified in prioractualworkflow; dummy/softwarefixedstep resyncs recorded, no physical hearing/smoothness claim.',
           'prior_actual_maximum_media_resyncs':prior['maximum_actual_media_resyncs'],
           'prior_observed_media_drift_seconds':prior['maximum_observed_media_drift_seconds'],
           'no_recorded_actor_voice_or_lipsync_claim':True,'user_session_untouched':True,'no_windows_build':True}
    final_path=R/'NPC_INTRO_FINAL_NATIVE_REVIEW.json';final_path.write_text(json.dumps(final,indent=2,sort_keys=True)+'\n')
    print('PASS correctedEllis native720p portrait + exact archivedcontrol/sourceinheritance:',final_path)


if __name__=='__main__':
    main()
