"""Externally capture actual natural NPC shots; never inject input/story state."""
from pathlib import Path
import json,os,subprocess,time
from PIL import ImageGrab
N=Path(__file__).resolve().parents[1];out=N/'review/npc-intro-natural';source=out/'NPC_INTRO_FRAMES.jsonl'
wanted=[5.,15.,25.,33.];records=[];deadline=time.monotonic()+240
while wanted and time.monotonic()<deadline:
 try:
  with source.open('rb') as f:
   f.seek(0,2);size=f.tell();f.seek(max(0,size-32768));lines=f.read().decode().splitlines()
  row=next(json.loads(s) for s in reversed(lines) if s.startswith('{') and s.endswith('}'))
 except (OSError,StopIteration,ValueError):time.sleep(.12);continue
 if not row.get('active') or row.get('visual_preview') or row.get('paused_menu') or not row.get('focused') or row.get('loading') or row.get('phase')!=51 or row.get('time',0)<wanted[0]:time.sleep(.12);continue
 for line in subprocess.check_output(['wmctrl','-lpG'],text=True).splitlines():
  f=line.split(None,9)
  if len(f)<9:continue
  try:exe=os.readlink('/proc/'+f[2]+'/exe')
  except OSError:continue
  if exe!=str(N/'runtime-native-v1/native-bin/rpg'):continue
  x,y,w,h=map(int,f[3:7]);target=wanted.pop(0);path=out/('actual-natural-'+str(int(target))+'s.png')
  ImageGrab.grab(xdisplay=':0').crop((x,y,x+w,y+h)).save(path)
  records.append(dict(path=str(path),requested_after_seconds=target,most_recent_actual_telemetry=row,display_size=[w,h],external_frame_not_exactly_synchronized=True));break
 time.sleep(.12)
out.mkdir(exist_ok=True,parents=True);(out/'EXTERNAL_NATURAL_CAPTURES.json').write_text(json.dumps(dict(captures=records,requested_times=[5,15,25,33],missing_times=wanted,actual_state_only=True,inputs_injected=False,not_a_frame_pacing_certificate=True),indent=2)+'\n')
print('External actual natural captures',len(records),flush=True)
