#!/usr/bin/env python3
"""Capture the actual Orc setup UI without bypassing its real focus gate."""
from pathlib import Path
import fcntl
import hashlib
import json
import os
import subprocess
import sys
from PIL import Image

BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[1]
REVIEW=BASE/'opponent-review'
PROJECT=REVIEW/'orc-capture-project'
MANIFEST=BASE/'arena-project/arena_orcs/ORC_ASSETS.json'
BINARY=BASE/'runtime-native-v1/native-bin/rpg'

def require(value,message):
    if not value:raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    material=json.loads(MANIFEST.read_text());geometry_only=not material.get('original_textures_present',False)
    check=[sys.executable,str(BASE/'check_orc_assets.py'),'--manifest',str(MANIFEST),'--check-only']
    if geometry_only:check.append('--geometry-only')
    subprocess.run(check,check=True)
    contract_path=REVIEW/('ORC_GEOMETRY_ONLY_REVIEW.json' if geometry_only else 'ORC_ASSET_CONTRACT_REVIEW.json')
    contract=json.loads(contract_path.read_text());ready_path=BASE/'runtime-native-v1/READY.json';ready=json.loads(ready_path.read_text())
    require(ready.get('ready') is True and ready['sha256']==sha(BINARY),'Native build is stale')
    for name,value in ready['source_sha256'].items():require(sha(BASE/'source-v1'/name)==value,'Source changed after native build: '+name)
    hero_manifest=BASE/'HERO_ASSET.json';hero=Path(json.loads(hero_manifest.read_text())['path'])
    files=[Path(__file__).resolve(),BASE/'check_orc_assets.py',contract_path,ready_path,BINARY,hero_manifest,hero,
        REVIEW/'prepare_capture_project.py',REVIEW/'write_capture_profile.cpp',REVIEW/'read_capture_profile.cpp',
        BASE/'shaders/world_skeletal.vs.glsl',BASE/'shaders/rig_first_person.fs.glsl',BASE/'loadout-rigs/loadouts.manifest']
    inputs={str(p.resolve()):sha(p) for p in files};inputs.update(contract['inputs'])
    inspector=REVIEW/'read_capture_profile'
    subprocess.run(['/usr/bin/g++','-std=c++17','-O2','-I',str(BASE/'source-v1'),str(REVIEW/'read_capture_profile.cpp'),'-o',str(inspector)],check=True)
    env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',MESA_DEBUG='1')
    for key in list(env):
        if key.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_')) or key.endswith('_SEQUENCE') or key in {'VALWOOD_CAPTURE_VIEW','VALWOOD_CAPTURE_ARENA_START','VALWOOD_OPENING_PROLOGUE','VALWOOD_SHOWCASE'}:env.pop(key,None)
    env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_MODULAR_KNIGHT=str(hero),VALWOOD_MODULAR_KNIGHT_THIRD_PERSON='1',
        VALWOOD_MODULAR_KNIGHT_SHADER=str(BASE/'shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(BASE/'shaders/rig_first_person.fs.glsl'),
        VALWOOD_SKELETAL_LOADOUTS=str(BASE/'loadout-rigs/loadouts.manifest'))
    capture=REVIEW/'arena-orcs-actual-setup.bmp';log=REVIEW/'arena-orcs-actual-setup.log'
    with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        subprocess.run([sys.executable,str(REVIEW/'prepare_capture_project.py'),'--species','orc'],check=True)
        before=json.loads(subprocess.check_output([str(inspector),str(PROJECT)],text=True))
        require(before['selected_species']=='Orcs' and before['active_run_id']==0 and before['next_run_id']==1,'Setup profile is not fresh')
        command=[str(BINARY),'--project',str(PROJECT),'--play','--scenic','city-walk','--frames','24','--hidden','--width','1280','--height','720','--capture',str(capture)]
        with log.open('w') as stream:subprocess.run(command,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT,check=True)
        after=json.loads(subprocess.check_output([str(inspector),str(PROJECT)],text=True))
    require(after==before,'Setup-only capture changed the profile or allocated a run')
    text=log.read_text();world_path=capture.with_suffix(capture.suffix+'.world.json');world=json.loads(world_path.read_text());bank=world['arena_opponents']
    require('[Arena] Setup ready: 8 Orcs / 1 concurrent, Orc assets ready' in text,'Actual setup did not load selected Orc choice')
    require('[Arena] Run ' not in text,'Setup-only capture unexpectedly started combat')
    require(bank['orcs_ready'] and bank['body_only'] and bank['body_poses']==172 and bank['arm_poses']==0,'Actual purchased Orc bank unavailable in setup')
    require(bank['species']=='Humans' and bank['actors']==0,'An uncommitted setup draft changed the live opponent bank')
    require(bank.get('original_textures')==bool(material.get('original_textures_present',False)),'Setup texture-pending status is incorrect')
    require(world['gl_error']==0 and 'Safe legacy fallback:' not in text and 'Contact rejected:' not in text,'Actual setup render failed')
    png=capture.with_suffix('.png');Image.open(capture).save(png)
    profile=REVIEW/'arena-orcs-actual-setup.profile.json';profile.write_text(json.dumps({'before':before,'after':after},indent=2)+'\n')
    require(inputs=={p:sha(Path(p)) for p in inputs},'Setup source or bank changed during capture')
    result={'schema':'valwood.power-orc.actual-setup-render-review.v1','passed':True,'actual_setup_screenshot_captured':True,'selected_species':'Orcs',
        'actual_bank_available':True,'profile_unchanged':True,'run_allocated':False,'original_textures':bank.get('original_textures'),
        'controls_enabled_in_capture':False,'interactive_ui_acceptance':False,'focus_gate_bypassed':False,
        'focus_note':'The actual offscreen SDL window has no input focus/device. Controls remain disabled by the ordinary focus/neutral gate; asset availability is independent of that gate.',
        'visual_review':{'accepted':False,'status':'Manual screenshot inspection still required'},'capture':str(png),'command':command,'inputs':inputs,
        'outputs':{str(p):sha(p) for p in (log,capture,png,world_path,profile)},
        'limits':['Original purchased Orc texture maps remain pending when original_textures is false.','No physical input/controller or enabled-control claim from the offscreen screenshot.']}
    (REVIEW/'ORC_SETUP_RENDER_REVIEW.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'passed':True,'capture':str(png),'profile_unchanged':True,'focus_gate_bypassed':False,'controls_enabled_in_capture':False}))

if __name__=='__main__':main()
