from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time,sys
N=Path(__file__).resolve().parent;E=N.parents[1];sys.path.insert(0,str(N))
from launch import configuration
ap=argparse.ArgumentParser();ap.add_argument('test',choices=['walking-first','walking-third','opening','skip','journey','editor','arena','joust','scenery','road','arena-orcs','equipment','area-city','area-grandstone','area-showcase','area-atlas']);ap.add_argument('--first-case',type=int);ap.add_argument('--last-case',type=int);ap.add_argument('--name');a=ap.parse_args();
if a.name and (not a.name.replace('-','').isalnum()):raise ValueError('Invalid private review name')
if a.first_case is not None and (a.test not in ('arena','arena-orcs') or a.last_case is None or not 0<=a.first_case<a.last_case<=30):raise ValueError('Invalid bounded clash range')
mode={'walking-first':'world','walking-third':'world','opening':'demo','skip':'demo','journey':'demo','scenery':'world','road':'demo','arena-orcs':'arena','equipment':'arena'}.get(a.test,'world' if a.test.startswith('area-') else a.test)
d=N/'review'/(a.name or a.test);d.mkdir(parents=True,exist_ok=True)
private=Path('/tmp')/('valwood-third-person-polish-'+(a.name or a.test)+'-20261117');private.mkdir(exist_ok=True)
command,env,origin=configuration(mode,['--area',a.test[5:]] if a.test.startswith('area-') else [])
if a.test=='walking-first':
 command,env,origin=configuration('arena')
 env.pop('VALWOOD_COMBAT_ARENA',None);env['VALWOOD_WORLD_EXPLORATION']='1'
 # Same purchased player rigs and shaders; smaller private scene for first-person input/camera checks.

for f in origin.iterdir():
 if f.name in ('saves','settings','studio-layout.ini','world.studio.workspace') or '.recovery' in f.name:continue
 out=private/f.name
 if out.exists() or out.is_symlink():continue
 if f.is_dir():out.symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,out,follow_symlinks=True)
