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
REVIEW=BASE/'animation-review-v4'
ASSET=ROOT/'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
BINARY=BASE/'runtime-native-v1/native-bin/rpg'
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

ready=json.loads((BASE/'runtime-native-v1/READY.json').read_text())
if ready.get('ready') is not True or ready['sha256']!=sha(BINARY):
    raise RuntimeError('Native candidate has no current successful build receipt.')
for name,expected in ready['source_sha256'].items():
    if sha(BASE/'source-v1'/name)!=expected:raise RuntimeError('Native source changed after build.')
REVIEW.mkdir(exist_ok=True)
capture=REVIEW/'arena-native-v4_1.bmp'
log=REVIEW/'arena-native-v4_1.log'
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='4',MESA_DEBUG='1')
with log.open('w') as out:
    subprocess.run([str(BASE/'launch_knight_candidate.sh'),'--frames','24','--hidden','--width','1280','--height','720','--capture',str(capture)],env=env,stdout=out,stderr=subprocess.STDOUT,check=True)
text=log.read_text()
if f'CPU asset loaded: {ASSET}' not in text:raise RuntimeError('Actual game did not load the intended corrected asset.')
if 'Safe legacy fallback:' in text or 'Contact rejected:' in text:raise RuntimeError('Game fell back or rejected candidate contact.')
match=re.search(r'\[Skeletal hero candidate\] updates=(\d+) uploads=(\d+) draws=(\d+) attacks=(\d+) failures=(\d+).*foot_plants=(\d+) foot_releases=(\d+) foot_reach_releases=(\d+)',text)
if not match:raise RuntimeError('Game did not report actual candidate render/support counters.')
updates,uploads,draws,attacks,failures,plants,releases,reach=map(int,match.groups())
world=json.loads(capture.with_suffix(capture.suffix+'.world.json').read_text())
if updates!=24 or uploads!=24 or draws<=0 or failures or world['gl_error']:
    raise RuntimeError('Actual native game render accounting failed.')
result={'schema':'valwood.knight-locomotion.world-smoke.v1','accepted':True,
    'binary':str(BINARY),'binary_sha256':sha(BINARY),'asset':str(ASSET),'asset_sha256':sha(ASSET),
    'model_bank':str((BASE/'arena-project/models').resolve()),
    'updates':updates,'uploads':uploads,'hero_draw_passes':draws,'attacks':attacks,'hero_failures':failures,'gl_error':world['gl_error'],
    'foot_support':{'plants':plants,'releases':releases,'reach_releases':reach},
    'log':str(log),'log_sha256':sha(log),'capture':str(capture),'capture_sha256':sha(capture),
    'limits':['Bounded idle/setup game integration and rendering check; not combat or interactive gameplay acceptance.',
        'Offscreen software OpenGL; no hardware performance or controller-feel claim.']}
(REVIEW/'WORLD_SMOKE_ACCEPTED.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
