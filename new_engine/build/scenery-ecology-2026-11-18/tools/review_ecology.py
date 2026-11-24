"""Native matched ecology views; independent no-readback camera benchmark."""
from pathlib import Path
import argparse,csv,hashlib,json,math,os,shutil,subprocess,sys,time
from PIL import Image
N=Path(__file__).resolve().parents[1];E=N.parents[1];BASE=N.with_name('arena-scenery-2026-11-18');sys.path.insert(0,str(N));from launch import configuration
ap=argparse.ArgumentParser();ap.add_argument('mode',choices=['arena','world']);ap.add_argument('--baseline',action='store_true');ap.add_argument('--timing',action='store_true');ap.add_argument('--full',action='store_true');a=ap.parse_args()
D=N/'review'/a.mode/('baseline' if a.baseline else 'updated')/('gameplay-views' if a.full else 'timing' if a.timing else 'views');D.mkdir(parents=True,exist_ok=True);P=D/'private-project'
for pattern in ['view*.bmp','view*.png','ROUTE.json','ROUTE.frames.csv','performance.json','performance.json.world.json','EXECUTION.json']:
 for f in D.glob(pattern):f.unlink()
origin=(BASE/'arena-project' if a.mode=='arena' else E/'projects/valwood_current') if a.baseline else N/(a.mode+'-project')
if not P.exists():
 P.mkdir();(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 for f in origin.iterdir():
  if f.name in ('saves','studio-layout.ini','world.studio.workspace','player-profile.config','arena_player_character.cfg','models'):continue
  t=P/f.name
  if f.is_dir() or f.is_symlink():t.symlink_to(f.resolve(),target_is_directory=f.is_dir())
  else:shutil.copy2(f,t)
 (P/'saves').mkdir();(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
 if a.mode=='world':
  (P/'models').mkdir()
  for f in (origin/'models').iterdir():
   if a.full or f.name not in ('aquatic_hero','combat_anatomy'):(P/'models'/f.name).symlink_to(f.resolve(),target_is_directory=f.is_dir())
 else:
  (P/'models').symlink_to((origin/'models').resolve(),target_is_directory=True)
  (P/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
  subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(P),'human'],check=True)
surface_link=P/'surfaces'
if surface_link.is_symlink() and surface_link.resolve()!=(origin/'surfaces').resolve():surface_link.unlink();surface_link.symlink_to((origin/'surfaces').resolve(),target_is_directory=True)
if a.mode=='world' and a.full:
 for name in ['aquatic_hero','combat_anatomy']:
  target=P/'models'/name
  if not target.exists():target.symlink_to((origin/'models'/name).resolve(),target_is_directory=True)
if a.mode=='world':
 for name in ['cinematic_grass_0_mid.glb','cinematic_grass_1_mid.glb']:
  target=P/'models'/name
  if target.is_symlink():target.unlink()
  target.symlink_to((origin/'models'/name).resolve())
shutil.copy2(origin/'world.studio',P/'world.studio');(P/'graphics.settings').write_text(json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())['text'].replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0'))
if a.mode=='arena':
 points=[('ring',-1800,65.75,-10507,-1800,65.5,-10480),('shade-floor',-1818,66.5,-10510,-1823,64.2,-10511),('sunlit-verge',-1786,66.6,-10504,-1783,64.2,-10498),('woodland-overview',-1790,84,-10516,-1800,65,-10497)]
else:
 points=[('camp',-11988,13.55,5181,-11988,12.6,5172),('shade-floor',-11998,13.7,5179,-12004,12.1,5175),('path-verge',-11984,13.8,5200,-11955,13.7,5244),('woodland-edge',-11982,14.1,5197,-11993,12.3,5190),('village-approach',-11940,14.7,5300,-11932,13.3,5330)]
lines=['VALWOOD_SOURCE_REVIEW 1 60']
for label,x,y,z,tx,ty,tz in points:
 dx,dz=tx-x,tz-z;lines.append(f'{label} 0 {210 if a.timing else 75} {0 if a.timing else 1} {x} {y} {z} {math.atan2(dx,-dz)} {math.atan2(ty-y,math.hypot(dx,dz))} 1.0471975511966')
route=D/'cameras.route';route.write_text('\n'.join(lines)+'\n');cmd,env,_=configuration(a.mode);cmd[cmd.index('--project')+1]=str(P)
if a.mode=='world':
 cmd[cmd.index('--scenic')+1]='seldonne-arrival'
 if a.full:env['VALWOOD_PRESENTATION']='1'
 else:env={k:v for k,v in env.items() if not k.startswith('VALWOOD_MODULAR_KNIGHT') and k!='VALWOOD_SKELETAL_LOADOUTS'}
cmd+=['--hidden','--no-hud','--benchmark','--frames','2000','--warmup','60','--profile',str(D/'performance.json')]
env.update(SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(D/'ROUTE'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(D/'view'),LD_PRELOAD=str(E/'build/arena-material-quality-2026-11-15/review/libtexture_observer.so'),VALWOOD_QA_TEXTURE_OBSERVATION=str(D/'TEXTURE_GPU.json'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();model_inputs=[P/'models'/name for name in (['cinematic_grass_0.glb','cinematic_grass_0_mid.glb','cinematic_grass_1.glb','cinematic_grass_1_mid.glb'] if a.mode=='world' else ['bep_nature_grass_long_near.glb','bep_nature_grass_long_mid.glb','bep_nature_grass_broad_near.glb','bep_nature_grass_broad_mid.glb'])];inputs={str(p):sha(p) for p in [Path(cmd[0]),P/'world.studio',P/'graphics.settings',route,Path(__file__),*model_inputs,*sorted((P/'scenery').glob('*instances.txt'))]};start=time.monotonic()
with (D/'native.log').open('w') as out:r=subprocess.run(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=360)
assert all(sha(p)==v for p,v in inputs.items())
for f in D.glob('view*.bmp'):Image.open(f).save(f.with_suffix('.png'))
route_report=json.loads((D/'ROUTE.json').read_text()) if (D/'ROUTE.json').exists() else {};world=json.loads((D/'performance.json.world.json').read_text()) if (D/'performance.json.world.json').exists() else {};log=(D/'native.log').read_text();timings={}
if (D/'ROUTE.frames.csv').exists():
 frames=list(csv.DictReader((D/'ROUTE.frames.csv').open()))
 for label,*_ in points:
  rows=[r for r in frames if r['label']==label and r['phase']=='hold' and r['capture_readback']=='0'][30:]
  if rows:
   ms=sorted(float(r['frame_ms']) for r in rows);gpu=sorted(float(r['gpu_ms']) for r in rows);timings[label]=dict(frames=len(rows),median_ms=ms[len(ms)//2],p95_ms=ms[int(len(ms)*.95)],median_gpu_ms=gpu[len(gpu)//2])
passed=r.returncode==0 and route_report.get('complete') and route_report.get('failures')==0 and world.get('gl_error')==0 and all(x not in log for x in ['Surface library rejected:','unknown model alias','A required renderer resource failed','Cannot rebind saved source objects:','Saved asset definitions could not load'])
report=dict(passed=bool(passed),mode=a.mode,baseline=a.baseline,timing=a.timing,exit_code=r.returncode,binary_sha256=sha(cmd[0]),inputs=inputs,command=cmd,elapsed_seconds=time.monotonic()-start,native_GPU=True,private_save=True,player_animation_banks_omitted_for_scenery_only=(a.mode=='world' and not a.full),timings=timings,gl_error=world.get('gl_error'))
(D/'EXECUTION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert passed
