#!/usr/bin/env python3
"""Matched, isolated forest views; preparation never starts the game.

Run each stage sequentially while the normal game is closed. Static native
captures test composition/readiness, not traversal, swimming or animation.
All measured profiler intervals remain in the pacing report. Saved automatic
quality is retained by default; --freeze-quality is an explicit private-only
override, recorded in the receipt, useful for strictly matched A/B images.
"""
from __future__ import annotations
import argparse
import fcntl
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import time

sys.dont_write_bytecode = True
N = Path(__file__).resolve().parents[1]
B = N.with_name('camp-reference-2026-11-19')
E = N.parents[1]


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def camera(eye, target):
    dx, dy, dz = (b-a for a,b in zip(eye,target))
    return dict(x=eye[0],y=eye[1],z=eye[2],yaw=math.atan2(dx,-dz),
                pitch=math.atan2(dy,math.hypot(dx,dz)),fov=math.pi/3)


VIEWS = {
    'camp-eye': camera((-11982,13.5500001907,5180),(-11988,12.4,5172)),
    'camp-west-creek': camera((-12000,13.55,5180),(-12013,10.0,5166)),
    'creek-surface': camera((-12013,12.1,5178),(-12018,10.4,5164)),
    'village-road': camera((-11946,14.01,5265),(-11930,13.0,5328)),
}


def load_configuration(generation: Path):
    spec = importlib.util.spec_from_file_location('forest_'+generation.name.replace('-','_'),generation/'launch.py')
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module.private_configuration


def scene_inputs(origin: Path) -> dict[str,str]:
    result = {}
    for name in ('world.studio','project.valwood','city.physics','city.scene','city.sky',
                 'showcase.views','showcase.flames','opening.config','opening.scene',
                 'CHARACTER_POLISH.json','SCENERY_POLISH.json','collision/structures/catalog'):
        p=origin/name
        if p.is_file(): result[str(p)] = sha(p)
    for folder in ('scenery','surfaces','opening','road-traffic'):
        for p in sorted((origin/folder).rglob('*')):
            if p.is_file() and p.suffix in ('.txt','.settings','.physics','.scene','.config','.walk','.manifest'):
                result[str(p)] = sha(p)
    # Bind actual optical image resources and baked movement caches, including
    # new art additions. These can change without changing an alias/stem name.
    def resource(name):
        relative=Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Escaping scene resource: '+name)
        p=origin/relative
        if not p.is_file(): raise ValueError('Missing declared scene resource: '+str(p))
        result[str(p)]=sha(p)
    for line in (origin/'world.studio').read_text().splitlines():
        row=shlex.split(line)
        if row and row[0]=='environment' and len(row)>1:resource(row[1])
        if len(row)==2 and row[0]=='surfaces':
            image=shlex.split(row[1])
            if len(image)>2 and image[0]=='image':resource(image[2])
    for line in (origin/'collision/structures/catalog').read_text().splitlines():
        row=shlex.split(line)
        if len(row)==4 and row[0]=='mesh':
            resource(row[2]);resource(row[3])
    return result


def verify_pins(pins):
    return all(Path(p).is_file() and sha(Path(p)) == h for p,h in pins.items())


def settings_for(a):
    record = json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())
    text = record['text']
    if a.freeze_quality:
        text = '\n'.join('auto_quality=0' if row.startswith('auto_quality=') else row
                         for row in text.splitlines())+'\n'
    if a.water_quality:
        rows=[row for row in text.splitlines() if not row.startswith('water_quality=')]
        rows=['version=6' if row.startswith('version=') else row for row in rows]
        text='\n'.join([*rows,'water_quality='+a.water_quality])+'\n'
    return record,text


