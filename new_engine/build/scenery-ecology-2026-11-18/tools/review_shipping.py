"""Unmodified shipping graphics settings, ordinary idle opening, private save."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
from PIL import Image
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N));from launch import configuration
D=N/'review/shipping';D.mkdir(exist_ok=True);P=D/'private-project';origin=N/'world-project'
if not P.exists():
 P.mkdir();(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 for f in origin.iterdir():
  if f.name in ('saves','studio-layout.ini','world.studio.workspace','player-profile.config'):continue
  t=P/f.name
  if f.is_dir() or f.is_symlink():t.symlink_to(f.resolve(),target_is_directory=f.is_dir())
  else:shutil.copy2(f,t)
 (P/'saves').mkdir();(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
settings=json.loads((N/'review/ACTIVE_SETTINGS.json').read_text());(P/'graphics.settings').write_text(settings['text'])
cmd,env,_=configuration('world');cmd[cmd.index('--project')+1]=str(P);cmd[cmd.index('--scenic')+1]='seldonne-arrival'
env.update(VALWOOD_PRESENTATION='1',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
cmd+=['--hidden','--frames','600','--warmup','150','--capture',str(D/'result.bmp'),'--profile',str(D/'performance.json')]
start=time.monotonic()
with (D/'native.log').open('w') as f:r=subprocess.run(cmd,cwd=E,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=360)
world=json.loads((D/'performance.json.world.json').read_text()) if (D/'performance.json.world.json').exists() else {};profile=json.loads((D/'performance.json').read_text()) if (D/'performance.json').exists() else {};log=(D/'native.log').read_text()
passed=r.returncode==0 and world.get('gl_error')==0 and '[skeletal-loadouts] loaded4 purchased rigs' in log and 'A required renderer resource failed' not in log
if (D/'result.bmp').exists():Image.open(D/'result.bmp').save(D/'result.png')
out=dict(passed=passed,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),exit_code=r.returncode,elapsed_seconds=time.monotonic()-start,ordinary_idle_scene=True,live_saves_used=False,no_source_review_or_combat_qa=True,shipping_settings_unchanged=True,settings=(P/'graphics.settings').read_text(),profile=profile,world=world,scope='Idle opening at its normal spawn. Not sustained movement/combat certification.')
(D/'EXECUTION.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({k:v for k,v in out.items() if k not in ['world','profile','settings']},indent=2));print('Mean FPS',profile.get('mean_fps'));assert passed
