#!/usr/bin/env python3
"""Check actual private game loading/rendering separately from animation previews."""
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[1]
REVIEW=BASE/'rig-fit-review'
ASSET=Path(json.loads((BASE/'HERO_ASSET.json').read_text())['path'])
BINARY=BASE/'runtime-native-v1/native-bin/rpg'
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

ready=json.loads((BASE/'runtime-native-v1/READY.json').read_text())
if ready.get('ready') is not True or ready['sha256']!=sha(BINARY):
    raise RuntimeError('Native candidate has no current successful build receipt.')
for name,expected in ready['source_sha256'].items():
    if sha(BASE/'source-v1'/name)!=expected:raise RuntimeError('Native source changed after build.')
# Snapshot the exact four rigs and host inputs before either actual game run.
import shlex
manifest=BASE/'loadout-rigs/loadouts.manifest'
tokens=shlex.split(manifest.read_text())
if tokens[:2]!=['VALWOOD_SKELETAL_LOADOUTS','1'] or len(tokens)!=11:raise RuntimeError('Invalid loadout manifest.')
rig_assets=[ASSET]+[Path(tokens[i]) for i in (3,6,9)]
input_paths=[Path(__file__).resolve(),BINARY,BASE/'runtime-native-v1/READY.json',BASE/'HERO_ASSET.json',
    manifest,BASE/'launch_knight_candidate.sh',BASE/'shaders/world_skeletal.vs.glsl',
    BASE/'shaders/rig_first_person.fs.glsl',*rig_assets]
inputs_before={str(p):sha(p) for p in input_paths}
REVIEW.mkdir(exist_ok=True)
capture=REVIEW/'arena-rig-fit.bmp'
log=REVIEW/'arena-rig-fit.log'
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',MESA_DEBUG='1')
with log.open('w') as out:
    subprocess.run([str(BASE/'launch_knight_candidate.sh'),'--frames','24','--hidden','--width','1280','--height','720','--capture',str(capture)],env=env,stdout=out,stderr=subprocess.STDOUT,check=True)
text=log.read_text()
if f'[Skeletal shader] {BASE / "shaders/world_skeletal.vs.glsl"}' not in text:raise RuntimeError('Actual game did not load the private candidate shader.')
if f'CPU asset loaded: {ASSET}' not in text:raise RuntimeError('Actual game did not load the intended corrected asset.')
if 'Safe legacy fallback:' in text or 'Contact rejected:' in text:raise RuntimeError('Game fell back or rejected candidate contact.')
match=re.search(r'\[Skeletal hero candidate\] updates=(\d+) uploads=(\d+) draws=(\d+) attacks=(\d+) failures=(\d+).*foot_plants=(\d+) foot_releases=(\d+) foot_reach_releases=(\d+)',text)
if not match:raise RuntimeError('Game did not report actual candidate render/support counters.')
updates,uploads,draws,attacks,failures,plants,releases,reach=map(int,match.groups())
world=json.loads(capture.with_suffix(capture.suffix+'.world.json').read_text())
variant=re.search(r'\[Arena knight variants\] loaded=(\d+) mask=(\d+) body=([0-9,]+) crowd=([0-9,]+) pieces=([0-9,]+) support=([0-9,]+)',text)
if not variant or tuple(map(int,variant.groups()[:2]))!=(1,15):
    raise RuntimeError('Actual game did not enable all four purchased opponent variants.')
variant_draws={name:list(map(int,value.split(','))) for name,value in zip(('body','crowd','pieces','support'),variant.groups()[2:])}
if any(len(v)!=4 for v in variant_draws.values()):raise RuntimeError('Invalid variant accounting.')
if (BASE/'arena-project/models').resolve()!=(BASE/'pose-cook/bank/models').resolve():
    raise RuntimeError('Actual game project still uses a previous opponent bank.')
if '[skeletal-loadouts] loaded4 purchased rigs Sword/Shield/Axe/Staff' not in text:raise RuntimeError('Missing purchased weapon rig family.')
if updates not in (24,25) or uploads!=24 or draws<=0 or failures or world['gl_error']:
    raise RuntimeError('Actual native game render accounting failed.')
