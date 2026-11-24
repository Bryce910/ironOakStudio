#!/usr/bin/env python3
"""Replay the connected opening through ordinary SDL inputs in a private copy.

The existing apartment/TV/arrival workflow hands over after all three travelers.
This runner then verifies the loan, an actual loss/retry and coached sword win,
supplies, one reward, and a walked town checkpoint with a real save/restore.
"""
import argparse
import fcntl
import hashlib
import json
import math
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import time

from PIL import Image, ImageStat
from benchmark_graphics_settings import native_environment
from check_opening_prologue import (
    ARRIVAL_CAMP_CAPTURES, ARRIVAL_CAMP_CHECKS, GAME_START_CAPTURES,
    GAME_START_CHECKS, ROOM_LIGHT_CAPTURES, ROOM_LIGHT_CHECKS, cinematic_captures,
    game_start_story, home_cinematic, isolate_project, mutable_snapshot, write_qa_paths,
)
from check_playtest_slices import timings
from character_polish_assets import character_asset_record
from prepare_combat_arena import blood_asset_record

ROOT = Path(__file__).resolve().parents[1]
PATH_NAMES = ('loan', 'encounter', 'satchel', 'return', 'town')
CHECKS = {
    'opening_controls_and_camp_complete', 'fresh_loan_interaction',
    'loan_dialogue_held_confirm', 'loan_weapon_granted_once',
    'grounded_third_person_sword', 'journey_pause_freezes_clocks',
    'journey_resume_requires_neutral', 'ordinary_movement_reaches_coached_fight',
    'coached_fight_requires_fresh_interaction', 'real_unguarded_defeat',
    'retry_preserves_first_contacts_and_loan', 'matching_directional_block',
    'sword_body_contact', 'first_and_third_person_combat', 'coached_fight_real_win',
    'fresh_satchel_pickup', 'rowan_reward_once', 'repeat_rowan_does_not_duplicate_reward',
    'town_checkpoint_reached_on_foot', 'town_checkpoint_saved',
    'town_checkpoint_restored', 'all_journey_checkpoints_captured',
}
CAPTURES = {
    'journey-loan', 'journey-road', 'journey-coach', 'journey-defeat',
    'journey-retry', 'journey-block', 'journey-hit', 'journey-victory',
    'journey-satchel', 'journey-reward', 'journey-town', 'journey-restored',
}
CAPTURE_STAGES = {
    'journey-loan':'offer','journey-road':'road','journey-coach':'encounter',
    'journey-defeat':'encounter','journey-retry':'road','journey-block':'encounter',
    'journey-hit':'encounter','journey-victory':'recover_supplies',
    'journey-satchel':'return_camp','journey-reward':'town',
    'journey-town':'complete','journey-restored':'complete',
}
INHERITED_CHECKS = ((GAME_START_CHECKS | ARRIVAL_CAMP_CHECKS) - {
    'house_skip', 'meadow_walk', 'waystone_interaction', 'checkpoint_retry',
    'controller_interaction',
}) | {'house_cinematic_natural', 'house_cinematic_all_shots', 'house_cinematic_match_handoff'}


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def finite(value, minimum=0):
    return type(value) in (int, float) and math.isfinite(value) and value >= minimum


def authored_paths(project):
    path = Path(project)/'opening/first-journey.routes.json'
    if path.is_symlink() or not path.is_file() or not 32 <= path.stat().st_size <= 256*1024:
        raise ValueError('Missing bounded authored first-journey walking routes')
    data = json.loads(path.read_text())
    if not isinstance(data,dict) or data.get('schema') != 'valwood.first-journey-routes' or type(data.get('version')) is not int or data['version'] != 1:
        raise ValueError('Unsupported first-journey route schema')
    paths = data.get('paths')
    if not isinstance(paths, dict) or set(paths) != set(PATH_NAMES):
        raise ValueError('First-journey routes must contain exactly loan/encounter/satchel/return/town')
    total = 0
    for name in PATH_NAMES:
        points = paths[name]
        if not isinstance(points, list) or not 1 <= len(points) <= 512:
            raise ValueError('Authored walking route has invalid point count: '+name)
        total += len(points)
        previous = None
        for point in points:
            if (not isinstance(point, list) or len(point) != 3 or
                    any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1_000_000 for v in point)):
                raise ValueError('Authored walking route has invalid position: '+name)
            if previous is not None and math.hypot(point[0]-previous[0],point[2]-previous[2]) > 25:
                raise ValueError('Authored walking route skips more than25 metres: '+name)
            previous = point
    if total > 2048:
        raise ValueError('Combined first-journey route exceeds2048 points')
    return paths


