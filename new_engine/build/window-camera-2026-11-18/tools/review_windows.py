from pathlib import Path
import argparse,hashlib,json,math,shutil,subprocess,sys,time
from PIL import Image
N=Path(__file__).resolve().parents[1];E=N.parents[1];B=N.with_name('playable-opening-2026-11-18');sys.path.insert(0,str(N));from launch import configuration
ap=argparse.ArgumentParser();ap.add_argument('--baseline',action='store_true');a=ap.parse_args()
D=N/'review'/('windows-baseline' if a.baseline else 'windows-updated');D.mkdir(exist_ok=True);P=D/'private-project';origin=(B if a.baseline else N)/'world-project'
if not P.exists():
 P.mkdir();(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 for f in origin.iterdir():
  if f.name in ('saves','player-profile.config','world.studio.workspace','studio-layout.ini'):continue
  t=P/f.name
  if f.is_dir() or f.is_symlink():t.symlink_to(f.resolve(),target_is_directory=f.is_dir())
  else:shutil.copy2(f,t)
 (P/'saves').mkdir();(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
shaders=P/'shaders'
if shaders.is_symlink():shaders.unlink()
shaders.symlink_to((B if a.baseline else N)/'shared/shaders',target_is_directory=True)
shutil.copy2(origin/'CHARACTER_POLISH.json',P/'CHARACTER_POLISH.json')
models=P/'models'
if models.is_symlink():models.unlink()
models.mkdir(exist_ok=True)
omit={'aquatic_hero','combat_anatomy','weapon_loadouts','unarmed_hero','hero_locomotion','hero_grips','fatality_hero'}
for f in (origin/'models').iterdir():
 if f.name in omit:continue
 t=models/f.name
 if not t.exists():t.symlink_to(f.resolve(),target_is_directory=f.is_dir())
opening=P/'opening'
if opening.is_symlink():opening.unlink();shutil.copytree(origin/'opening',opening,symlinks=True)
# One unchanged living-room probe, whose box contains every comparison view.
# Other empty publisher display rooms are outside this diagnostic.
it=(origin/'opening/interior.settings').read_text().splitlines()
probes=[r for r in it if r.startswith('probe ')]
it=[r for r in it if not r.startswith('probe ')]
it.insert(2,probes[-1]);(opening/'interior.settings').write_text('\n'.join(it)+'\n')
s=(origin/'world.studio').read_text();rows=s.splitlines()
for i,row in enumerate(rows):
 if row.startswith('look '):
  t=row.split();t[2:5]=['.25','.85','1.12'];rows[i]=' '.join(t)
(P/'world.studio').write_text('\n'.join(rows)+'\n')
settings=json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())['text'].replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0');(P/'graphics.settings').write_text(settings)
points=[('window-room',-3498,601.75,-11005,-3498.2,601.15,-11000.2),('window-floor',-3498,601.55,-11004.7,-3498.2,600.15,-11001.2),('room-return',-3497.3,601.6,-11001.4,-3498.3,600.65,-11005)]
lines=['VALWOOD_SOURCE_REVIEW 1 60']
for label,x,y,z,tx,ty,tz in points:
 dx,dz=tx-x,tz-z;lines.append(f'{label} 0 16 1 {x} {y} {z} {math.atan2(dx,-dz)} {math.atan2(ty-y,math.hypot(dx,dz))} 1.0471975511966')
route=D/'cameras.route';route.write_text('\n'.join(lines)+'\n')
cmd,env,_=configuration('world');cmd[cmd.index('--project')+1]=str(P);cmd[cmd.index('--scenic')+1]='apartment'
if a.baseline:cmd[0]=str(B/'runtime/rpg')
env={k:v for k,v in env.items() if not k.startswith('VALWOOD_MODULAR_KNIGHT') and k!='VALWOOD_SKELETAL_LOADOUTS'}
env.update(SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',VALWOOD_QA_SOURCE_ROUTE=str(route),VALWOOD_QA_SOURCE_ROUTE_REPORT=str(D/'ROUTE'),VALWOOD_QA_SOURCE_ROUTE_CAPTURE=str(D/'view'))
cmd[cmd.index('--width')+1]='1280';cmd[cmd.index('--height')+1]='720'
cmd+=['--hidden','--no-hud','--benchmark','--frames','800','--warmup','4','--profile',str(D/'performance.json')]
start=time.monotonic()
with (D/'native.log').open('w') as out:r=subprocess.run(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=480)
for f in D.glob('view*.bmp'):Image.open(f).save(f.with_suffix('.png'))
world=json.loads((D/'performance.json.world.json').read_text()) if (D/'performance.json.world.json').exists() else {};rr=json.loads((D/'ROUTE.json').read_text()) if (D/'ROUTE.json').exists() else {}
passed=r.returncode==0 and world.get('gl_error')==0 and rr.get('complete') and rr.get('failures')==0 and world.get('local_light_shadows',{}).get('failures')==0
report=dict(passed=bool(passed),binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),baseline=a.baseline,exit_code=r.returncode,elapsed_seconds=time.monotonic()-start,command=cmd,local_shadows=world.get('local_light_shadows'),source_route=rr,private_save=True,static_lighting_diagnostic=True,unused_player_banks_omitted=True,one_unchanged_room_probe=True,software_renderer=env.get('LIBGL_ALWAYS_SOFTWARE')=='1',apartment_sun_sky_exposure=[.25,.85,1.12],settings=settings)
(D/'EXECUTION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert passed
