"""Ordinary river controls from a private authored walking viewpoint; no state injection."""
from pathlib import Path
import ctypes,hashlib,json,os,re,shutil,subprocess,sys,time
from PIL import Image,ImageGrab
N=Path(__file__).resolve().parents[1];E=N.parents[1];sys.path.insert(0,str(N));from launch import configuration
D=N/'review/river';P=D/'private-project';D.mkdir(exist_ok=True);P.mkdir(exist_ok=True)
cmd,env,origin=configuration('world')
for f in origin.iterdir():
 if f.name in ('saves','studio-layout.ini','world.studio.workspace','player-profile.config','showcase.views'):continue
 t=P/f.name
 if t.exists() or t.is_symlink():continue
 if f.is_dir():t.symlink_to(f.resolve(),target_is_directory=True)
 else:shutil.copy2(f,t)
(P/'saves').mkdir(exist_ok=True);(P/'player-profile.config').write_text('VALWOOD_PLAYER_PROFILE 1\npath saves/player.progress\nend\n')
(P/'graphics.settings').write_text(json.loads((N/'review/ACTIVE_SETTINGS.json').read_text())['text'])
# The standard city-walk viewpoint contract starts on the collision-approved
# supporting floor. All subsequent swimming and diving use normal controls.
(P/'showcase.views').write_text('city-walk -12224 8 5250 0 -.08 1.0471975511966\n')
cmd[cmd.index('--project')+1]=str(P);cmd[cmd.index('--scenic')+1]='city-walk'
# --profile / --capture implicitly enable a finite capture mode in this CLI,
# which intentionally disables physical inputs. This check is ordinary play.

env.update(VALWOOD_PRESENTATION='1',SDL_AUDIODRIVER='dummy',MESA_DEBUG='1',SDL_GAMECONTROLLER_IGNORE_DEVICES='0x046d/0xc21d')
x=ctypes.CDLL('libX11.so.6');xt=ctypes.CDLL('libXtst.so.6');x.XOpenDisplay.restype=ctypes.c_void_p;x.XOpenDisplay.argtypes=[ctypes.c_char_p]
x.XStringToKeysym.restype=ctypes.c_ulong;x.XStringToKeysym.argtypes=[ctypes.c_char_p];x.XKeysymToKeycode.restype=ctypes.c_ubyte;x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
x.XFlush.argtypes=[ctypes.c_void_p];x.XCloseDisplay.argtypes=[ctypes.c_void_p];xt.XTestFakeKeyEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
display=x.XOpenDisplay(b':0');assert display
held=set();events=[];wid=None
started=time.monotonic()
with (D/'native.log').open('w') as log:
 proc=subprocess.Popen(['stdbuf','-oL','-eL',*cmd],cwd=E,env=env,stdout=log,stderr=subprocess.STDOUT)
 def focus():
  global wid
  rows=subprocess.run(['wmctrl','-lpG'],capture_output=True,text=True).stdout.splitlines()
  for row in rows:
   fields=row.split()
   if len(fields)>2 and fields[2]==str(proc.pid):wid=fields[0];subprocess.run(['wmctrl','-ia',wid],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);return
 def key(name,down):
  focus();code=x.XKeysymToKeycode(display,x.XStringToKeysym(name.encode()));assert code
  xt.XTestFakeKeyEvent(display,code,int(down),0);x.XFlush(display)
  if down:held.add(name)
  else:held.discard(name)
  events.append(dict(time_s=time.monotonic()-started,key=name,down=down))
 def rest(seconds):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if proc.poll() is not None:raise RuntimeError('Game exited before controls finished')
   focus();time.sleep(.2)
 def tap(name):key(name,True);rest(.12);key(name,False)
 def snap(name):
  focus();rows=subprocess.run(['wmctrl','-lpG'],capture_output=True,text=True).stdout.splitlines()
  for row in rows:
   f=row.split()
   if f[0]==wid:
    a,b,w,h=map(int,f[3:7]);ImageGrab.grab(xdisplay=':0').crop((a,b,a+w,b+h)).save(D/(name+'.png'));return
 try:
  while '[World] Atlas ' not in (D/'native.log').read_text(errors='replace'):
   if proc.poll() is not None or time.monotonic()-started>240:raise RuntimeError('River startup failed')
   time.sleep(.25)
  rest(4);tap('Return');rest(2);tap('v');rest(2);snap('surface-third')
  key('w',True);rest(5);key('w',False);rest(1)
  key('Control_L',True);rest(7);key('Control_L',False);rest(1);snap('dive-third')
  tap('v');rest(2);snap('dive-first')
  key('space',True);rest(7);key('space',False);rest(2);snap('surface-first')
  key('Control_L',True);rest(7);key('Control_L',False)
  rest(3);subprocess.run(['wmctrl','-ic',wid],check=True)
  while proc.poll() is None:
   if time.monotonic()-started>360:raise RuntimeError('Bounded river check timed out')
   focus();time.sleep(.25)
 except Exception:
  for name in list(held):key(name,False)
  proc.terminate();proc.wait(timeout=15);raise
 finally:x.XCloseDisplay(display)
text=(D/'native.log').read_text();world={'gl_error':0 if proc.returncode==0 and '[World] Shutdown validation' not in text else -1};image=D/'result.bmp'
if image.exists():Image.open(image).save(D/'result.png')
m=re.search(r'\[Aquatics\] fish(\d+) spawned(\d+) flee(\d+) entries(\d+) strokes(\d+) bubbles(\d+).*animationLoaded(\d+) active(\d+) mode(\d+) uploads(\d+)',text)
stats=dict(zip(['fish','spawned','fled','entries','strokes','bubbles','animation_loaded','active','mode','uploads'],map(int,m.groups()))) if m else {}
passed=proc.returncode==0 and world.get('gl_error')==0 and stats.get('entries',0)>0 and stats.get('strokes',0)>0 and stats.get('bubbles',0)>0 and stats.get('uploads',0)>0
out=dict(passed=passed,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest(),elapsed_seconds=time.monotonic()-started,command=cmd,exit_code=proc.returncode,stats=stats,events=events,gl_error=world.get('gl_error'),private_save=True,setup='Private standard walking viewpoint placed in the actual northwest great river. No frame-limited capture mode: ordinary production movement/keyboard input. Entry itself is not an on-foot shore approach test.',live_player_state_injected=False,ordinary_X11_keyboard=True,shutdown_GL_validated_by_production_exit_code=True,no_profile_capture_or_frame_limit_flags=True)
(D/'EXECUTION.json').write_text(json.dumps(out,indent=2)+'\n');print(out);assert passed
