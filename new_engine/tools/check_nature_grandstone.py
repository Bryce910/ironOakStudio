#!/usr/bin/env python3
"""Native GPU captures or walking rehearsal of the purchased mountain citadel."""
from pathlib import Path
import argparse,fcntl,hashlib,json,os,shutil,subprocess,tempfile
from PIL import Image
from benchmark_graphics_settings import native_environment,MemoryMonitor

ROOT=Path(__file__).resolve().parents[1];PROJECT=ROOT/'projects/valwood_stronghold'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def scene_inputs():
 paths=list((PROJECT/'shaders').glob('*.glsl'))+[ROOT/'engine/world_nature_colliders.inc']
 paths+=[PROJECT/f'models/stronghold/{name}.glb'for name in ['nature_mountain_shell','nature_snow_boulder','nature_service_wall','nature_throne']]
 return{str(p.relative_to(ROOT)):digest(p)for p in sorted(paths)if p.is_file()}
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--document',type=Path);p.add_argument('--scene',default='stronghold-gate');p.add_argument('--walk',action='store_true');p.add_argument('--frames',type=int,default=360);p.add_argument('--scale',type=float,default=1);a=p.parse_args()
 lock=(ROOT/'projects/seldonne/.valwood-instance.lock').open('a')
 try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 except BlockingIOError:raise RuntimeError('A Valwood session is already running; native validation needs an exclusive game session')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 for name in ['result.json','profile.json','profile.json.world.json','capture.bmp','capture.png']:(out/name).unlink(missing_ok=True)
 protected=[PROJECT/'world.studio',PROJECT/'graphics.settings',ROOT/'projects/valwood_visual/world.studio']
 for project in [PROJECT,ROOT/'projects/valwood_visual',ROOT/'projects/valwood_world']:protected+=sorted((project/'saves').rglob('*'))
 before={str(f):digest(f)for f in protected if f.is_file()}
 document_sha=digest(a.document)if a.document else digest(PROJECT/'world.studio')
 binary_sha=digest(ROOT/'rpg_grandstone')
 inputs=scene_inputs()
 with tempfile.TemporaryDirectory(prefix='grandstone-citadel-native-')as temp:
  env,_=native_environment();env.update(DISPLAY=':0',XAUTHORITY='/home/computer/.Xauthority',VALWOOD_PRESENTATION='1',VALWOOD_QA_PROGRESS_FILE=str(Path(temp)/'journey.progress'),VALWOOD_QA_FRAME_TIMES_ONLY='1',VALWOOD_CAPTURE_VIEW='first')
  if a.document:
   private_doc=Path(temp)/'world.studio';shutil.copy2(a.document,private_doc)
   env['VALWOOD_QA_STUDIO_DOCUMENT']=str(private_doc)
  profile=out/'profile.json';capture=out/'capture.bmp'
  command=[str(ROOT/'rpg_grandstone'),'--project',str(PROJECT),'--play','--scenic',a.scene,'--no-hud','--frames',str(a.frames),'--width','1600','--height','900','--warmup','120','--auto-quality','off','--render-scale',str(a.scale),'--profile',str(profile),'--capture',str(capture)]
  if a.walk:command+=['--benchmark','--view','river'];env['VALWOOD_QA_STRONGHOLD_SNOW']='1'
  monitor=MemoryMonitor();monitor.start()
  try:
   with(out/'run.log').open('w')as log:result=subprocess.run(command,cwd='/tmp',env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
  finally:monitor.stop()
  assert result.returncode==0,(result.returncode,str(out/'run.log'))
  world=json.loads(Path(str(profile)+'.world.json').read_text());perf=json.loads(profile.read_text())
  assert perf['frames']>0 and capture.exists(),('Game exited before producing a capture',str(out/'run.log'))
  if a.document:
   text=a.document.read_text().splitlines()
   assert world['studio']['added_models']==sum(row.startswith('prop ')for row in text),'Candidate document was not loaded'
   assert world['studio_tools']['asset_definitions']==sum(row.startswith('asset ')for row in text),'Candidate palette was not loaded'
  assert 'AMD'in perf['renderer']and not any(v in perf['renderer'].lower()for v in ['llvmpipe','software'])
  for key in ['gl_error','failed_tiles','remaining_texture_references','weather_failures']:assert world[key]==0,(key,world[key])
  assert world['stronghold']['failures']==0 and world['powder_snow']['failures']==0
  route=world.get('benchmark_route',{})
  if a.walk:assert route['complete']and route['collision_blocked_steps']==0 and route['slope_blocked_steps']==0,route
  Image.open(capture).convert('RGB').save(out/'capture.png');capture.unlink()
 after={str(f):digest(f)for f in protected if f.is_file()};assert before==after
 if a.document:assert digest(a.document)==document_sha,'Candidate changed during validation'
 assert inputs==scene_inputs(),'Scene assets changed during validation'
 report=dict(passed=True,scene=a.scene,frames=a.frames,render_scale=a.scale,renderer=perf['renderer'],mean_fps=perf['mean_fps'],one_percent_low_fps=perf['one_percent_low_fps'],frames_below45=perf['frames_over_45fps_budget'],max_ms=perf['max_ms'],document_sha256=document_sha,binary_sha256=binary_sha,scene_inputs_sha256=inputs,saved_project_state_unchanged=True,route=route,stronghold=world['stronghold'],snow=world['powder_snow'],memory=monitor.summary(),screenshot=str(out/'capture.png'))
 report['performance_45_fps_floor_passed']=report['frames_below45']==0
 (out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
