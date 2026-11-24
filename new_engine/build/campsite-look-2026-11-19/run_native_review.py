from pathlib import Path
import importlib.util
import argparse,hashlib,json,os,shutil,subprocess,time,sys
sys.dont_write_bytecode=True
N=Path(__file__).resolve().parent;E=N.parents[1];sys.path.insert(0,str(N))
from launch import private_configuration as configuration
ap=argparse.ArgumentParser();ap.add_argument('test',choices=['apartment','walking-first','walking-third','opening','skip','journey','story-swim','npc-intro','npc-intro-natural','editor','arena','joust','scenery','road','arena-orcs','equipment','area-city','area-grandstone','area-showcase','area-atlas']);ap.add_argument('--first-case',type=int);ap.add_argument('--last-case',type=int);ap.add_argument('--name');ap.add_argument('--real-audio',action='store_true',help='Use the actual SDL audio backend; does not itself certify audible quality');ap.add_argument('--prepare-only',action='store_true',help='Prepare the isolated fixture and record command without starting the game');ap.add_argument('--stowed',action='store_true',help='Verify ordinary stowed movement with the human arm-swing clip');ap.add_argument('--baseline',action='store_true',help='Compare the protected live baseline scene and prior accepted binary in a private project');ap.add_argument('--swim-route',choices=('baseline','creek','deep'),default='baseline');ap.add_argument('--require-dive',action='store_true');ap.add_argument('--route-fixture');ap.add_argument('--test-width',type=int);ap.add_argument('--test-height',type=int);a=ap.parse_args();
if (a.swim_route!='baseline' or a.require_dive) and a.test!='story-swim':raise ValueError('Swim options require story-swim')
if a.baseline and (a.swim_route!='baseline' or a.require_dive):raise ValueError('Baseline reproduction uses the original shallow route')
if (a.test_width is None)!=(a.test_height is None) or (a.test_width is not None and not(640<=a.test_width<=2560 and 360<=a.test_height<=1440)):raise ValueError('Explicit private test size must have both supported dimensions')
if a.route_fixture and (a.test!='story-swim' or a.baseline or Path(a.route_fixture).name!=a.route_fixture or not a.route_fixture.endswith('.qa.walk')):raise ValueError('Named swim fixture must stay in the candidate fixture bank')
if a.stowed and not a.test.startswith('walking-'):raise ValueError('--stowed requires a walking scenario')
if a.name and (not a.name.replace('-','').isalnum()):raise ValueError('Invalid private review name')
if a.first_case is not None and (a.test not in ('arena','arena-orcs') or a.last_case is None or not 0<=a.first_case<a.last_case<=30):raise ValueError('Invalid bounded clash range')
mode={'apartment':'demo','walking-first':'world','walking-third':'world','opening':'demo','skip':'demo','journey':'demo','story-swim':'demo','npc-intro':'demo','npc-intro-natural':'demo','scenery':'world','road':'demo','arena-orcs':'arena','equipment':'arena'}.get(a.test,'world' if a.test.startswith('area-') else a.test)
d=N/'review'/(a.name or a.test);d.mkdir(parents=True,exist_ok=True)
harness_copy=d/'HARNESS_AT_LAUNCH.py';harness_source=Path(__file__).read_bytes();harness_copy.write_bytes(harness_source);harness_sha256=hashlib.sha256(harness_source).hexdigest()
if a.test.startswith('npc-intro') and (d/'EXECUTION.json').exists():raise ValueError('Retain the previous NPC evidence and choose a fresh --name')
private=Path('/tmp')/('valwood-campsite-look-'+(a.name or a.test)+'-20261119');private.mkdir(exist_ok=True)
command,env,origin=configuration(mode,['--area',a.test[5:]] if a.test.startswith('area-') else [])
if mode=='demo':origin=N/'world-project'
selected_configuration=configuration
if a.baseline:
 baseline=Path(json.loads((N/'BASELINE.json').read_text())['baseline'])
 spec=importlib.util.spec_from_file_location('apartment_camera_baseline_launch',baseline/'launch.py');baseline_launch=importlib.util.module_from_spec(spec);spec.loader.exec_module(baseline_launch)
 selected_configuration=baseline_launch.private_configuration
 command,env,origin=selected_configuration(mode,['--area',a.test[5:]] if a.test.startswith('area-') else [])
 if mode in ('demo','editor','world') and not a.test.startswith('area-'):origin=baseline/'world-project'
