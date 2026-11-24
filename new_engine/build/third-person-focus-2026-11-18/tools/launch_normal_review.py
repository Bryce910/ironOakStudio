from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N))
from launch import configuration
D=N/'review/desktop';D.mkdir(parents=True,exist_ok=True)
P=Path('/tmp/valwood-final-native-controls-20261118');P.mkdir(exist_ok=True)
cmd,env,origin=configuration('arena')
for f in origin.iterdir():
    if f.name in ('saves','settings','studio-layout.ini','world.studio.workspace') or '.recovery' in f.name:continue
    target=P/f.name
    if target.exists() or target.is_symlink():continue
    if f.is_dir():target.symlink_to(f.resolve(),target_is_directory=True)
    else:shutil.copy2(f,target,follow_symlinks=True)
(P/'saves').mkdir(exist_ok=True)
(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
(P/'PRIVATE_CAPTURE_PROJECT.json').write_text('{"private_profiles_only":true}\n')
(P/'arena_player_character.cfg').write_text('VALWOOD_ARENA_PLAYER_CHARACTER 1\nplayer knight09\nend\n')
subprocess.run([str(E/'build/arena-character-roster-2026-11-14/roster-review/write_capture_profile'),str(P),'human'],check=True)
cmd[cmd.index('--project')+1]=str(P)
cmd[cmd.index('--width')+1]='1280';cmd[cmd.index('--height')+1]='720'
# Do not pass --profile: the CLI intentionally selects bounded frame mode for
# profiling. This review needs the ordinary unbounded input and real clock.
env.update(SDL_VIDEODRIVER='x11',MESA_DEBUG='1',VALWOOD_OBSERVE_THIRD_PERSON=str(D/'OBSERVATION.jsonl'))
# Isolate this keyboard/mouse check from the connected physical F310. Earlier
# virtual-pad combat tests separately cover controller actions and directions.
env['SDL_GAMECONTROLLER_IGNORE_DEVICES']='0x046d/0xc21d'
env.pop('SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS',None)
log=(D/'native.log').open('w')
proc=subprocess.Popen(cmd,cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
(D/'SESSION.json').write_text(json.dumps(dict(pid=proc.pid,command=cmd,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),
    qa_enabled=False,frames_mode=False,real_time=True,private_project=str(P),live_saves_used=False,
    physical_gamepad_excluded_from_keyboard_mouse_check=True,observation_does_not_inject_inputs=True,started=time.time()),indent=2)+'\n')
print('Ordinary private desktop session',proc.pid)