shaders=private/'shaders'
if shaders.is_symlink():shaders.unlink()
if not shaders.exists():shaders.symlink_to(N/'shared/shaders',target_is_directory=True)
# Current authored arena is copied above, including the prior floor repair.
# Native authoring/story checks write only these private documents and saves.
(private/'saves').mkdir(exist_ok=True)
(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
opening=private/'opening'
if opening.is_symlink():opening.unlink();shutil.copytree(origin/'opening',opening,symlinks=True)
if a.test=='walking-first':(private/'arena.config').unlink(missing_ok=True)
settings=(origin/'graphics.settings').read_text().replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0')
diagnostic_balanced=a.test in ('road','walking-first','walking-third','editor','opening')
if diagnostic_balanced:
 settings=settings.replace('shadow_side=4096','shadow_side=2048').replace('reflection_scale=1\n','reflection_scale=0.5\n').replace('foliage_distance=Ultra','foliage_distance=High').replace('foliage_density=Ultra','foliage_density=High')
(private/'graphics.settings').write_text(settings)
env.update(SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
frames=1400;width=1280;height=720;timeout=720
if a.test in ('opening','skip'):
 (private/'.valwood-opening-startup-qa').write_text('VALWOOD_PRIVATE_OPENING_STARTUP_V1\n')
 env.update(VALWOOD_QA_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_STARTUP_ONLY='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(d),VALWOOD_QA_OPENING_STARTUP_RAW=str(d/'OPENING_RAW.json'))
 if a.test=='opening':env['VALWOOD_QA_OPENING_STARTUP_NATURAL']='1';frames=2000
 else:frames=1200
elif a.test=='journey':
 fixture=E/'docs/equipment-progression/2026-11-08/journey-native-v4/project/opening'
 for name in ('qa.walk','first-journey.qa.walk'):shutil.copy2(fixture/name,opening/name)
 shutil.copy2(N/'review/fixtures/traffic-aware-first-journey.qa.walk',opening/'first-journey.qa.walk')
 # The retained quest driver predates the optional first-NPC cinematic.
 # Its separate dialogue/cancel assertions require the plain conversation.
 # Only this private rehearsal disables that optional presentation; shipping keeps it.
 intro=opening/'npc-intro.settings'
 if intro.exists():intro.rename(opening/'npc-intro.settings.not-used-by-legacy-quest-driver')
 env.update(VALWOOD_QA_FIRST_JOURNEY='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(d),VALWOOD_QA_FIRST_JOURNEY_CAPTURE_DIR=str(d))
 frames=30000;timeout=1000
elif a.test.startswith('walking-'):
 (private/'.valwood-walking-qa').write_text('VALWOOD_WALKING_QA_V1\n')
 env.update(VALWOOD_PRESENTATION='1',VALWOOD_QA_WALKING_CONTROLS='1',VALWOOD_WALKING_QA_VIEW=a.test[8:],VALWOOD_WALKING_QA_OUTPUT=str(d))
 frames=5000;timeout=600
elif a.test=='road':
 (private/'.valwood-road-qa').write_text('VALWOOD_PRIVATE_ROAD_V1\n')
 shutil.copy2(E/'build/road-life-2026-11-15/review/native-road/private-project/saves/opening.progress',private/'saves/opening.progress')
 # Clearance diagnosis uses the identical world and traffic assets, with unused
 # player-only animation banks omitted. This is not a combat acceptance run.
 shutil.copy2(origin/'CHARACTER_POLISH.json',private/'CHARACTER_POLISH.json')
 modelroot=private/'models'
 if modelroot.is_symlink():modelroot.unlink();modelroot.mkdir()
 for item in (origin/'models').iterdir():
  target=modelroot/item.name
  if item.name not in ('hero_locomotion','hero_grips','unarmed_hero','weapon_loadouts','aquatic_hero') and not target.exists():target.symlink_to(item.resolve(),target_is_directory=item.is_dir())
 env={k:v for k,v in env.items() if not k.startswith('VALWOOD_MODULAR_KNIGHT') and k!='VALWOOD_SKELETAL_LOADOUTS'}
 env.update(VALWOOD_QA_ROAD_TRAFFIC='1',VALWOOD_QA_ROAD_DIR=str(d))
 frames=1200
elif a.test=='editor':
 env.update(VALWOOD_QA_EDITOR_WORKFLOWS_V17='1',VALWOOD_QA_STUDIO_DOCUMENT=str(private/'world.studio'))
 frames=2400
elif a.test=='equipment':
 (private/'.valwood-equipment-qa').write_text('VALWOOD_PRIVATE_EQUIPMENT_V1\n')
 (private/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 (private/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
 subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(private),'human'],check=True)
 env.update(VALWOOD_QA_EQUIPMENT='1',VALWOOD_QA_EQUIPMENT_DIR=str(d),VALWOOD_CAPTURE_VIEW='first',VALWOOD_CAPTURE_ARENA_START='1')
 frames=1800;timeout=600
elif a.test in ('arena','arena-orcs'):
 (private/'.valwood-blade-clash-qa').write_text('VALWOOD_PRIVATE_BLADE_CLASH_V1\n')
 (private/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 (private/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight04\nend\n')
 subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(private),'human'],check=True)
 env.update(VALWOOD_QA_BLADE_CLASH='1',VALWOOD_QA_BLADE_CLASH_DIR=str(d),VALWOOD_QA_CLASH_FIRST_CASE='0',VALWOOD_QA_CLASH_LAST_CASE='2')

 if a.test=='arena-orcs':env.update(VALWOOD_QA_CLASH_FIRST_CASE='4',VALWOOD_QA_CLASH_LAST_CASE='10',VALWOOD_QA_CLASH_PLAYER_ID='knight04')
 frames=8000 if a.test=='arena-orcs' else 3600;timeout=600
elif a.test=='joust':
 (d/'.valwood-joust-qa').write_text('VALWOOD_PRIVATE_JOUST_V1\n')
 env.update(VALWOOD_QA_JOUST='1',VALWOOD_QA_JOUST_DIR=str(d),VALWOOD_QA_JOUST_MODE='controller-third')
 frames=4200;timeout=600
elif a.test.startswith('area-'):
 frames=300
elif a.test=='scenery':
 # Actual world and the same shared materials; static full-quality performance sample.
 frames=500
if a.first_case is not None:env.update(VALWOOD_QA_CLASH_FIRST_CASE=str(a.first_case),VALWOOD_QA_CLASH_LAST_CASE=str(a.last_case))
capture=d/'result.bmp'
command[command.index('--project')+1]=str(private)
command += (['--hidden'] if a.test in ('arena','arena-orcs','equipment','joust') else []) + ['--frames',str(frames),'--warmup','30','--capture',str(capture),'--profile',str(d/'performance.json')]
# Replace the base launch size to keep captures consistent across this integration.
command[command.index('--width')+1]=str(width);command[command.index('--height')+1]=str(height)
import threading
stop_consumer=threading.Event()
def consume():
 from PIL import Image
 while not stop_consumer.wait(.05):
  for p in d.glob('walking-*.bmp'):
   try:
    with Image.open(p) as img:img.save(p.with_suffix('.png'))
    p.unlink()
   except (OSError,ValueError):pass
if a.test.startswith('walking-'):threading.Thread(target=consume,daemon=True).start()
start=time.monotonic()
timed_out=False
with (d/'native.log').open('w') as log:
 proc=subprocess.Popen(command,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
 while proc.poll() is None:
  if time.monotonic()-start>timeout:
   timed_out=True;proc.kill();proc.wait();log.write('\n[Integration review] TIMEOUT after '+str(timeout)+' seconds\n');break
  if env.get('SDL_VIDEODRIVER')!='offscreen' and a.test in ('road','walking-first','walking-third','editor','opening','skip'):
   rows=subprocess.run(['wmctrl','-lpG'],capture_output=True,text=True).stdout.splitlines()
   for row in rows:
    fields=row.split()
    if len(fields)>2 and fields[2]==str(proc.pid):
     active=subprocess.run(['xprop','-root','_NET_ACTIVE_WINDOW'],capture_output=True,text=True).stdout.strip().split()[-1]
     if active.startswith('0x') and int(active,16)!=int(fields[0],16):subprocess.run(['wmctrl','-ia',fields[0]],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
     break
  time.sleep(.25)
 result=subprocess.CompletedProcess(command,124 if timed_out else proc.returncode)
stop_consumer.set()
worldpath=d/'performance.json.world.json'
if not worldpath.exists():worldpath=Path(str(capture)+'.world.json')
world=json.loads(worldpath.read_text()) if worldpath.exists() else {}
rawname={'opening':'OPENING_RAW.json','skip':'OPENING_RAW.json','arena':'NATIVE_CLASH_RAW.json','arena-orcs':'NATIVE_CLASH_RAW.json','equipment':'EQUIPMENT_NATIVE_RAW.json','joust':'JOUST_NATIVE_RAW.json','road':'ROAD_NATIVE_RAW.json'}.get(a.test)
raw=json.loads((d/rawname).read_text()) if rawname and (d/rawname).exists() else {}
logtext=(d/'native.log').read_text()
passed=not timed_out and result.returncode==0 and world.get('gl_error')==0 and 'Surface library rejected:' not in logtext and 'Safe legacy fallback:' not in logtext and 'Contact requested before explicit loadout transition seed' not in logtext
if a.test not in ('joust','road'):passed=passed and '[skeletal-loadouts] loaded4 purchased rigs' in logtext
if a.test=='scenery' or a.test.startswith('area-'):
 profile=json.loads((d/'performance.json').read_text()) if (d/'performance.json').exists() else {}
 passed=passed and profile.get('samples',0)>100 and (d/'result.bmp').is_file()
if rawname:passed=passed and raw.get('passed') is True
elif a.test=='journey':passed=passed and world.get('first_journey_qa',{}).get('passed') is True
elif a.test.startswith('walking-'):passed=passed and world.get('walking_controls_qa',{}).get('passed') is True
elif a.test=='editor':passed=passed and '[Editor workflows v17 QA] PASS' in (d/'native.log').read_text()
record=dict(timed_out=timed_out,passed=passed,test=a.test,exit_code=result.returncode,elapsed_seconds=time.monotonic()-start,binary_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest(),command=command,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},raw=raw,world=world,private_project=str(private),optional_intro_disabled_only_for_legacy_quest_driver=(a.test=='journey'),traffic_aware_centreline_fixture=(a.test=='journey'),player_only_banks_omitted_for_clearance_diagnostic=(a.test=='road'),compact_scene_for_first_person_controls=(a.test=='walking-first'),private_balanced_graphics=diagnostic_balanced,sdl_video_driver=env.get('SDL_VIDEODRIVER','native-default'),external_window_activation_for_visible_input_tests=(env.get('SDL_VIDEODRIVER')!='offscreen' and a.test in ('road','walking-first','walking-third','editor','opening','skip')))
(d/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:record[k] for k in ('passed','test','exit_code','elapsed_seconds','raw')},indent=2))
if passed and a.test=='scenery':
 subprocess.run([sys.executable,str(N/'check_story_cpu.py')],check=True)
sys.exit(0 if passed else 1)