if a.test=='walking-first':
 command,env,origin=selected_configuration('arena')
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
if not shaders.exists():shaders.symlink_to((baseline if a.baseline else N)/'shared/shaders',target_is_directory=True)
# Current authored arena is copied above, including the prior floor repair.
# Native authoring/story checks write only these private documents and saves.
(private/'saves').mkdir(exist_ok=True)
(private/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
opening=private/'opening'
if opening.is_symlink():opening.unlink();shutil.copytree(origin/'opening',opening,symlinks=True)
if a.test=='walking-first':(private/'arena.config').unlink(missing_ok=True)
settings=json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())['text']
diagnostic_balanced=False
if diagnostic_balanced:
 settings=settings.replace('shadow_side=4096','shadow_side=2048').replace('reflection_scale=1\n','reflection_scale=0.5\n').replace('foliage_distance=Ultra','foliage_distance=High').replace('foliage_density=Ultra','foliage_density=High')
(private/'graphics.settings').write_text(settings)
env.update(MESA_DEBUG='1',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1',SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
if a.test in ('journey','road'):env['VALWOOD_PROFILE_PHASES']='1'
if a.real_audio:
 if env.get('SDL_AUDIODRIVER')=='dummy':env.pop('SDL_AUDIODRIVER')
else:env['SDL_AUDIODRIVER']='dummy'
npc_fixture=None
swim_fixture=None
frames=1400;width=1280;height=720;timeout=720
if a.test=='apartment':
 (private/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
 env['VALWOOD_OBSERVE_THIRD_PERSON']=str(d/'CAMERA_OBSERVATION.jsonl')
 # Private ordinary-input walkthrough through every authored room and switch,
 # then real bills, checkpoint save/load and TV New Game. No pose writes.
 fixture=E/'docs/equipment-progression/2026-11-08/journey-native-v4/project/opening/qa.walk'
 shutil.copy2(fixture,opening/'qa.walk')
 env.update(VALWOOD_QA_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_CONSOLE_ONLY='1',VALWOOD_QA_OPENING_ROOM_LIGHTS='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(d))
 frames=18000;timeout=1000
elif a.test in ('opening','skip'):
 (private/'.valwood-opening-startup-qa').write_text('VALWOOD_PRIVATE_OPENING_STARTUP_V1\n')
 env.update(VALWOOD_QA_OPENING_PROLOGUE='1',VALWOOD_QA_OPENING_STARTUP_ONLY='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(d),VALWOOD_QA_OPENING_STARTUP_RAW=str(d/'OPENING_RAW.json'))
 if a.test=='opening':env['VALWOOD_QA_OPENING_STARTUP_NATURAL']='1';frames=2000
 else:frames=1200
elif a.test in ('npc-intro','npc-intro-natural'):
 # Shipping introduction settings remain intact. Only a production-serialized
 # unseen arrival checkpoint is seeded; the driver then uses ordinary controls.
 intro=opening/'npc-intro.settings'
 if not intro.is_file():raise ValueError('Shipping NPC introduction settings are required')
 if hashlib.sha256(intro.read_bytes()).digest()!=hashlib.sha256((origin/'opening/npc-intro.settings').read_bytes()).digest():raise ValueError('Private introduction differs from shipping settings')
 (private/'.valwood-npc-intro-qa').write_text('VALWOOD_PRIVATE_NPC_INTRO_V1\n')
 (private/'saves/player.progress').unlink(missing_ok=True)
 helper=N/'tools/write_intro_fixture.cpp';helper_bin=d/'write_intro_fixture'
 helper_command=['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-I'+str(N/'source-v1'),str(helper),'-o',str(helper_bin)]
 subprocess.run(helper_command,check=True)
 subprocess.run([str(helper_bin),str(private)],check=True)
 npc_fixture=dict(production_encoded_checkpoint=True,unseen_arrival_bootstrap=True,live_state_injected=False,
  helper_source=str(helper),helper_source_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(),compile_command=helper_command,
  checkpoint_sha256=hashlib.sha256((private/'opening/npc-intro.qa.seed').read_bytes()).hexdigest(),
  shipping_intro_settings_sha256=hashlib.sha256(intro.read_bytes()).hexdigest(),
  prepared_voice_media_present=(opening/'npc_intro/media.cfg').is_file())
 env.update(VALWOOD_QA_NPC_INTRO='1',VALWOOD_QA_NPC_INTRO_BOUNDED='1',VALWOOD_QA_NPC_INTRO_DIR=str(d))
 if a.test=='npc-intro-natural':env['VALWOOD_QA_NPC_INTRO_NATURAL']='1'
 frames=6000 if a.test=='npc-intro-natural' else 2400;timeout=1200
elif a.test in ('journey','story-swim'):
 fixture=E/'docs/equipment-progression/2026-11-08/journey-native-v4/project/opening'
 for name in ('qa.walk','first-journey.qa.walk'):shutil.copy2(fixture/name,opening/name)
 shutil.copy2(N/'review/fixtures/traffic-aware-first-journey.qa.walk',opening/'first-journey.qa.walk')
 # The retained quest driver predates the optional first-NPC cinematic.
 # Its separate dialogue/cancel assertions require the plain conversation.
 # Only this private rehearsal disables that optional presentation; shipping keeps it.
 intro=opening/'npc-intro.settings'
 if intro.exists():intro.rename(opening/'npc-intro.settings.not-used-by-legacy-quest-driver')
 (private/'.valwood-story-swim-qa').write_text('VALWOOD_PRIVATE_STORY_SWIM_V1\n')
 route_name=a.route_fixture or {'baseline':'story-swim.qa.walk','creek':'story-swim-west-creek.qa.walk','deep':'story-swim-deep.qa.walk'}[a.swim_route]
 route_source=N/'review/fixtures'/route_name
 shutil.copy2(route_source,opening/'story-swim.qa.walk')
 if a.test=='story-swim':
  route_sha=hashlib.sha256(route_source.read_bytes()).hexdigest()
  swim_fixture=dict(kind=a.swim_route,source=str(route_source),source_sha256=route_sha,private_route=str(opening/'story-swim.qa.walk'),private_sha256=hashlib.sha256((opening/'story-swim.qa.walk').read_bytes()).hexdigest(),require_deep_dive=a.require_dive)
  if a.require_dive:env['VALWOOD_QA_STORY_SWIM_REQUIRE_DIVE']='1'
 env.update(VALWOOD_QA_FIRST_JOURNEY='1',VALWOOD_QA_OPENING_CAPTURE_DIR=str(d),VALWOOD_QA_FIRST_JOURNEY_CAPTURE_DIR=str(d))
 if a.test=='story-swim':env.update(VALWOOD_QA_STORY_SWIM='1',VALWOOD_QA_STORY_SWIM_ONLY='1')
 else:env['VALWOOD_QA_INVENTORY']='1'
 frames=60000;timeout=1400;width=1600;height=900
elif a.test.startswith('walking-'):
 (private/'.valwood-walking-qa').write_text('VALWOOD_WALKING_QA_V1\n')
 env.update(VALWOOD_PRESENTATION='1',VALWOOD_QA_WALKING_CONTROLS='1',VALWOOD_WALKING_QA_VIEW=a.test[8:],VALWOOD_WALKING_QA_OUTPUT=str(d))
 frames=5000;timeout=600
 if a.stowed:env['VALWOOD_WALKING_QA_STOWED']='1'
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
if a.test_width is not None:width,height=a.test_width,a.test_height
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
launch_binary_sha256=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest() if Path(command[0]).is_file() else None
# Pin the scene documents actually supplied to each new native rehearsal.
scene_paths={origin/name for name in ('world.studio','showcase.flames','opening.config','opening.scene','city.physics','city.scene','project.valwood')}
for folder in ('scenery','opening'):
 for f in (origin/folder).rglob('*'):
  if f.is_file() and f.suffix in ('.txt','.settings','.physics','.config','.walk'):
   scene_paths.add(f)
scene_input_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(scene_paths) if p.is_file()}
shader_input_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(shaders.rglob('*')) if p.is_file()}
if not shader_input_sha256:raise ValueError('Native review requires the actual loaded shader bank')
preparation=dict(harness_source_path=str(harness_copy),harness_source_sha256=harness_sha256,test=a.test,command=command,binary_sha256=launch_binary_sha256,private_project=str(private),
 environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},real_audio_requested=a.real_audio,
 sdl_audio_driver=env.get('SDL_AUDIODRIVER','native-default'),npc_fixture=npc_fixture,swim_fixture=swim_fixture,game_started=False,scene_input_sha256=scene_input_sha256,shader_input_sha256=shader_input_sha256)
(d/'PREPARATION.json').write_text(json.dumps(preparation,indent=2)+'\n')
if a.prepare_only:
 print(json.dumps(preparation,indent=2));sys.exit(0)
if not launch_binary_sha256:raise ValueError('Build the candidate runtime before launching native checks')
timed_out=False
import fcntl
review_lock=os.open(E/'projects/seldonne/.valwood-instance.lock',os.O_RDWR|os.O_CREAT,0o600)
fcntl.flock(review_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
with (d/'native.log').open('w') as log:
 proc=subprocess.Popen(command,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
 while proc.poll() is None:
  if time.monotonic()-start>timeout:
   timed_out=True;proc.kill();proc.wait();log.write('\n[Integration review] TIMEOUT after '+str(timeout)+' seconds\n');break
  if env.get('SDL_VIDEODRIVER')!='offscreen' and a.test in ('apartment','road','walking-first','walking-third','editor','opening','skip','journey','story-swim','npc-intro','npc-intro-natural'):
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
rawname={'opening':'OPENING_RAW.json','skip':'OPENING_RAW.json','arena':'NATIVE_CLASH_RAW.json','arena-orcs':'NATIVE_CLASH_RAW.json','equipment':'EQUIPMENT_NATIVE_RAW.json','joust':'JOUST_NATIVE_RAW.json','road':'ROAD_NATIVE_RAW.json','npc-intro':'NPC_INTRO_NATIVE_RAW.json','npc-intro-natural':'NPC_INTRO_NATIVE_RAW.json'}.get(a.test)
raw=json.loads((d/rawname).read_text()) if rawname and (d/rawname).exists() else {}
logtext=(d/'native.log').read_text()
passed=not timed_out and result.returncode==0 and world.get('gl_error')==0 and 'GL_INVALID' not in logtext and 'Surface library rejected:' not in logtext and 'Safe legacy fallback:' not in logtext and 'Contact requested before explicit loadout transition seed' not in logtext
if a.test not in ('joust','road'):passed=passed and '[skeletal-loadouts] loaded4 purchased rigs' in logtext
if a.test=='scenery' or a.test.startswith('area-'):
 profile=json.loads((d/'performance.json').read_text()) if (d/'performance.json').exists() else {}
 passed=passed and profile.get('samples',0)>100 and (d/'result.bmp').is_file()
if rawname:passed=passed and raw.get('passed') is True
if a.test.startswith('npc-intro'):
 passed=passed and world.get('npc_introduction',{}).get('enabled') is True and raw.get('intro_starts')==1 and raw.get('intro_completions')==1
 if a.test=='npc-intro-natural':passed=passed and raw.get('actual_natural_native_completion') is True and raw.get('actual_skipped') is False
 else:passed=passed and raw.get('actual_skipped') is True and raw.get('actual_natural_native_completion') is False
elif a.test=='apartment':
 qa=world.get('opening_prologue_qa',{});passed=passed and qa.get('passed') is True and 'room_lights_walked_all_rooms' in qa.get('checks',[]) and 'console_only_complete' in qa.get('checks',[])
elif a.test=='journey':passed=passed and world.get('first_journey_qa',{}).get('passed') is True
elif a.test=='story-swim':
 q=world.get('story_swim_qa',{})
 passed=passed and q.get('passed') is True
 if not a.baseline:
  passed=passed and q.get('surface_float_checked')==[True,True] and all(x>=60 for x in q.get('surface_float_samples',[]))
  if 'reflections=1\n' in settings:
   reflection=world.get('water_reflection_demand',{})
   passed=passed and reflection.get('river_planar_sampling') is True and reflection.get('river_reflected_draws',0)>0 and reflection.get('shared_mirror_targets')==1
  if a.require_dive:
   passed=passed and q.get('require_deep_dive') is True and q.get('neutral_depth_hold_checked')==[True,True] and q.get('render_eye_underwater_checked')==[True,True] and q.get('preloan_dive_checked') is True and q.get('loaned_dive_checked') is True
 if swim_fixture:
  passed=passed and hashlib.sha256(Path(swim_fixture['private_route']).read_bytes()).hexdigest()==swim_fixture['private_sha256'] and hashlib.sha256(Path(swim_fixture['source']).read_bytes()).hexdigest()==swim_fixture['source_sha256']
elif a.test.startswith('walking-'):passed=passed and world.get('walking_controls_qa',{}).get('passed') is True
elif a.test=='editor':passed=passed and '[Editor workflows v17 QA] PASS' in (d/'native.log').read_text()
scene_inputs_unchanged=all(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==h for path,h in scene_input_sha256.items() for p in [Path(path)])
shader_inputs_unchanged=all(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==h for path,h in shader_input_sha256.items() for p in [Path(path)])
passed=passed and scene_inputs_unchanged and shader_inputs_unchanged
record=dict(harness_source_path=str(harness_copy),harness_source_sha256=harness_sha256,shader_input_sha256=shader_input_sha256,shader_inputs_unchanged=shader_inputs_unchanged,scene_input_sha256=scene_input_sha256,scene_inputs_unchanged=scene_inputs_unchanged,baseline_comparison=a.baseline,software_renderer=env.get('LIBGL_ALWAYS_SOFTWARE')=='1',timed_out=timed_out,passed=passed,test=a.test,exit_code=result.returncode,elapsed_seconds=time.monotonic()-start,binary_sha256=launch_binary_sha256,command=command,environment={k:v for k,v in env.items() if k.startswith('VALWOOD_')},raw=raw,world=world,private_project=str(private),optional_intro_disabled_only_for_legacy_quest_driver=(a.test in ('journey','story-swim')),input_only_story_swim_scope=(a.test=='story-swim'),traffic_aware_centreline_fixture=(a.test in ('journey','story-swim')),player_only_banks_omitted_for_clearance_diagnostic=(a.test=='road'),compact_scene_for_first_person_controls=(a.test=='walking-first'),private_balanced_graphics=diagnostic_balanced,sdl_video_driver=env.get('SDL_VIDEODRIVER','native-default'),external_window_activation_for_visible_input_tests=(env.get('SDL_VIDEODRIVER')!='offscreen' and a.test in ('apartment','road','walking-first','walking-third','editor','opening','skip','journey','story-swim','npc-intro','npc-intro-natural')))
if a.stowed:
 passed=passed and world.get('walking_stowed_review',{}).get('requested') is True and world['walking_stowed_review'].get('relaxed_samples',0)>10 and world['walking_stowed_review'].get('relaxed_peak',0)>.5 and world['walking_stowed_review'].get('full_idle_samples',0)>5 and world['walking_stowed_review'].get('full_moving_samples',0)>10
 record['passed']=passed
record.update(stowed_motion_requested=a.stowed,real_audio_requested=a.real_audio,sdl_audio_driver=env.get('SDL_AUDIODRIVER','native-default'),
 audio_quality_certified=False,npc_fixture=npc_fixture,swim_fixture=swim_fixture)
if a.test.startswith('npc-intro'):
 record['npc_intro_scope']=dict(shipping_settings_retained=True,scenario='natural-completion-and-controls' if a.test=='npc-intro-natural' else 'bounded-skip-controls-and-once-only-save',
  natural_timeline_completed=raw.get('actual_natural_native_completion',False),
  controls_window=[1280,720] if a.test=='npc-intro-natural' else [640,360],separate_visual_preview_size=[1280,720],full_resolution_continuous_presentation_certified=False,
  narration_quality_certified=False,source_body_motion_and_audio_quality_not_implied=True)
# Timings contain no per-frame playable/loading/modal labels. Preserve all
# intervals and report sparse checkpoint correlations without inventing a split.
sys.path.insert(0,str(N/'tools'))
from review_frame_pacing import build_report
pacing=build_report(d,world=worldpath)
pacing_path=d/'FRAME_PACING_REVIEW.json';pacing_path.write_text(json.dumps(pacing,indent=2)+'\n')
record['frame_pacing_review']=dict(path=str(pacing_path),sha256=hashlib.sha256(pacing_path.read_bytes()).hexdigest(),
 playable_loading_split_available=pacing['playable_loading_split_available'],normal_playable_frame_pacing_claim_available=False)
(d/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:record[k] for k in ('passed','test','exit_code','elapsed_seconds','raw')},indent=2))
if passed and a.test=='scenery':
 subprocess.run([sys.executable,str(N/'check_story_cpu.py')],check=True)
sys.exit(0 if passed else 1)
