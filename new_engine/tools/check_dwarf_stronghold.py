#!/usr/bin/env python3
"""Native Grandstone route/render/snow verification with isolated progress."""
from pathlib import Path
import argparse,hashlib,json,math,os,subprocess,tempfile
from PIL import Image
from benchmark_graphics_settings import native_environment,MemoryMonitor
ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_stronghold'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--walk',action='store_true');p.add_argument('--drifts',action='store_true');p.add_argument('--ice',action='store_true');p.add_argument('--walk-frames',type=int);p.add_argument('--view',choices=['all','approach','gate','drifts','snow','waterfall','trees','hall','forge','throne'],default='all');a=p.parse_args()
    metadata=json.loads((PROJECT/'stronghold.json').read_text());route=metadata['route']
    minimum=640 if a.ice else 3600 if a.drifts else math.ceil(sum(math.dist(x,y)for x,y in zip(route,route[1:]))/3.5*60)+360
    if a.walk_frames is None:a.walk_frames=minimum+240
    if a.walk_frames<minimum:p.error('--walk-frames must allow the complete walking route')
    if (a.ice or a.drifts)and not a.walk:p.error('--ice and --drifts require --walk')
    if a.ice and a.drifts:p.error('Choose one route')
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    protected=[PROJECT/'world.studio',PROJECT/'graphics.settings',ROOT/'projects/valwood_visual/world.studio',*sorted((ROOT/'projects/valwood_visual/saves').glob('*'))]
    before={str(f):sha(f)for f in protected if f.is_file()};runs=[]
    views=[('ice-walk' if a.ice else 'drift-walk' if a.drifts else 'walk','stronghold')]if a.walk else[(name,scene)for name,scene in [('approach','stronghold'),('gate','stronghold-gate'),('drifts','stronghold-drifts'),('snow','stronghold-snow'),('waterfall','stronghold-waterfall'),('trees','stronghold-trees'),('hall','stronghold-hall'),('forge','stronghold-forge'),('throne','stronghold-throne')]if a.view in ['all',name]]
    with tempfile.TemporaryDirectory(prefix='grandstone-qa-')as temp:
        for name,scene in views:
            env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'),VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_CAPTURE_VIEW='first')
            profile=out/(name+'.json');capture=out/(name+'.bmp')
            cmd=[str(ROOT/'rpg_grandstone'),'--project',str(PROJECT),'--play','--scenic',scene,'--no-hud','--frames',str(a.walk_frames)if a.walk else'280','--width','1600','--height','900','--warmup','120','--auto-quality','off','--render-scale','.7'if a.walk else'1','--profile',str(profile),'--capture',str(capture)]
            if a.walk:cmd+=['--benchmark','--view','river'];env['VALWOOD_QA_STRONGHOLD_SNOW']='1'
            if a.drifts:env['VALWOOD_QA_STRONGHOLD_DRIFTS']='1'
            if a.ice:env['VALWOOD_QA_STRONGHOLD_ICE']='1'
            monitor=MemoryMonitor();monitor.start()
            with(out/(name+'.log')).open('w')as log:
                try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1200 if a.walk else 240).returncode
                except subprocess.TimeoutExpired:code=-1
            monitor.stop()
            if capture.exists():Image.open(capture).convert('RGB').save(out/(name+'.png'));capture.unlink()
            wp=Path(str(profile)+'.world.json');world=json.loads(wp.read_text())if wp.exists()else{};perf=json.loads(profile.read_text())if profile.exists()else{}
            s=world.get('stronghold',{});snow=world.get('powder_snow',{});route=world.get('benchmark_route',{});renderer=perf.get('renderer','')
            ok=code==0 and 'AMD'in renderer and world.get('gl_error')==0 and world.get('remaining_texture_references')==0 and s.get('actors')==len(metadata['actors']) and s.get('families')==4 and s.get('pose_assets')==48 and s.get('failures')==0 and snow.get('failures')==0 and world.get('rain_instances',0)>0 and world.get('weather_failures')==0
            if a.walk:
                ok=ok and route.get('complete')is True and route.get('collision_blocked_steps')==0 and route.get('slope_blocked_steps')==0
                if a.ice:ok=ok and snow.get('created')==0 and snow.get('puffs_created')==0 and snow.get('depressed_texels')==0
                else:ok=ok and snow.get('created',0)>100 and abs(snow.get('left_steps',0)-snow.get('right_steps',0))<=1 and snow.get('puffs_created',0)>600 and snow.get('audio_steps',0)>100 and snow.get('imprint_updates',0)>100 and snow.get('depressed_texels',0)>100 and .03<snow.get('deepest_imprint',0)<.04
            row=dict(name=name,passed=ok,exit=code,renderer=renderer,stronghold=s,snow=snow,route=route,memory=monitor.summary(),performance=perf);runs.append(row)
            (out/'results.json').write_text(json.dumps(dict(passed=all(r['passed']for r in runs),runs=runs),indent=2)+'\n');print(json.dumps({k:v for k,v in row.items()if k!='performance'}),flush=True)
    unchanged=before=={str(f):sha(f)for f in protected if f.is_file()}
    result=dict(passed=unchanged and all(r['passed']for r in runs),source_and_player_saves_unchanged=unchanged,binary_sha256=sha(ROOT/'rpg_grandstone'),runs=runs)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())