# Exercise the actual host's camera-local arms/FBO path separately. The native
# launch uses the identical private project and settings; only the capture view
# changes. Hold the same session lock without touching any active game.
import fcntl
first_log=REVIEW/'arena-rig-fit-first-person.log'
first_capture=REVIEW/'arena-rig-fit-first-person.bmp'
first_env=dict(env)
for key in list(first_env):
    if key.startswith(('VALWOOD_QA_','VALWOOD_TEST_','VALWOOD_PLAYTEST_','VALWOOD_DISABLE_')) or key.endswith('_SEQUENCE'):
        first_env.pop(key)
first_env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',
    VALWOOD_MODULAR_KNIGHT=str(ASSET),VALWOOD_CAPTURE_VIEW='first',
    VALWOOD_MODULAR_KNIGHT_SHADER=str(BASE/'shaders/world_skeletal.vs.glsl'),
    VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(BASE/'shaders/rig_first_person.fs.glsl'),
    VALWOOD_SKELETAL_LOADOUTS=str(BASE/'loadout-rigs/loadouts.manifest'))
for key in ('VALWOOD_MODULAR_KNIGHT_THIRD_PERSON','VALWOOD_OPENING_PROLOGUE','VALWOOD_SHOWCASE'):
    first_env.pop(key,None)
with (ROOT/'projects/seldonne/.valwood-instance.lock').open('a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    with first_log.open('w') as out:
        subprocess.run([str(BINARY),'--project',str(BASE/'arena-project'),'--play','--scenic','city-walk',
            '--frames','60','--hidden','--width','1280','--height','720','--capture',str(first_capture)],
            env=first_env,cwd=ROOT,stdout=out,stderr=subprocess.STDOUT,check=True)
first_text=first_log.read_text()
first_match=re.search(r'\[Skeletal first person\] draws=(\d+) failures=(\d+)',first_text)
if not first_match:raise RuntimeError('No native first-person accounting.')
first_draws,first_failures=map(int,first_match.groups())
first_world=json.loads(first_capture.with_suffix(first_capture.suffix+'.world.json').read_text())
if first_draws<12 or first_failures or first_world['gl_error'] or any(q in first_text for q in ('Fallback:', 'Transition failed:', 'Safe legacy fallback:', 'Contact rejected:')):
    raise RuntimeError('Native first-person rendering failed.')
if inputs_before!={p:sha(Path(p)) for p in inputs_before}:raise RuntimeError('Integration source/asset changed during game checks.')
result={'schema':'valwood.arena-rig-fit.world-smoke.v1','accepted':True,
    'binary':str(BINARY),'binary_sha256':sha(BINARY),'asset':str(ASSET),'asset_sha256':sha(ASSET),
    'model_bank':str((BASE/'arena-project/models').resolve()),
    'variants':{'loaded':True,'available_mask':15,'draw_counters':variant_draws},
    'loadouts':4,'first_person':{'draws':first_draws,'failures':first_failures,'gl_error':first_world['gl_error']},
    'updates':updates,'uploads':uploads,'hero_draw_passes':draws,'attacks':attacks,'hero_failures':failures,'gl_error':world['gl_error'],
    'foot_support':{'plants':plants,'releases':releases,'reach_releases':reach},
    'log':str(log),'log_sha256':sha(log),'capture':str(capture),'capture_sha256':sha(capture),
    'limits':['Bounded idle/setup game integration and rendering check; not combat or interactive gameplay acceptance.',
        'Offscreen software OpenGL; no hardware performance or controller-feel claim.']}
result['dependencies_sha256']={str(p):sha(p) for p in [Path(__file__).resolve(),BASE/'launch_knight_candidate.sh',BASE/'HERO_ASSET.json',BASE/'runtime-native-v1/READY.json',BASE/'PLAYER_STREAM_CORRESPONDENCE.json',BASE/'pose-cook/POSE_BANK_READY.json',BASE/'shaders/world_skeletal.vs.glsl',BASE/'shaders/hero_preview.fs.glsl',BASE/'shaders/rig_first_person.fs.glsl',BASE/'loadout-rigs/loadouts.manifest',first_log,first_capture,first_capture.with_suffix(first_capture.suffix+'.world.json'),capture.with_suffix(capture.suffix+'.world.json')]}
result['dependencies_sha256'].update(inputs_before)
(REVIEW/'WORLD_SMOKE_ACCEPTED.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
