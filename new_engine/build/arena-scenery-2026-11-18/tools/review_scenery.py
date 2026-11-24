#!/usr/bin/env python3
"""Matched native arena views and a separate no-readback timing route."""
from pathlib import Path
import csv,json,math,os,shutil,subprocess,sys,time,hashlib
from PIL import Image
N=Path(__file__).resolve().parents[1];E=N.parents[1];BASE=N.with_name('third-person-focus-2026-11-18');sys.path.insert(0,str(N))
from launch import configuration
baseline='--baseline' in sys.argv;timing='--timing' in sys.argv
D=N/'review'/('baseline' if baseline else 'scenery')/('timing' if timing else 'views');D.mkdir(parents=True,exist_ok=True)
P=D/'private-project';origin=(BASE if baseline else N)/'arena-project'
if not P.exists():
 P.mkdir();(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 for s in origin.iterdir():
  if s.name in ('saves','studio-layout.ini','world.studio.workspace','player-profile.config','arena_player_character.cfg'):continue
  t=P/s.name
  if s.is_dir() or s.is_symlink():t.symlink_to(s.resolve(),target_is_directory=s.is_dir())
  else:shutil.copy2(s,t)
 (P/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
 subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(P),'human'],check=True)
shutil.copy2(origin/'world.studio',P/'world.studio')
(P/'graphics.settings').write_text((origin/'graphics.settings').read_text().replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0'))
points=[('inside-north',0,65.75,-7,0,65.5,20),('east-equipment',-6,65.75,0,25,66,0),('entry-court',0,65.75,7,0,65.7,-27),('west-shelter',6,65.75,0,-25,65.8,0),('woodland-overview',10,84,-16,0,65,3),('seating',-4,68,-4,0,66,17)]
lines=['VALWOOD_SOURCE_REVIEW 1 60']
for label,x,y,z,tx,ty,tz in points:
 dx,dz=tx-x,tz-z;lines.append(f'{label} 0 {240 if timing else 90} {0 if timing else 1} {x-1800} {y} {z-10500} {math.atan2(dx,-dz)} {math.atan2(ty-y,math.hypot(dx,dz))} 1.0471975511966')
route=D/'cameras.route';route.write_text('\n'.join(lines)+'\n')
cmd,env,_=configuration('arena');cmd[0]=str((BASE if baseline else N)/'runtime/rpg');cmd[2]=str(P)
cmd+=['--hidden','--no-hud','--benchmark','--frames','2600','--warmup','60','--profile',str(D/'performance.json')]
env.update(SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(D/'ROUTE'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(D/'view'))
if True:env['VALWOOD_QA_TEXTURE_OBSERVATION']=str(D/'TEXTURE_GPU.json');env['LD_PRELOAD']=str(E/'build/arena-material-quality-2026-11-15/review/libtexture_observer.so')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
inputs={str(p):sha(p) for p in [Path(cmd[0]),P/'world.studio',P/'graphics.settings',route,Path(__file__)]};start=time.monotonic()
with (D/'native.log').open('w') as f:r=subprocess.run(cmd,cwd=E,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
assert all(sha(p)==h for p,h in inputs.items())
for p in D.glob('*.bmp'):Image.open(p).save(p.with_suffix('.png'))
report=json.loads((D/'ROUTE.json').read_text()) if (D/'ROUTE.json').exists() else {}
frames=list(csv.DictReader((D/'ROUTE.frames.csv').open())) if (D/'ROUTE.frames.csv').exists() else []
timings={}
for label,*_ in points:
 rows=[x for x in frames if x['label']==label and x['phase']=='hold' and x['capture_readback']=='0'][30:]
 if rows:
  ms=sorted(float(x['frame_ms']) for x in rows);gpu=sorted(float(x['gpu_ms']) for x in rows)
  timings[label]=dict(frames=len(rows),median_ms=ms[len(ms)//2],p95_ms=ms[int(len(ms)*.95)],mean_ms=sum(ms)/len(ms),median_gpu_ms=gpu[len(gpu)//2])
result=dict(passed=r.returncode==0 and report.get('complete') and report.get('failures')==0,exit_code=r.returncode,binary_sha256=inputs[cmd[0]],inputs=inputs,command=cmd,elapsed_seconds=time.monotonic()-start,native_GPU=True,private_saves=True,baseline=baseline,timing_no_readbacks=timing,timings=timings)
(D/'EXECUTION.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['passed']