def write_paths(project):
    paths = authored_paths(project)
    rows = ['VALWOOD_FIRST_JOURNEY_QA 1']
    for name in PATH_NAMES:
        rows.append(f'{name} {len(paths[name])}')
        rows.extend(' '.join(format(value,'.17g') for value in point) for point in paths[name])
    rows.append('end')
    path = Path(project)/'opening/first-journey.qa.walk'
    if path.is_symlink():
        raise ValueError('Sealed journey QA path cannot be a link')
    path.write_text('\n'.join(rows)+'\n')
    return sha(path)


def inherited_captures(cinematic, room_lights=False):
    names = (GAME_START_CAPTURES | ARRIVAL_CAMP_CAPTURES) - {'waystone-complete', 'house-establish'}
    names |= cinematic_captures(cinematic)
    if room_lights:
        names |= ROOM_LIGHT_CAPTURES
    return names


def qa_failures(world, cinematic, room_lights=False):
    if not isinstance(world, dict):
        return ['Missing journey world report']
    errors = []
    qa = world.get('first_journey_qa', {})
    if qa.get('enabled') is not True or qa.get('passed') is not True:
        errors.append('Connected first-journey SDL workflow incomplete')
    labels = qa.get('checks')
    if not isinstance(labels, list) or not all(type(v) is str for v in labels) or len(labels) != len(CHECKS) or set(labels) != CHECKS:
        errors.append('Missing or duplicate journey control/story checks')
    names = qa.get('capture_names')
    if not isinstance(names, list) or not all(type(v) is str for v in names) or len(names) != len(CAPTURES) or set(names) != CAPTURES or qa.get('captures') != len(CAPTURES):
        errors.append('Missing or duplicate actual journey captures')
    for key in ('qa_player_pose_writes','qa_story_state_writes','qa_health_writes','unexpected_walk_discontinuities'):
        if type(qa.get(key)) is not int or qa[key] != 0:
            errors.append('Journey QA bypassed ordinary gameplay: '+key)
    for key, minimum in {'events':50,'movement_frames':120,'walked_m':100,'loss_enemy_hits':3,
                         'win_hits':1,'retries':1,'restores':1}.items():
        if not finite(qa.get(key),minimum):
            errors.append('Missing actual journey evidence: '+key)
    if (not finite(qa.get('win_parries')) or not finite(qa.get('win_blocks')) or
            qa.get('win_parries',0)+qa.get('win_blocks',0) < 1):
        errors.append('No genuine matching directional weapon contact')
    journey = world.get('first_journey', {})
    if journey.get('enabled') is not True or journey.get('stage') != 'complete' or journey.get('camp_mask') != 7:
        errors.append('Journey did not reach the town checkpoint after all three travelers')
    for key in ('loaned','supplies','rewarded','complete'):
        if journey.get(key) is not True:
            errors.append('Connected journey progress missing: '+key)
    for key in ('loan_grants','reward_grants'):
        if type(qa.get(key)) is not int or qa[key] != 1:
            errors.append('Loan or reward was missing/duplicated: '+key)
    if qa.get('save_verified') is not True or qa.get('restore_verified') is not True:
        errors.append('Actual town checkpoint save/load was not verified')
    old = world.get('opening_prologue_qa', {})
    required = INHERITED_CHECKS | (ROOM_LIGHT_CHECKS if room_lights else set())
    if not isinstance(old.get('checks'),list) or not required.issubset(old['checks']):
        errors.append('Apartment/TV/three-traveler controls were not fully exercised before handoff')
    for key in ('qa_player_pose_writes','qa_story_state_writes','capture_failures'):
        if old.get(key) != 0:
            errors.append('Opening replay failed or bypassed ordinary inputs: '+key)
    opening = world.get('opening_prologue', {})
    if opening.get('arrivals') != 1 or world.get('opening_game_start', {}).get('new_games') != 1:
        errors.append('Apartment New Game did not transport exactly once')
    entry = world.get('opening_entry_qa', {})
    if (entry.get('enabled') is not True or entry.get('shot_count') != cinematic['shot_count'] or
            entry.get('shot_mask') != (1 << cinematic['shot_count'])-1 or entry.get('matched_handoff') is not True or
            not finite(entry.get('handoff_max_error')) or entry['handoff_max_error'] > .0001):
        errors.append('Natural apartment cinematic and playable camera handoff missing')
    for key in ('gl_error','remaining_texture_references'):
        if world.get(key) != 0:
            errors.append(key)
    if world.get('asset_streaming', {}).get('failed') != 0:
        errors.append('Asset streaming failed during the connected journey')
    return errors


