from pathlib import Path
import os,sys,json,shutil,subprocess,hashlib,time
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N));from launch import configuration
D=N/'review/ordinary-duel';D.mkdir(exist_ok=True)
P=Path('/tmp/valwood-third-person-focus-ordinary-20261118');P.mkdir(exist_ok=True)
cmd,env,origin=configuration('arena')
for f in origin.iterdir():
 if f.name in ('saves','settings','studio-layout.ini','world.studio.workspace') or '.recovery' in f.name:continue
 out=P/f.name
 if out.exists() or out.is_symlink():continue
 if f.is_dir():out.symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,out,follow_symlinks=True)
(P/'saves').mkdir(exist_ok=True)
(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
(P/'.valwood-third-person-duel-qa').write_text('VALWOOD_PRIVATE_THIRD_PERSON_DUEL_V1\n')
subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(P),'human'],check=True)
(P/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
(P/'graphics.settings').write_text((origin/'graphics.settings').read_text().replace('auto_quality=1','auto_quality=0').replace('vsync=1','vsync=0'))
fifo=D/'frames.rgb.fifo';fifo.unlink(missing_ok=True);os.mkfifo(fifo)
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1',VALWOOD_QA_DUEL='1',VALWOOD_QA_DUEL_DIR=str(D),VALWOOD_QA_DUEL_PIPE=str(fifo))
cmd[cmd.index('--project')+1]=str(P);cmd[cmd.index('--width')+1]='1280';cmd[cmd.index('--height')+1]='720';cmd+=['--hidden','--frames','7200','--warmup','30','--capture',str(D/'result.bmp'),'--profile',str(D/'performance.json')]
encoder_cmd=['ffmpeg','-nostdin','-v','error','-y','-f','rawvideo','-pixel_format','rgb24','-video_size','640x360','-framerate','60','-i',str(fifo),'-an','-c:v','libx264','-threads','2','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(D/'continuous.mp4')]
start=time.monotonic()
with (D/'encoder.log').open('w') as enc,(D/'native.log').open('w') as out:
 encoder=subprocess.Popen(encoder_cmd,stdout=enc,stderr=subprocess.STDOUT)
 try:
  run=subprocess.run(cmd,cwd=E,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=900);encoder_code=encoder.wait(timeout=30)
 finally:
  if encoder.poll() is None:encoder.terminate();encoder.wait(timeout=10)
  fifo.unlink(missing_ok=True)
raw=json.load(open(D/'DUEL_NATIVE_RAW.json')) if (D/'DUEL_NATIVE_RAW.json').exists() else {}
world=json.load(open(D/'performance.json.world.json')) if (D/'performance.json.world.json').exists() else {}
log=(D/'native.log').read_text();passed=run.returncode==0 and encoder_code==0 and raw.get('passed') is True and world.get('gl_error')==0 and 'Safe legacy fallback:' not in log and world.get('skeletal_hero_contact',{}).get('failures')==0 and world.get('source_camera_collision',{}).get('query_failures')==0 and world.get('arena_camera_policy',{}).get('unsafe_frames')==0
r=dict(passed=passed,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),elapsed_seconds=time.monotonic()-start,exit_code=run.returncode,encoder_code=encoder_code,raw=raw,world=world,private_project=str(P),live_saves_used=False,hardware_fps_certified=False)
(D/'EXECUTION.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:r[k] for k in ['passed','elapsed_seconds','exit_code','raw']},indent=2));sys.exit(0 if passed else 1)
