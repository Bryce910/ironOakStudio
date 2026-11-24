#!/usr/bin/env python3
"""Bounded neutral GPU review of the frozen private locomotion candidate."""
import concurrent.futures
import hashlib
import json
import os
import subprocess
from pathlib import Path

BASE=Path(__file__).resolve().parent
ROOT=BASE.parents[1]
REVIEW=BASE/'animation-review-v4'
ASSET=ROOT/'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
BINARY=BASE/'controller-preview/preview_knight_controller'
SCENARIOS={'locomotion':360,'combat':330,'combat-moving':330,'combat-running':330,
           'guard':270,'guard-switch':270,'run-entries':450,'reaction':240,'steering':300,
           'production-speeds':420,'stop-restart':360,'turn-in-place':270,'slope':240,'uneven-ground':240,'terminal-hit':390,'reversal':270}

def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def freeze():
    return {str(p):sha(p) for p in (ASSET,BINARY,BASE/'preview_knight_controller.cpp',
        BASE/'source-v1/engine/world_skeletal_hero.cpp',BASE/'source-v1/engine/world_skeletal_hero.h',
        BASE/'source-v1/engine/world_character_terminal.h',BASE/'source-v1/engine/world_training_combat.cpp')}

build=json.loads((BASE/'controller-preview/BUILD_INPUTS.json').read_text())
if build['binary_sha256']!=sha(BINARY) or any(sha(Path(p))!=expected for p,expected in build['sha256'].items()):
    raise RuntimeError('Preview binary no longer matches the current source; rebuild it before capturing.')
before=freeze()
REVIEW.mkdir(exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='3',MESA_DEBUG='1')
def render(item):
    name,frames=item;folder=REVIEW/name
    if folder.exists() and any(folder.glob('frame-*.png')):
        raise RuntimeError(f'{folder} already contains captures; preserve them or choose a fresh review directory.')
    folder.mkdir(exist_ok=True)
    command=[str(BINARY),str(ASSET),'--scenario',name,'--frames',str(frames),'--fps','30',
             '--width','832','--height','720','--yaw','27','--pitch','7','--capture-dir',str(folder)]
    with (REVIEW/f'{name}.log').open('w') as log:
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(f'{name} failed; inspect its capture log.')
    print(f'{name}: {frames} frames captured',flush=True)
    return name

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(render,SCENARIOS.items()))
if freeze()!=before:raise RuntimeError('Candidate changed during capture; these frames cannot certify the final candidate.')
(REVIEW/'CAPTURE_INPUTS.json').write_text(json.dumps({'sha256':before,'frames':SCENARIOS,'offscreen_software_rendering':True},indent=2)+'\n')
subprocess.run(['python3',str(BASE/'finalize_animation_previews.py')],check=True)
if freeze()!=before:
    (REVIEW/'PREVIEW_RECEIPT.json').unlink(missing_ok=True)
    raise RuntimeError('Candidate changed while encoding; no final preview receipt can be issued.')