def capture_failures(name, state):
    if not isinstance(state, dict) or state.get('name') != name or type(state.get('frame')) is not int or state['frame'] <= 0:
        return ['Invalid journey capture name/frame: '+name]
    errors = []
    if name not in CAPTURE_STAGES or state.get('journey_stage') != CAPTURE_STAGES[name]:
        errors.append('Journey checkpoint has the wrong stage: '+name)
    if state.get('camp_mask') != 7 or state.get('grounded') is not True:
        errors.append('Journey capture lacks completed camp or supported player: '+name)
    if name not in {'journey-satchel','journey-reward','journey-town','journey-restored'} and state.get('supplies') is not False:
        errors.append('Supplies were granted before their pickup: '+name)
    if name not in {'journey-reward','journey-town','journey-restored'} and state.get('rewarded') is not False:
        errors.append('Reward was granted before the return conversation: '+name)
    if name not in {'journey-town','journey-restored'} and state.get('complete') is not False:
        errors.append('Town checkpoint completed too early: '+name)
    if name == 'journey-loan' and state.get('loaned') is not False:
        errors.append('Loan was granted before completing its conversation')
    if name != 'journey-loan' and state.get('loaned') is not True:
        errors.append('Loaned sword missing: '+name)
    if name == 'journey-defeat' and (state.get('combat_state') != 5 or state.get('combat_health') != 0 or not finite(state.get('enemy_hits'),3)):
        errors.append('Real unguarded combat defeat missing')
    if name == 'journey-retry' and (state.get('combat_health') != 100 or state.get('loaned') is not True or state.get('supplies') is not False):
        errors.append('Ordinary retry did not preserve loan and restart the encounter')
    if name == 'journey-block':
        parries,blocks=state.get('parries'),state.get('blocks')
        if not finite(parries) or not finite(blocks) or parries+blocks<1:
            errors.append('Matching directional guard did not contact the weapon')
    if name == 'journey-hit' and (not finite(state.get('hits'),1) or not finite(state.get('enemy_health'))):
        errors.append('Real sword body contact missing')
    if name == 'journey-victory' and (state.get('combat_state') != 4 or state.get('enemy_health') != 0 or not finite(state.get('hits'),1)):
        errors.append('Actual sword victory missing')
    if name in {'journey-satchel','journey-reward','journey-town','journey-restored'} and state.get('supplies') is not True:
        errors.append('Recovered supplies missing: '+name)
    if name in {'journey-reward','journey-town','journey-restored'} and (state.get('rewarded') is not True or state.get('reward_grants') != 1):
        errors.append('One genuine Rowan reward missing: '+name)
    if name in {'journey-town','journey-restored'} and (state.get('complete') is not True or state.get('journey_stage') != 'complete'):
        errors.append('Town checkpoint incomplete: '+name)
    return errors


def fnv64(data):
    value=1469598103934665603
    for byte in data:value=((value^byte)*1099511628211)&((1<<64)-1)
    return value


