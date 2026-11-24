"""Private scenery/armor captures, using the candidate shader bank; never writes live saves."""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys,time,hashlib
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N));from launch import configuration
p=argparse.ArgumentParser();p.add_argument('case',choices=['camp','town','city','grandstone','jungle','wood-floor-diagnostic','armor-clean','armor-blood','armor-first']);p.add_argument('--baseline',action='store_true');a=p.parse_args()
case=a.case;baseline=E/'build/animation-polish-2026-11-17';G=baseline if a.baseline else N
mode='arena' if case.startswith('armor') or case=='wood-floor-diagnostic' else 'world';extra=['--area',{'city':'city','grandstone':'grandstone','jungle':'atlas'}[case]] if case in ('city','grandstone','jungle') else []
cmd,env,origin=configuration(mode,extra);d=N/'review'/((case+'-baseline') if a.baseline else case);d.mkdir(exist_ok=True)
private=Path('/tmp')/('valwood-surface-review-'+d.name);private.mkdir(exist_ok=True)
for f in origin.iterdir():
 if f.name in ('saves','settings','studio-layout.ini','world.studio.workspace','graphics.settings','shaders') or '.recovery' in f.name:continue
 out=private/f.name
 if out.exists() or out.is_symlink():continue
 if f.is_dir():out.symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,out,follow_symlinks=True)
(private/'saves').mkdir(exist_ok=True);(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
shaders=private/'shaders'
if shaders.is_symlink():shaders.unlink()
shaders.symlink_to(G/'shared/shaders',target_is_directory=True)
if mode=='arena' and not a.baseline:shutil.copy2(N/'arena-scene-patch/world.studio',private/'world.studio')
settings=(origin/'graphics.settings').read_text().replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0');(private/'graphics.settings').write_text(settings)
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1')
scenery_only=not case.startswith('armor')
if scenery_only:
 env={k:v for k,v in env.items() if not k.startswith('VALWOOD_MODULAR_KNIGHT') and k!='VALWOOD_SKELETAL_LOADOUTS'}
 models=private/'models'
 if models.is_symlink():models.unlink()
 models.mkdir(exist_ok=True)
 for item in (origin/'models').iterdir():
  if item.name in ('aquatic_hero','combat_anatomy'):continue
  dest=models/item.name
  if not dest.exists():dest.symlink_to(item.resolve(),target_is_directory=item.is_dir())
 for name in ('aquatic_hero','combat_anatomy'):
  blocked=models/name
  if blocked.is_symlink():blocked.unlink()
if case=='wood-floor-diagnostic':
 (private/'showcase.views').write_text('city-walk -1800 65.75 -10492 0 -.22 1.0471975512\n')
 cmd[cmd.index('--scenic')+1]='city-walk'
 env.pop('VALWOOD_COMBAT_ARENA',None);env.pop('VALWOOD_PLAYTEST_SLICE',None);env['VALWOOD_WORLD_EXPLORATION']='1';(private/'arena.config').unlink(missing_ok=True)
elif case.startswith('armor'):
 (private/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 (private/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight04\nend\n')
 (private/'arena_armor_finish.cfg').write_text('VALWOOD_ARMOR_FINISH 1\nstyle '+('0' if case=='armor-clean' else '2')+'\nend\n')
 subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(private),'human'],check=True)
 env.update(VALWOOD_CAPTURE_ARENA_START='1',VALWOOD_CAPTURE_VIEW='first' if case=='armor-first' else 'third')
 if a.baseline:
  env['VALWOOD_MODULAR_KNIGHT_SHADER']=str(G/'shared/shaders/world_skeletal.vs.glsl');env['VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER']=str(G/'shared/shaders/rig_first_person.fs.glsl')
cmd[0]=str(G/'runtime/rpg');cmd[cmd.index('--project')+1]=str(private)
if case in ('camp','town'):cmd[cmd.index('--scenic')+1]='seldonne-arrival' if case=='camp' else 'city-walk'
cmd[cmd.index('--width')+1]='1280';cmd[cmd.index('--height')+1]='720'
cmd+=['--hidden','--frames','180','--warmup','30','--capture',str(d/'result.bmp'),'--profile',str(d/'performance.json')]
start=time.monotonic()
with (d/'native.log').open('w') as log:
 try:r=subprocess.run(cmd,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420);code=r.returncode
 except subprocess.TimeoutExpired:code=124
worldpath=d/'performance.json.world.json';world=json.loads(worldpath.read_text()) if worldpath.exists() else {};log=(d/'native.log').read_text()
passed=code==0 and world.get('gl_error')==0 and (d/'result.bmp').is_file() and 'Missing texture ground/' not in log and 'Surface library rejected:' not in log
receipt=dict(passed=passed,case=case,baseline=a.baseline,exit_code=code,elapsed_seconds=time.monotonic()-start,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),command=cmd,private_project=str(private),scenery_diagnostic_player_banks_omitted=scenery_only,world=world)
(d/'EXECUTION.json').write_text(json.dumps(receipt,indent=2)+'\n')
if (d/'result.bmp').is_file():
 from PIL import Image
 with Image.open(d/'result.bmp') as im:im.save(d/'result.png')
print(json.dumps({k:receipt[k] for k in ('passed','case','baseline','exit_code','elapsed_seconds')}));sys.exit(0 if passed else 1)
