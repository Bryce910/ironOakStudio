#!/usr/bin/env python3
"""Real X11 mouse/keyboard input, only against this runner's private game PID."""
from pathlib import Path
import ctypes as c,hashlib,json,os,shutil,subprocess,time
n=Path(__file__).resolve().parent;e=n.parents[1];review=n/'review/setup-input';review.mkdir(parents=True,exist_ok=True)
p=review/'private-project'
if p.exists():raise SystemExit('Retain previous UI review; choose a fresh review directory')
shutil.copytree(n/'arena-project',p,symlinks=True)
a=e/'build/arena-animation-polish-2026-11-15';q=e/'build/cinematic-studio-2026-11-15/equipment-assets'
env={k:v for k,v in os.environ.items() if not k.startswith('VALWOOD_') and k!='LD_PRELOAD'}
env.update(VALWOOD_PRESENTATION='1',VALWOOD_PLAYTEST_SLICE='city',VALWOOD_COMBAT_ARENA='1',VALWOOD_MODULAR_KNIGHT=json.loads((q/'HERO_ASSET.json').read_text())['path'],VALWOOD_MODULAR_KNIGHT_THIRD_PERSON='1',VALWOOD_MODULAR_KNIGHT_SHADER=str(a/'shaders/world_skeletal.vs.glsl'),VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER=str(a/'shaders/rig_first_person.fs.glsl'),VALWOOD_SKELETAL_LOADOUTS=str(q/'loadouts.manifest'))
command=[str(n/'runtime-native-v1/native-bin/rpg'),'--project',str(p),'--play','--scenic','city-walk','--width','1600','--height','900','--frames','6000']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
watched=[n/'arena-project/saves/player.progress',e/'build/demo-final-pass-2026-11-15/arena-project/saves/player.progress'];before={str(p):sha(p) for p in watched}
x=c.CDLL('libX11.so.6');t=c.CDLL('libXtst.so.6');x.XOpenDisplay.restype=c.c_void_p;x.XOpenDisplay.argtypes=[c.c_char_p]
d=x.XOpenDisplay(None)
if not d:raise SystemExit('No actual desktop display')
x.XDefaultRootWindow.argtypes=[c.c_void_p];x.XDefaultRootWindow.restype=c.c_ulong;root=x.XDefaultRootWindow(d)
x.XTranslateCoordinates.argtypes=[c.c_void_p,c.c_ulong,c.c_ulong,c.c_int,c.c_int,c.POINTER(c.c_int),c.POINTER(c.c_int),c.POINTER(c.c_ulong)]
x.XStringToKeysym.argtypes=[c.c_char_p];x.XStringToKeysym.restype=c.c_ulong;x.XKeysymToKeycode.argtypes=[c.c_void_p,c.c_ulong];x.XKeysymToKeycode.restype=c.c_uint
x.XFlush.argtypes=[c.c_void_p]
t.XTestFakeMotionEvent.argtypes=[c.c_void_p,c.c_int,c.c_int,c.c_int,c.c_ulong];t.XTestFakeButtonEvent.argtypes=[c.c_void_p,c.c_uint,c.c_int,c.c_ulong];t.XTestFakeKeyEvent.argtypes=[c.c_void_p,c.c_uint,c.c_int,c.c_ulong]
log=(review/'native.log').open('w');proc=subprocess.Popen(command,cwd=e,env=env,stdout=log,stderr=subprocess.STDOUT);window=None;passed=False;error='';captures=[]
try:
 start=time.monotonic()
 while time.monotonic()-start<60:
  if proc.poll() is not None:raise RuntimeError('Game exited before UI review')
  for line in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines():
   fields=line.split(None,4)
   if len(fields)==5 and fields[2]==str(proc.pid) and 'Valwood' in fields[4]:window=int(fields[0],16);break
  if window and (time.monotonic()-start>15 or '[Arena] Setup ready:' in (review/'native.log').read_text()):break
  time.sleep(.2)
 if not window:raise RuntimeError('Private game window unavailable')
 subprocess.run(['wmctrl','-ia',hex(window)],check=True);time.sleep(1)
 ox=c.c_int();oy=c.c_int();child=c.c_ulong();x.XTranslateCoordinates(d,window,root,0,0,c.byref(ox),c.byref(oy),c.byref(child))
 def click(px,py):
  t.XTestFakeMotionEvent(d,-1,ox.value+px,oy.value+py,0);x.XFlush(d);time.sleep(.12)
  t.XTestFakeButtonEvent(d,1,1,0);x.XFlush(d);time.sleep(.12);t.XTestFakeButtonEvent(d,1,0,0);x.XFlush(d);time.sleep(.3)
 def key(name):
  code=x.XKeysymToKeycode(d,x.XStringToKeysym(name.encode()));t.XTestFakeKeyEvent(d,code,1,0);x.XFlush(d);time.sleep(.1);t.XTestFakeKeyEvent(d,code,0,0);x.XFlush(d);time.sleep(.2)
 def capture(name):
  out=review/(name+'.png');subprocess.run(['ffmpeg','-nostdin','-y','-loglevel','error','-f','x11grab','-window_id',str(window),'-i',os.environ.get('DISPLAY',':0'),'-frames:v','1',str(out)],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,check=True);captures.append(str(out))
 capture('01-easy')
 click(580,367);key('Down');key('Return');capture('02-medium')
 click(580,367);key('Down');key('Return');capture('03-hard')
 for y in (458,513,567):click(808,y)
 capture('04-counts')
 click(600,200)
 deadline=time.monotonic()+75
 profile=p/'saves/player.progress'
 while time.monotonic()<deadline:
  text=profile.read_text()
  if 'selection 24 24' in text and 'challenge 6 3 6 3' in text:break
  if proc.poll() is not None:raise RuntimeError('Game exited before committing selected settings')
  time.sleep(.2)
 else:raise RuntimeError('Real UI selection did not commit24/24/6/Hard: '+profile.read_text())
 capture('05-started')
 if 'ledger 160 2 16' not in profile.read_text():raise RuntimeError('UI start changed the saved purse/reward ledger')
 passed=True
except Exception as exc:error=str(exc)
finally:
 if proc.poll() is None:
  if window:subprocess.run(['wmctrl','-ic',hex(window)],check=False)
  try:proc.wait(timeout=20)
  except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=10)
 log.close()
 after={str(p):sha(p) for p in watched}
 receipt=dict(passed=passed and before==after,error=error,private_pid=proc.pid,window_id=window,command=command,actual_mouse_and_keyboard=True,no_events_or_outcomes_injected=True,difficulty_sequence=['Easy','Medium','Hard'],committed=dict(total=24,simultaneous=24,teammates=6,difficulty='Hard') if passed else None,public_purse_before=before,public_purse_after=after,binary_sha256=sha(n/'runtime-native-v1/native-bin/rpg'),captures=captures)
 (review/'SETUP_INPUT_RAW.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2),flush=True)
raise SystemExit(0 if receipt['passed'] else 1)
