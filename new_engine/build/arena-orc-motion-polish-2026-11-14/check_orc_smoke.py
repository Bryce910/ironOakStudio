#!/usr/bin/env python3
"""Actual private native Orc load/render checks; no interactive combat claim."""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from PIL import Image

STAGE=Path(__file__).resolve().parent
ROOT=STAGE.parents[1]
REVIEW=STAGE/'opponent-review'
PROJECT=REVIEW/'orc-capture-project'
BINARY=STAGE/'runtime-native-v1/native-bin/rpg'
READY=STAGE/'runtime-native-v1/READY.json'
MANIFEST=STAGE/'arena-project/arena_orcs/ORC_ASSETS.json'

def require(value,message):
    if not value:raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main(species='orc'):
    parser=argparse.ArgumentParser();parser.add_argument('--geometry-only',action='store_true');parser.add_argument('--review-existing-human',action='store_true');parser.add_argument('--resume-third',action='store_true');args=parser.parse_args()
    require(species in {'human','orc'},'Unknown native control species')
    require(species=='orc' and not args.geometry_only and not args.review_existing_human,'This textured child checks only strict actual Orc TP/FP; inherited Human/geometry evidence is unchanged')
    subprocess.run([sys.executable,str(STAGE/'check_inherited_arena.py'),'--check-only'],check=True)
    inherited_path=REVIEW/'INHERITED_ARENA_SOURCE_REVIEW.json';inherited=json.loads(inherited_path.read_text())
    require(not args.review_existing_human or species=='human','Existing-capture recovery is only for the completed Human control')
    expected_species='Orcs' if species=='orc' else 'Humans'
    PROJECT=REVIEW/(species+'-capture-project')
    if species=='human':args.geometry_only=not json.loads(MANIFEST.read_text()).get('original_textures_present',False)
    contract_path=REVIEW/('ORC_GEOMETRY_ONLY_REVIEW.json' if args.geometry_only else 'ORC_ASSET_CONTRACT_REVIEW.json')
    contract_command=[sys.executable,str(STAGE/'check_orc_assets.py'),'--manifest',str(MANIFEST),'--check-only']
    if args.geometry_only:contract_command.append('--geometry-only')
    subprocess.run(contract_command,check=True)
    assets=json.loads(contract_path.read_text())
    require(assets['passed'] and assets['geometry_only']==args.geometry_only,'Actual source geometry contract failed')
    if not args.geometry_only:require(assets['accepted'] and assets['purchased_albedo_present'],'Actual textured Orc assets are not accepted')
    ready=json.loads(READY.read_text());require(ready.get('ready') is True and ready['sha256']==sha(BINARY),'Native build is not current')
    for name,expected in ready['source_sha256'].items():require(sha(STAGE/'source-v1'/name)==expected,'Native source changed after build: '+name)
    hero_manifest=STAGE/'HERO_ASSET.json';hero=Path(json.loads(hero_manifest.read_text())['path'])
    loadouts=STAGE/'loadout-rigs/loadouts.manifest';tokens=shlex.split(loadouts.read_text())
    require(tokens[:2]==['VALWOOD_SKELETAL_LOADOUTS','1'] and len(tokens)==11,'Invalid current Human hero loadouts')
    paths=[Path(__file__).resolve(),STAGE/'check_orc_assets.py',STAGE/'check_inherited_arena.py',inherited_path,contract_path,READY,BINARY,hero_manifest,hero,loadouts,
        STAGE/'shaders/world_skeletal.vs.glsl',STAGE/'shaders/rig_first_person.fs.glsl',REVIEW/'prepare_capture_project.py',
        REVIEW/'write_capture_profile.cpp',REVIEW/'read_capture_profile.cpp',*[Path(tokens[i]) for i in (3,6,9)]]
    if args.review_existing_human or args.resume_third:paths.extend((REVIEW/(species.upper()+'_CAPTURE_PROJECT.json'),REVIEW/'write_capture_profile'))
    inputs={str(p.resolve()):sha(p) for p in paths};inputs.update(assets['inputs']);inputs.update(inherited['inputs'])
    if species=='human':inputs[str(STAGE/'check_human_smoke.py')]=sha(STAGE/'check_human_smoke.py')
    inspector=REVIEW/'read_capture_profile'
    compile_command=['/usr/bin/g++','-std=c++17','-O2','-I',str(STAGE/'source-v1'),str(REVIEW/'read_capture_profile.cpp'),'-o',str(inspector)]
    subprocess.run(compile_command,check=True)
    recovered_before=None
    if args.review_existing_human or args.resume_third:
        # A native process already completed. Recreate its
        # deterministic production fixture, and prove its bytes match the
        # pinned preparation receipt, rather than resetting the tested saves.
        preparation=json.loads((REVIEW/(species.upper()+'_CAPTURE_PROJECT.json')).read_text())
        for path,expected in preparation['dependencies'].items():require(sha(path)==expected,'Human fixture source changed')
        require(sha(preparation['writer'])==preparation['writer_sha256'],'Human fixture writer changed')
        with tempfile.TemporaryDirectory(prefix='profile-recovery-',dir=REVIEW) as temporary:
            fixture=Path(temporary);(fixture/'PRIVATE_CAPTURE_PROJECT.json').write_text('{}\n')
            subprocess.run([preparation['writer'],str(fixture),species],check=True,capture_output=True,text=True)
            require(sha(fixture/'saves/player.progress')==preparation['profile_sha256'],'Recreated Human fixture differs from the actual pre-run bytes')
            recovered_before=json.loads(subprocess.check_output([str(inspector),str(fixture)],text=True))
    views=[];outputs={str(inspector):sha(inspector)}
    env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',MESA_DEBUG='1')
    for key in list(env):
        if key.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_')) or key.endswith('_SEQUENCE') or key in {'VALWOOD_CAPTURE_VIEW','VALWOOD_OPENING_PROLOGUE','VALWOOD_SHOWCASE','VALWOOD_MODULAR_KNIGHT_THIRD_PERSON'}:env.pop(key,None)
    env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_CAPTURE_ARENA_START='1',
        VALWOOD_MODULAR_KNIGHT=str(hero),VALWOOD_MODULAR_KNIGHT_SHADER=str(STAGE/'shaders/world_skeletal.vs.glsl'),
        VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(STAGE/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(loadouts))
    # Same ordinary instance lock as the player's launcher. Never interrupt an
    # existing editor/game; these isolated profiles are the only save target.
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        for view in ('third','first'):
            reuse=args.review_existing_human or (args.resume_third and view=='third')
            if not reuse:subprocess.run([sys.executable,str(REVIEW/'prepare_capture_project.py'),'--species',species],check=True)
            require((PROJECT/'arena_orcs').resolve()==MANIFEST.parent.resolve(),'Capture project points to a different Orc bank')
            profile_before=recovered_before if reuse else json.loads(subprocess.check_output([str(inspector),str(PROJECT)],text=True))
            require(profile_before['selected_species']==expected_species and profile_before['active_run_id']==0 and profile_before['next_run_id']==1 and profile_before['balance']==0,'Private starting profile is not fresh')
            capture_tag='arena-opponents-human' if species=='human' else 'arena-orcs-prototype-native' if args.geometry_only else 'arena-orcs-native'
            capture=REVIEW/f'{capture_tag}-{view}.bmp';log=REVIEW/f'{capture_tag}-{view}.log'
            current_env=dict(env)
            if view=='first':current_env['VALWOOD_CAPTURE_VIEW']='first'
            else:current_env['VALWOOD_MODULAR_KNIGHT_THIRD_PERSON']='1'
            command=[str(BINARY),'--project',str(PROJECT),'--play','--scenic','city-walk','--frames','24','--hidden','--width','1280','--height','720','--capture',str(capture)]
            if not reuse:
                with log.open('w') as stream:subprocess.run(command,cwd=ROOT,env=current_env,stdout=stream,stderr=subprocess.STDOUT,check=True)
            text=log.read_text();world_path=capture.with_suffix(capture.suffix+'.world.json');world=json.loads(world_path.read_text())
            bank=world['arena_opponents']
            require(bank['species']==expected_species and bank['orcs_ready'] and bank['body_only'],'Actual game failed to keep selected species while loading the real Orc bank')
            require(bank.get('original_textures')==bool(assets.get('original_textures_present',False)),'Native texture-pending status disagrees with inspected actual assets')
            require(bank['body_poses']==172 and bank['arm_poses']==0 and bank['measured_pivots']==172 and bank['certified_weapon_pairs']==217,'Actual Orc bank certification/ownership differs from contract')
            measured=assets['metadata']['measured_profile']
            for field,expected in [('body_radius_m',measured['body_radius_m']),('body_height_m',measured['body_height_m']),('walk_stride_m',measured['walk_stride_m'])]:
                require(abs(bank.get(field,float('nan'))-round(expected,3))<1e-6,'Actual measured Orc sizing/stride not applied at native JSON precision: '+field)
            if species=='orc':require(bank['actors']>0 and bank['uploads']>0,'No real Orc opponent was uploaded')
            else:require(bank['actors']==0,'Inactive Orc bank presented live actors during Human control')
            require(world['gl_error']==0,'Native game reported OpenGL error')
            require(f'CPU asset loaded: {hero}' in text and '[skeletal-loadouts] loaded4 purchased rigs Sword/Shield/Axe/Staff' in text,'Human player rig changed while selecting Orc defenders')
            require(f'[Arena] Run 1: {expected_species} / 8 total / 1 concurrent' in text,'Capture did not call ordinary species-aware arena_begin')
            require(not any(s in text for s in ('Safe legacy fallback:','Transition failed:','Contact rejected:')),'Native candidate fell back/rejected its rig')
            hero_counts=re.search(r'\[Skeletal hero candidate\] updates=(\d+) uploads=(\d+) draws=(\d+) attacks=(\d+) failures=(\d+)',text)
            require(hero_counts is not None,'Missing actual Human player counters')
            updates,uploads,draws,attacks,failures=map(int,hero_counts.groups())
            require(updates>=24 and uploads>=24 and (draws>0 if view=='third' else draws==0) and failures==0,'Human hero render counters failed for selected view')
            first_counts=re.search(r'\[Skeletal first person\] draws=(\d+) failures=(\d+)',text)
            require(first_counts is not None,'Missing actual first-person accounting')
            fp_draws,fp_failures=map(int,first_counts.groups())
            require(fp_failures==0 and (fp_draws>=12 if view=='first' else fp_draws==0),'Wrong view or first-person failure')
            old_snapshot=REVIEW/f'{capture_tag}-{view}.profile.json'
            profile_after=json.loads(old_snapshot.read_text())['after'] if reuse and old_snapshot.is_file() else json.loads(subprocess.check_output([str(inspector),str(PROJECT)],text=True))
            require(profile_after['active_species']==expected_species and profile_after['selected_species']==expected_species and profile_after['active_run_id']==1 and profile_after['next_run_id']==2,'Actual arena run persisted wrong race/receipt')
            require(profile_after['balance']==profile_after['wins']==profile_after['opponents']==0,'Bounded rendering capture awarded coins')
            variant_draws=None
            if species=='human':
                variant=re.search(r'\[Arena knight variants\] loaded=(\d+) mask=(\d+) body=([0-9,]+) crowd=([0-9,]+) pieces=([0-9,]+) support=([0-9,]+)',text)
                require(variant is not None and tuple(map(int,variant.groups()[:2]))==(1,15),'Purchased Human opponent variants were lost')
                variant_draws={name:list(map(int,value.split(','))) for name,value in zip(('body','crowd','pieces','support'),variant.groups()[2:])}
                require(all(len(v)==4 for v in variant_draws.values()) and sum(variant_draws['body'])+sum(variant_draws['crowd'])>0,'No actual Human variant opponent draw')
                require((PROJECT/'models').resolve()==(STAGE/'pose-cook/bank/models').resolve(),'Human bank differs from accepted purchased variants')
            png=capture.with_suffix('.png');Image.open(capture).save(png)
            snapshot=REVIEW/f'{capture_tag}-{view}.profile.json';snapshot.write_text(json.dumps({'before':profile_before,'after':profile_after},indent=2)+'\n')
            for output in (log,capture,png,world_path,snapshot):outputs[str(output)]=sha(output)
            views.append({'view':view,'command':command,'arena_opponents':bank,'variants':variant_draws,'hero_draw_passes':draws,'hero_failures':failures,'first_person_draws':fp_draws,'first_person_failures':fp_failures,'gl_error':world['gl_error'],'saved_profile':profile_after,'capture':str(png)})
    require(inputs=={p:sha(Path(p)) for p in inputs},'Native/source/asset inputs changed during captures')
    result={'schema':'valwood.power-orc.native-load-render-review.v1' if species=='orc' else 'valwood.arena-opponents-human.world-smoke.v1','passed':True,'accepted':True,'species':expected_species,'actual_orc_load_render_acceptance':species=='orc',
        'geometry_only_prototype':args.geometry_only,'original_texture_contract_accepted':not args.geometry_only,'visual_quality_accepted':False,'public_release_ready':False,
        'actual_interactive_combat_acceptance':False,'binary':str(BINARY),'binary_sha256':sha(BINARY),'views':views,'inputs':inputs,'outputs':outputs,
        'profile_inspector_compile':compile_command,'private_profiles_only':True,'limits':['Offscreen software OpenGL with no SDL input focus; these are bounded load/render checks, not controller feel or interactive combat acceptance.',
        'No victory was simulated or awarded. Actual combat geometry, severing, animation review and performance remain separate checks.']}
    result['reviewed_existing_human_captures']=args.review_existing_human
    result['reviewed_completed_third_capture_without_rerender']=args.resume_third
    result['native_profile_telemetry_precision_decimals']=3
    result['bounded_frames_per_view']=24
    result['new_owned_source_geometry']=assets['source_name']
    output=REVIEW/('HUMAN_WORLD_SMOKE_ACCEPTED.json' if species=='human' else 'ORC_NATIVE_GEOMETRY_RENDER_REVIEW.json' if args.geometry_only else 'ORC_NATIVE_LOAD_RENDER_REVIEW.json')
    if species=='human':
        result.update(real_orc_bank_loaded_during_human_control=True,human_body_profile_m=[.34,1.8],variants={'loaded':True,'available_mask':15},loadouts=4,first_person={'draws':views[1]['first_person_draws'],'failures':0,'gl_error':0},hero_failures=0,gl_error=0,actual_orc_asset_acceptance=False,asset=str(hero),asset_sha256=sha(hero),model_bank=str((PROJECT/'models').resolve()),dependencies_sha256=inputs)
    if args.geometry_only:result['limits'].append('The installed actual Orc bank uses provisional authored materials; its original purchased textures and visual acceptance are pending. No public ready gate.')
    output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'passed':True,'species':expected_species,'views':[v['view'] for v in views],'geometry_only_prototype':args.geometry_only,'actual_orc_load_render_acceptance':species=='orc','actual_interactive_combat_acceptance':False,'visual_quality_accepted':False,'public_release_ready':False}))

if __name__=='__main__':main()