def checkpoint_evidence(project):
    """Independently read the one atomic opening save and its quoted quest block."""
    project=Path(project);path=project/'saves/opening.progress';settings=project/'opening/first-journey.settings'
    if path.is_symlink() or not path.is_file() or not 32<=path.stat().st_size<=1024:
        raise ValueError('Missing bounded atomic opening checkpoint')
    if any((project/'saves').glob('opening.progress.tmp-*')):
        raise ValueError('Uncommitted opening checkpoint temporary remains')
    if settings.is_symlink() or not settings.is_file() or not 32<=settings.stat().st_size<=4096:
        raise ValueError('Missing bounded journey settings for checkpoint signature')
    config=shlex.split(settings.read_text())
    if config[:2]!=['VALWOOD_FIRST_JOURNEY','1']:raise ValueError('Invalid checkpoint settings version')
    cursor=2;values=[]
    try:
        for name in ('camp','rowan','bram','encounter','supplies','retry','town'):
            if config[cursor]!=name:raise ValueError('Missing checkpoint route position')
            values.extend(float(v) for v in config[cursor+1:cursor+4]);cursor+=4
        if config[cursor]!='radii':raise ValueError('Missing checkpoint route radii')
        values.extend(float(v) for v in config[cursor+1:cursor+5]);cursor+=5
        if config[cursor:]!=['end'] or len(values)!=25 or not all(math.isfinite(v) for v in values):raise ValueError('Invalid checkpoint route signature inputs')
        layout=fnv64(b''.join(struct.pack('<d',v) for v in values))
        tokens=shlex.split(path.read_text())
        if tokens[:2]!=['VALWOOD_OPENING','1'] or tokens.count('first_journey')!=1 or tokens[-2]!='first_journey':raise ValueError('Missing/duplicate embedded first-journey checkpoint')
        if tokens[tokens.index('camp_spoken')+1]!='7':raise ValueError('Saved camp contacts are incomplete')
        header,body=tokens[-1].split('\n',1);tag,version,checksum=header.split()
        if tag!='VALWOOD_FIRST_JOURNEY_PROGRESS' or version!='1' or int(checksum)!=fnv64(body.encode('ascii')):raise ValueError('Embedded first-journey checkpoint checksum mismatch')
        fields=body.split()
        expected=['layout',str(layout),'stage','7','camp','7','progress','1','1','1','1','1','reward','12','1','1','end']
        if fields!=expected:raise ValueError('Saved quest is incompatible, incomplete, or duplicates its reward ledger')
    except (IndexError,UnicodeError,TypeError) as error:raise ValueError('Malformed atomic opening checkpoint') from error
    return dict(path=str(path),sha256=sha(path),layout_signature=layout,stage='complete',camp_mask=7,
        weapon=True,won=True,supplies=True,rewarded=True,town=True,coins=12,food=1,reputation=1,
        atomic_embedded_checkpoint=True)


def isolate(source, target):
    inputs = isolate_project(source,target)
    inputs['opening/qa.walk'] = write_qa_paths(target)
    inputs['opening/first-journey.qa.walk'] = write_paths(target)
    for name in ('CHARACTER_POLISH.json',):
        path=source/name
        if path.is_symlink() or not path.is_file() or not 8 <= path.stat().st_size <= 1024*1024:
            raise ValueError('Missing bounded runtime marker: '+name)
        shutil.copyfile(path,target/name);inputs[name]=sha(path)
    for label, bank in [('blood_assets',blood_asset_record(source/'blood_assets')),
                        ('models/longsword_mocap',character_asset_record(source/'models/longsword_mocap'))]:
        if label=='blood_assets' and not (target/label).exists():
            (target/label).symlink_to(bank['bank'],target_is_directory=True)
        for name,digest in bank['files_sha256'].items():inputs[label+'/'+name]=digest
    return inputs