def prepare(a, stage, view):
    generation=N if stage=='candidate' else B
    if json.loads((N/'BASELINE.json').read_text())['baseline']!=str(B): raise ValueError('Unexpected matched baseline')
    output=N/'review/forest-views'/a.name/stage/view
    output.mkdir(parents=True,exist_ok=True)
    if (output/'EXECUTION.json').exists():
        raise ValueError('Retain previous evidence; choose a new --name: '+str(output))
    private=Path('/tmp')/('valwood-camp-atmosphere-review-'+a.name+'-'+stage+'-'+view)
    old=output/'PREPARATION.json'
    if old.exists():
        prep=json.loads(old.read_text())
        if not verify_pins(prep['input_sha256']): raise ValueError('Prepared inputs changed; choose fresh --name')
        if prep['frames']!=a.frames or prep['warmup']!=a.warmup or prep['quality_frozen']!=a.freeze_quality:
            raise ValueError('Prepared options changed; choose fresh --name')
        if prep.get('requested_water_quality')!=a.water_quality or prep.get('river_scissor_disabled_for_comparison')!=a.disable_river_scissor:
            raise ValueError('Prepared quality/scissor options changed; choose fresh --name')
        return output,private,prep
    if private.exists(): raise ValueError('Private directory already exists without receipt: '+str(private))
    private.mkdir()
    command,env,origin=load_configuration(generation)('world')
    origin=Path(origin)
    for p in origin.iterdir():
        if p.name in ('saves','settings','studio-layout.ini','world.studio.workspace','shaders') or '.recovery' in p.name:
            continue
        dest=private/p.name
        if p.is_dir():dest.symlink_to(p.resolve(),target_is_directory=True)
        else:shutil.copy2(p,dest,follow_symlinks=True)
    (private/'shaders').symlink_to((generation/'shared/shaders').resolve(),target_is_directory=True)
    (private/'saves').mkdir()
    (private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
    settings,text=settings_for(a)
    (private/'graphics.settings').write_text(text)
    cam=VIEWS[view]
    # project_runtime.c explicitly accepts city-camera-02..11. Custom names
    # are rejected before showcase.views is read. This slot is not one of
    # apply_source_view's four walking-height overrides, so authored eye Y is
    # retained exactly. Each private project contains just this matching row.
    scenic='city-camera-02'
    (private/'showcase.views').write_text(scenic+' '+' '.join(format(cam[k],'.15g') for k in ('x','y','z','yaw','pitch','fov'))+'\n')
    # Bounded static first-person camera, no benchmark sweep or normal inputs.
    # Only the private showcase.views selects an eye; production poses untouched.
    command[command.index('--project')+1]=str(private)
    command[command.index('--scenic')+1]=scenic
    command[command.index('--width')+1]='1600'
    command[command.index('--height')+1]='900'
    command+=['--no-hud','--frames',str(a.frames),'--warmup',str(a.warmup),
              '--capture',str(output/'result.bmp'),'--profile',str(output/'performance.json')]
    env.update(VALWOOD_CAPTURE_VIEW='first',VALWOOD_QA_FIXED_TIME='1',
               MESA_DEBUG='1',SDL_AUDIODRIVER='dummy',
               SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='0',
               SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
    # Phase labels retain actual loading/capture intervals, not a blind FPS cut.
    env['VALWOOD_PROFILE_PHASES']='1'
    if a.disable_river_scissor:env['VALWOOD_QA_DISABLE_RIVER_REFLECTION_SCISSOR']='1'
    pins=scene_inputs(origin)
    # Pin the actual model files exposed by this scene's asset declarations,
    # including inherited leaf overlays and any new reviewed art variants.
    for line in (origin/'world.studio').read_text().splitlines():
        if not line.startswith('asset '): continue
        row=shlex.split(line)
        if len(row)<3: raise ValueError('Malformed asset declaration')
        relative=Path(row[2])
        if relative.is_absolute() or '..' in relative.parts: raise ValueError('Escaping model declaration')
        stem=origin/'models'/relative
        for p in sorted(stem.parent.glob(stem.name+'*.glb')):
            if p.is_file(): pins[str(p)]=sha(p)
    pins[str(Path(__file__).resolve())]=sha(Path(__file__).resolve())
    pins[str(N/'BASELINE.json')]=sha(N/'BASELINE.json')
    pins[str(generation/'runtime/rpg')]=sha(generation/'runtime/rpg')
    pins[str(generation/'launch.py')]=sha(generation/'launch.py')
    for p in sorted((generation/'shared/shaders').rglob('*')):
        if p.is_file():pins[str(p)]=sha(p)
    pins[str(N/'review/ACTIVE_SETTINGS.json')]=sha(N/'review/ACTIVE_SETTINGS.json')
    private_pins={str(private/p):sha(private/p) for p in ('world.studio','graphics.settings','showcase.views')}
    prep=dict(schema='valwood.forest-matched-views.preparation.v1',stage=stage,view=view,
              generation=str(generation),binary_sha256=sha(generation/'runtime/rpg'),
              camera=cam,resolution=[1600,900],frames=a.frames,warmup=a.warmup,
              private_project=str(private),command=command,
              environment={k:v for k,v in env.items() if k.startswith('VALWOOD_') or k in ('MESA_DEBUG','SDL_AUDIODRIVER')},
              input_sha256=pins,private_input_sha256=private_pins,
              saved_settings_source=settings.get('source'),saved_settings_sha256=hashlib.sha256(settings['text'].encode()).hexdigest(),
              actual_settings_sha256=sha(private/'graphics.settings'),quality_frozen=a.freeze_quality,
              requested_water_quality=a.water_quality,river_scissor_disabled_for_comparison=a.disable_river_scissor,
              fixed_visual_time=True,ordinary_gameplay=False,game_started=False,
              scope='Static full-resolution composition and residency; no traversal, swimming, animation or audio quality claim.')
    old.write_text(json.dumps(prep,indent=2)+'\n')
    return output,private,prep


def execute(a,stage,view):
    output,private,prep=prepare(a,stage,view)
    if a.prepare_only:
        print(json.dumps(dict(stage=stage,view=view,prepared=str(output/'PREPARATION.json'))),flush=True)
        return True
    if not verify_pins(prep['private_input_sha256']):raise ValueError('Prepared private scene changed')
    generation=Path(prep['generation'])
    _,env,_=load_configuration(generation)('world')
    env.update(prep['environment'])
    lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
    try:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        start=time.monotonic();timeout=False
        with (output/'native.log').open('w') as log:
            try:
                result=subprocess.run(prep['command'],cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=a.timeout)
                code=result.returncode
            except subprocess.TimeoutExpired:
                timeout=True;code=124
    finally:os.close(lock)
    profile=output/'performance.json'
    worldpath=Path(str(profile)+'.world.json')
    world=json.loads(worldpath.read_text()) if worldpath.exists() else {}
    perf=json.loads(profile.read_text()) if profile.exists() else {}
    logtext=(output/'native.log').read_text(errors='replace')
    final=world.get('camera_final',{})
    cam_ok=all(isinstance(final.get(k),(int,float)) and abs(final[k]-prep['camera'][k])<1e-4 for k in ('x','y','z','yaw','pitch'))
    stream=world.get('asset_streaming',{})
    settled=stream.get('critical_pending')==0 and stream.get('loading')==0 and stream.get('failed')==0
    capture=output/'result.bmp';image_ok=False
    if capture.is_file():
        from PIL import Image
        with Image.open(capture) as image:
            image_ok=image.size==(1600,900)
            image.save(output/'result.png')
    from review_frame_pacing import build_report
    pacing=build_report(output)
    (output/'FRAME_PACING_REVIEW.json').write_text(json.dumps(pacing,indent=2)+'\n')
    unchanged=verify_pins(prep['input_sha256']) and verify_pins(prep['private_input_sha256'])
    passed=(not timeout and code==0 and world.get('gl_error')==0 and 'GL_INVALID' not in logtext
            and 'Surface library rejected:' not in logtext and 'Safe legacy fallback:' not in logtext
            and cam_ok and settled and image_ok and perf.get('samples',0)>=a.frames-a.warmup-2 and unchanged)
    receipt=dict(schema='valwood.forest-matched-views.execution.v1',passed=passed,
                 stage=stage,view=view,binary_sha256=prep['binary_sha256'],
                 preparation_sha256=sha(output/'PREPARATION.json'),elapsed_seconds=time.monotonic()-start,
                 exit_code=code,timed_out=timeout,input_unchanged=unchanged,
                 exact_camera_checked=cam_ok,assets_settled_at_capture=settled,
                 asset_streaming=stream,resolution_checked=image_ok,
                 water_quality=world.get('water_quality'),river_reflection_scissor=world.get('river_reflection_scissor'),
                 camera_final=final,graphics=world.get('graphics'),authored_weather=world.get('authored_weather'),
                 gl_error=world.get('gl_error'),profile_samples=perf.get('samples'),
                 software_renderer=env.get('LIBGL_ALWAYS_SOFTWARE')=='1',
                 ordinary_gameplay=False,audio_quality_certified=False,aaa_quality_certified=False,
                 fixed_visual_time=True,quality_frozen=a.freeze_quality,
                 frame_pacing_report_sha256=sha(output/'FRAME_PACING_REVIEW.json'),
                 limitations=['Final residency is checked, but capture is at a bounded frame rather than a renderer readiness callback.',
                              'Retained all measured profiler intervals; static-view timings do not certify normal playable route pacing.',
                              'Automatic quality is retained unless explicitly frozen; inspect effective graphics before claiming matched quality.',
                              'First-person static eye excludes hero body and does not exercise creek traversal, swimming, collisions or NPC conversation.'])
    (output/'EXECUTION.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt),flush=True)
    return passed


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--stage',choices=('baseline','candidate','both'),default='both')
    ap.add_argument('--view',choices=(*VIEWS,'all'),default='all')
    ap.add_argument('--name',required=True)
    ap.add_argument('--frames',type=int,default=600)
    ap.add_argument('--warmup',type=int,default=120)
    ap.add_argument('--timeout',type=float,default=300)
    ap.add_argument('--prepare-only',action='store_true')
    ap.add_argument('--freeze-quality',action='store_true')
    ap.add_argument('--water-quality',choices=('Low','Medium','High','Ultra'))
    ap.add_argument('--disable-river-scissor',action='store_true')
    a=ap.parse_args()
    if a.disable_river_scissor and a.stage!='candidate':ap.error('Scissor overrides require candidate-only testing')
    if not a.name.replace('-','').isalnum():ap.error('Use a simple alphanumeric/hyphen review name')
    if not 60<=a.warmup<a.frames<=3600:ap.error('Require 60 <= warmup < frames <= 3600')
    stages=('baseline','candidate') if a.stage=='both' else (a.stage,)
    views=tuple(VIEWS) if a.view=='all' else (a.view,)
    ok=True
    for stage in stages:
        for view in views:
            result=execute(a,stage,view)
            ok=result and ok
            if not result:raise SystemExit(1)
    raise SystemExit(0 if ok else 1)


if __name__=='__main__':main()
