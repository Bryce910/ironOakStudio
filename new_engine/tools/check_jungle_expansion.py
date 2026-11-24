#!/usr/bin/env python3
"""Native visual-project jungle checks, with an isolated journey save.
Views are visual checks. The complete route uses ordinary3.5m/s capsule walking
at a fixed render scale; it does not accelerate time or teleport between points.
"""
import argparse,hashlib,json,math,subprocess,tempfile
from pathlib import Path
from PIL import Image
from benchmark_graphics_settings import native_environment,MemoryMonitor
ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_visual'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--walk',action='store_true');p.add_argument('--view',choices=['all','pool','shrine','canopy','boar','leopard','birds'],default='all');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 protected=[ROOT/'rpg',PROJECT/'world.studio',PROJECT/'graphics.settings',PROJECT/'jungle_expansion.json',*sorted((PROJECT/'saves').glob('*')),*sorted((PROJECT/'scenery').glob('*instances.txt'))]
 before={str(f):digest(f)for f in protected if f.is_file()};rows=[]
 choices=[('walk',None)]if a.walk else [(n,i)for i,n in enumerate(['pool','shrine','canopy','boar','leopard','birds'])if a.view in ['all',n]]
 with tempfile.TemporaryDirectory(prefix='valwood-jungle-qa-')as temp:
  for name,index in choices:
   env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'),VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_CAPTURE_VIEW='first')
   frames=19000 if a.walk else 240;scale=.7 if a.walk else 1.;profile=out/(name+'.json');capture=out/(name+'.bmp')
   if a.walk:env['VALWOOD_QA_JUNGLE_EXPANSION']='1'
   else:env.update(VALWOOD_QA_MILESTONE_VIEW='jungle',VALWOOD_QA_JUNGLE_VIEW=str(index))
   cmd=[str(ROOT/'rpg'),'--project',str(PROJECT),'--play','--scenic','rainforest-milestone','--no-hud','--frames',str(frames),'--width','1600','--height','900','--warmup','120','--auto-quality','off','--render-scale',str(scale),'--profile',str(profile)]
   if a.walk:cmd+=['--benchmark','--view','river']
   else:cmd+=['--capture',str(capture)]
   monitor=MemoryMonitor();monitor.start()
   with(out/(name+'.log')).open('w')as log:
    try:code=subprocess.run(cmd,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=850 if a.walk else 240).returncode
    except subprocess.TimeoutExpired:code=-1
   monitor.stop()
   (out/(name+'.memory.json')).write_text(json.dumps(dict(summary=monitor.summary(),samples=monitor.samples),indent=2)+'\n')
   if capture.exists():Image.open(capture).convert('RGB').save(out/(name+'.png'));capture.unlink()
   wp=Path(str(profile)+'.world.json');d=json.loads(wp.read_text())if wp.exists()else{};m=json.loads(profile.read_text())if profile.exists()else{};renderer=m.get('renderer','');w=d.get('wildlife',{});j=d.get('jungle_expansion',{});route=d.get('benchmark_route',{})
   ok=code==0 and 'AMD Radeon RX 5300'in renderer and d.get('gl_error')==0 and d.get('remaining_texture_references')==0 and d.get('asset_streaming',{}).get('failed')==0 and d.get('studio',{}).get('added_models',0)>=400 and w.get('actors')==12 and w.get('species')==3 and w.get('unsafe_roots')==0 and w.get('load_failures')==0 and j.get('flying_birds')==6
   if a.walk:ok=ok and route.get('complete')is True and route.get('collision_blocked_steps')==0 and route.get('slope_blocked_steps')==0 and route.get('return_error_m',1)<.001 and route.get('length_m',0)>1000 and abs(route.get('actual_distance_m',0)-route.get('length_m',0))<.02
   row=dict(name=name,passed=ok,exit=code,renderer=renderer,wildlife=w,expansion=j,route=route,graphics=d.get('graphics'),memory=monitor.summary());rows.append(row)
   (out/'results.json').write_text(json.dumps(dict(passed=all(r['passed']for r in rows),binary_sha256=before[str(ROOT/'rpg')],runs=rows),indent=2)+'\n');print(json.dumps(row),flush=True)
 after={str(f):digest(f)for f in protected if f.is_file()};unchanged=before==after
 result=dict(passed=unchanged and all(r['passed']for r in rows),inputs_and_player_saves_unchanged=unchanged,binary_sha256=before[str(ROOT/'rpg')],runs=rows)
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())
