from pathlib import Path
import json,subprocess,sys,time
N=Path(__file__).resolve().parents[1];D=N/'review/desktop';sys.path.insert(0,str(D))
import native_input as n
n.focus();n.click(640,185)
start=time.monotonic()
while time.monotonic()-start<60:
    lines=(D/'OBSERVATION.jsonl').read_text().splitlines()
    ready=False
    for line in reversed(lines[-5:]):
        try:observed=json.loads(line);ready=not observed['paused'];break
        except json.JSONDecodeError:continue
    if ready:break
    time.sleep(.1)
else:raise RuntimeError('Normal arena start did not become playable')
(D/'START_OBSERVATION_FRAME.json').write_text(json.dumps(observed,indent=2)+'\n')
wid,x,y,w,h=n.geometry()
log=(D/'encoder.log').open('w')
enc=subprocess.Popen(['ffmpeg','-nostdin','-v','error','-y','-f','x11grab','-window_id',str(int(wid,16)),
    '-draw_mouse','0','-framerate','30','-video_size',f'{w}x{h}','-i',':0','-t','20','-an',
    '-c:v','libx264','-threads','2','-preset','fast','-crf','21','-pix_fmt','yuv420p',
    '-movflags','+faststart',str(D/'normal-input.mp4')],stdout=log,stderr=subprocess.STDOUT)
actions=[];start=time.monotonic()
def mark(label):
    actions.append(dict(seconds=time.monotonic()-start,action=label));print(label,flush=True)
def hold(key,seconds):
    n.key(key);time.sleep(seconds);n.key(key,False)
try:
    mark('backpedal');hold('s',.8);n.shot('backpedal')
    mark('walk');hold('w',.65)
    mark('run then stop');n.key('Shift_L');hold('w',.7);n.key('Shift_L',False);time.sleep(.25);n.shot('stop')
    mark('strafe left/right');hold('a',.5);hold('d',.5)
    mark('focus');n.button(2);time.sleep(.08);n.button(2,False);time.sleep(.3)
    mark('left/right guards');n.button(3);n.tap('Left');time.sleep(.4);n.shot('left-guard');n.tap('Right');time.sleep(.4);n.shot('right-guard');n.button(3,False)
    mark('dodge');n.tap('space');time.sleep(.6);n.shot('dodge-return')
    mark('left then right attacks');n.tap('Left');n.button(1);time.sleep(.1);n.button(1,False);time.sleep(.65);n.tap('Right');n.button(1);time.sleep(.1);n.button(1,False);time.sleep(1.1)
    mark('stow/draw');n.tap('z');time.sleep(.8);n.shot('stowed');n.tap('z');time.sleep(1.0);n.shot('drawn')
    mark('pause');n.tap('Escape');time.sleep(.3);n.shot('paused')
finally:
    for key in ('w','a','s','d','Shift_L','Left','Right','Up','Down','space','z'):n.key(key,False)
    for button in (1,2,3):n.button(button,False)
    (D/'ACTIONS.json').write_text(json.dumps(actions,indent=2)+'\n')
enc.wait(timeout=25);assert enc.returncode==0
print('Normal unbounded native input sequence complete; game left paused.')