def source_snapshot(source):
    return dict(project=mutable_snapshot(source),shared_banks={
        name:str((source/name).resolve(strict=True)) for name in ('blood_assets','models/longsword_mocap')},
        character_marker=sha(source/'CHARACTER_POLISH.json'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--project',type=Path,default=ROOT/'build/journey-opening/candidate-v1/project')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--room-lights',action='store_true')
    parser.add_argument('--scale',type=float,default=.8)
    parser.add_argument('--timeout',type=int,default=1800)
    args=parser.parse_args()
    source=args.project.resolve(strict=True);binary=args.binary.resolve(strict=True);output=args.output.resolve()
    if args.project.is_symlink() or not source.is_dir() or not binary.is_file() or output==source or output.is_relative_to(source) or source.is_relative_to(output):
        parser.error('Supply a regular project, existing binary and separate fresh evidence directory')
    if not .5 <= args.scale <= 1 or not 120 <= args.timeout <= 2400:
        parser.error('Render scale must be0.5..1; timeout120..2400 seconds')
    if not game_start_story(source):parser.error('Connected journey requires the apartment TV/New Game opening')
    cinematic=home_cinematic(source)
    if not cinematic:parser.error('Connected journey requires the authored natural apartment cinematic')
    authored_paths(source)
    output.mkdir(parents=True,exist_ok=False);before=source_snapshot(source);binary_before=sha(binary)
    project=output/'project';inputs=isolate(source,project);captures=output/'captures';captures.mkdir()
    profile=output/'profile.json';final=output/'final.bmp'
    env,removed=native_environment();env.pop('VALWOOD_VILLAGE_LIGHTING',None)
    for key in ('VALWOOD_PLAYTEST_SLICE','VALWOOD_SHOWCASE','VALWOOD_CAPTURE_VIEW'):env.pop(key,None)
    env.update(VALWOOD_PRESENTATION='1',VALWOOD_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_PROLOGUE='1',
        VALWOOD_QA_OPENING_CINEMATIC='1',VALWOOD_QA_FIRST_JOURNEY='1',
        VALWOOD_QA_OPENING_CAPTURE_DIR=str(captures),VALWOOD_QA_FIRST_JOURNEY_CAPTURE_DIR=str(captures),
        SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    if args.room_lights:env['VALWOOD_QA_OPENING_ROOM_LIGHTS']='1'
    command=[str(binary),'--project',str(project),'--play','--scenic','city-meadow','--frames','60000',
        '--warmup','120','--width','1280','--height','720','--auto-quality','off','--render-scale',str(args.scale),
        '--profile',str(profile),'--capture',str(final)]
    (output/'command.json').write_text(json.dumps(dict(command=command,removed_environment=removed,
        environment={k:v for k,v in env.items() if k.startswith(('VALWOOD_','SDL_'))}),indent=2)+'\n')
    (output/'source-before.json').write_text(json.dumps(before,indent=2)+'\n')
    (output/'isolated-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
    started=time.monotonic()
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('rb') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        with (output/'native.log').open('x') as log:
            try:code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=args.timeout).returncode
            except subprocess.TimeoutExpired:code=-1
    world_path=Path(str(profile)+'.world.json');world=json.loads(world_path.read_text()) if world_path.is_file() else {}
    metrics=json.loads(profile.read_text()) if profile.is_file() else {};errors=qa_failures(world,cinematic,args.room_lights)
    if code!=0:errors.append('Native process exit '+str(code))
    renderer=str(metrics.get('renderer',''))
    if not renderer or any(name in renderer.lower() for name in ('llvmpipe','softpipe','software','swrast')):errors.append('Native GPU renderer missing')
    images={};expected=CAPTURES|inherited_captures(cinematic,args.room_lights)
    for name in sorted(expected|{'final'}):
        path=final if name=='final' else captures/(name+'.bmp')
        if not path.is_file():errors.append('Actual rendered capture missing: '+name);continue
        image=Image.open(path).convert('RGB');target=output/'final.png' if name=='final' else captures/(name+'.png')
        if image.size!=(1280,720) or max(ImageStat.Stat(image).var)<8:errors.append('Blank/invalid native image: '+name)
        image.save(target);images[name]=dict(path=str(target),sha256=sha(target))
        if name in CAPTURES:
            state_path=captures/(name+'.json')
            try:state=json.loads(state_path.read_text())
            except (OSError,ValueError):errors.append('Journey capture state missing: '+name)
            else:errors+=capture_failures(name,state);images[name].update(state=state,state_sha256=sha(state_path))
    checkpoint={}
    try:checkpoint=checkpoint_evidence(project)
    except (OSError,ValueError) as error:errors.append('Final checkpoint: '+str(error))
    unchanged=before==source_snapshot(source);inputs_unchanged=all((project/name).is_file() and sha(project/name)==digest for name,digest in inputs.items())
    binary_unchanged=sha(binary)==binary_before
    if not unchanged:errors.append('User/source project or saves changed')
    if not inputs_unchanged:errors.append('Isolated inputs or shared owned banks changed')
    if not binary_unchanged:errors.append('Runtime binary changed')
    result=dict(passed=not errors,failures=errors,exit=code,elapsed_seconds=round(time.monotonic()-started,2),
        source_project=str(source),binary=str(binary),binary_sha256=binary_before,renderer=renderer,
        first_journey=world.get('first_journey',{}),first_journey_qa=world.get('first_journey_qa',{}),
        opening_qa=world.get('opening_prologue_qa',{}),checkpoint=checkpoint,captures=images,timing=timings(profile.with_suffix('.json.csv')),
        source_state_unchanged=unchanged,isolated_inputs_unchanged=inputs_unchanged,binary_unchanged=binary_unchanged,
        scope='Real SDL virtual controller/keyboard, ordinary walking and combat; authored transport/save/retry only. No QA pose, quest, health or progress writes. Physical hardware and sustained whole-game performance are not certified.')
    (output/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=result['passed'],failures=errors,result=str(output/'results.json')),indent=2))
    return 0 if result['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
