#!/usr/bin/env python3
"""Bounded private normal-input battlefield QTE review.

Run only after the root reviewer launches tools/live_review.py battlefield launch
and confirms the new private window is ready. This driver uses XTest keys and
normal Continue; it never writes game state, saves, settings, or authored data.
Observer JSON is read-only. Every pressed key is released in finally.
"""
from pathlib import Path
import argparse
import ctypes as C
import importlib.util
import json
import os
import shlex
import signal
import subprocess
import sys
import time

N=Path(__file__).resolve().parents[1]
DEFAULT_REVIEW=N/'review/live-battlefield'
STAGES=('inactive','dazed','recover','flee_to_ambush','duck','tackle','struggle','stab','stand','escape','forest','campsite','dead')
TITLE='Legends of Valwood | A Place in the Watch'

class DriverError(RuntimeError):
    pass

class Driver:
    def __init__(self,args):
        self.args=args
        self.review=Path(args.review).resolve()
        allowed=(N/'review').resolve()
        if not self.review.is_relative_to(allowed):
            raise DriverError('Review must be below this candidate review directory.')
        self.project=self.review/'project'
        marker=self.project/'PRIVATE_CAPTURE_PROJECT.json'
        if not marker.is_file() or json.loads(marker.read_text()).get('private') is not True:
            raise DriverError('Private project marker is required; no inputs sent.')
        if self.project.resolve()==(N.parents[1]/'projects/valwood_current').resolve():
            raise DriverError('Published project is forbidden; no inputs sent.')
        self.pid=int((self.review/'pid').read_text().strip())
        self.check_process()
        self.output=self.review/args.output
        if self.output.exists():
            raise DriverError('Output exists; use a new --output name to preserve prior evidence.')
        self.output.mkdir()
        self.logfile=(self.output/'actions.jsonl').open('w')
        self.trace=self.review/'battlefield.jsonl'
        self.trace_position=max(0,self.trace.stat().st_size-4096) if self.trace.exists() else 0
        self.trace_tail=''
        self.latest={}
        self.history=[]
        self.start=time.monotonic()
        self.deadline=self.start+args.timeout
        self.pressed=set()
        self.recorder=None
        self.record_log=None
        self.last_resume=0.
        self.resumes=0
        self.receipts={}
        self.images=[]
        self.tap_count=0
        self.missed_duck=False
        self.retried=False
        self.ducked=False
        self.stole=False
        self.m=None
        self.last_lock_check=0.

    def log(self,action,**details):
        row={'time':time.time(),'seconds':round(time.monotonic()-self.start,4),'action':action,**details}
        self.logfile.write(json.dumps(row)+'\n');self.logfile.flush()

    def check_process(self):
        try:
            os.kill(self.pid,0)
            command=(Path('/proc')/str(self.pid)/'cmdline').read_bytes().replace(b'\0',b' ').decode(errors='replace')
        except (ProcessLookupError,FileNotFoundError) as exc:
            raise DriverError('Private game process is not live.') from exc
        if str(self.project) not in command or str(N) not in command:
            raise DriverError('PID does not run this candidate/private project; no inputs sent.')

    def desktop_unlocked(self):
        result=subprocess.run(['gdbus','call','--session','--dest','org.cinnamon.ScreenSaver','--object-path','/org/cinnamon/ScreenSaver','--method','org.cinnamon.ScreenSaver.GetActive'],text=True,capture_output=True,timeout=3)
        if result.returncode or result.stdout.strip()!='(false,)':
            raise DriverError('Desktop is locked or its lock state cannot be verified; no further key-down sent.')
        self.last_lock_check=time.monotonic()

    def setup_controls(self):
        self.desktop_unlocked()
        rows=[line for line in subprocess.check_output(['wmctrl','-lp'],text=True).splitlines() if TITLE in line]
        if not rows:
            raise DriverError('No private Valwood window exists yet.')
        # Existing reviewed API chooses the first matching title. Refuse to
        # focus an unrelated user session if more than one window is present.
        if int(rows[0].split()[2])!=self.pid:
            raise DriverError('The first Valwood window is not the private PID; no inputs sent.')
        self.window=int(rows[0].split()[0],16)
        path=N.with_name('camp-props-2026-11-22')/'tools/live_controls.py'
        spec=importlib.util.spec_from_file_location('reviewed_native_controls',path)
        self.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.m)
        self.bounds=self.m.control()
        self.m.X.XGetInputFocus.argtypes=[C.c_void_p,C.POINTER(C.c_ulong),C.POINTER(C.c_int)]
        self.log('controls_ready',pid=self.pid,window=hex(self.window),bounds=self.bounds)
        self.release_all()

    def key(self,name,down):
        if self.m is None:return
        if down:
            self.check_process()
            focused=C.c_ulong();revert=C.c_int()
            self.m.X.XGetInputFocus(self.m.D,C.byref(focused),C.byref(revert))
            if focused.value!=self.window:
                subprocess.run(['wmctrl','-ia',hex(self.window)],check=True)
                time.sleep(.08)
                self.m.X.XGetInputFocus(self.m.D,C.byref(focused),C.byref(revert))
                if focused.value!=self.window:
                    raise DriverError('Private window cannot receive focus; no key-down sent.')
        code=self.m.X.XKeysymToKeycode(self.m.D,self.m.X.XStringToKeysym(name.encode()))
        if not code:raise DriverError('Unknown X11 key: '+name)
        self.m.T.XTestFakeKeyEvent(self.m.D,code,1 if down else 0,0)
        self.m.X.XFlush(self.m.D)
        if down:self.pressed.add(name)
        else:self.pressed.discard(name)
        self.log('key',key=name,down=bool(down))

    def tap(self,name,duration=.10):
        self.key(name,True)
        try:self.sleep(duration)
        finally:self.key(name,False)

    def release_all(self):
        if self.m is None:return
        for name in sorted(self.pressed|{'w','a','s','d','q','f','Return','KP_Enter','Escape','Down','Up','Shift_L','Shift_R','Control_L','Control_R','Alt_L','Alt_R','Tab'}):
            self.key(name,False)
        self.pressed.clear()

    def guard(self):
        if time.monotonic()-self.last_lock_check>.75:self.desktop_unlocked()
        if time.monotonic()>self.deadline:
            raise DriverError('Global input review timeout reached; stopping and releasing keys.')
        self.check_process()

    def sleep(self,seconds):
        until=time.monotonic()+seconds
        while time.monotonic()<until:
            self.guard()
            remaining=until-time.monotonic()
            if remaining<=0:break
            time.sleep(min(.025,remaining))

    def observe(self):
        self.guard()
        if not self.trace.exists():return self.latest
        with self.trace.open() as stream:
            stream.seek(self.trace_position)
            chunk=stream.read()
            self.trace_position=stream.tell()
        self.trace_tail+=chunk
        lines=self.trace_tail.split('\n');self.trace_tail=lines.pop()
        for line in lines:
            if not line.strip():continue
            try:row=json.loads(line)
            except json.JSONDecodeError:continue
            if isinstance(row.get('battlefield'),dict):row={**row,**row['battlefield']}
            stage=row.get('stage',row.get('battlefield_stage',''))
            if isinstance(stage,int) and 0<=stage<len(STAGES):stage=STAGES[stage]
            stage=str(stage).lower().replace(' ','_')
            row['_stage']=stage
            if stage and (not self.latest or stage!=self.latest.get('_stage')):
                self.history.append({'stage':stage,'elapsed':row.get('elapsed',row.get('stage_elapsed')),'seconds':round(time.monotonic()-self.start,4)})
                self.log('stage',observer=row)
            self.latest=row
        return self.latest

    def stage(self):
        return self.observe().get('_stage','')

    def playing(self,row):
        if row.get('loading',False):
            if 'w' in self.pressed:self.key('w',False)
            return False
        screen=row.get('screen',row.get('preview_screen'))
        focused=row.get('focused',True)
        if not focused:
            if 'w' in self.pressed:self.key('w',False)
            subprocess.run(['wmctrl','-ia',hex(self.window)],check=True)
            self.log('refocus')
            self.sleep(.20)
            return False
        if screen==2 or (row.get('paused') and screen not in (0,None)):
            if time.monotonic()-self.last_resume>.7:
                self.resumes+=1
                if self.resumes>3:raise DriverError('Repeated unexpected pauses; refusing to keep pressing inputs.')
                self.release_all();subprocess.run(['wmctrl','-ia',hex(self.window)],check=True);self.sleep(.15)
                self.tap('Escape');self.last_resume=time.monotonic();self.log('resume_pause')
            return False
        if screen not in (1,None):return False
        return not row.get('paused',False)

    def wait_stage(self,wanted,timeout=15,allow_dead=False):
        wanted={wanted} if isinstance(wanted,str) else set(wanted)
        end=min(self.deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            row=self.observe();stage=row.get('_stage','')
            if stage in wanted:return row
            if stage=='dead' and not allow_dead:raise DriverError('Unexpected death while waiting for '+','.join(wanted))
            self.playing(row)
            self.sleep(.025)
        raise DriverError('Timed out waiting for '+','.join(wanted)+'; last observer='+json.dumps(self.latest))

    def shot(self,label):
        # Observer publishes before the rendered buffer; let the matching
        # visible frame reach the window before recording a stage still.
        self.sleep(.12)
        from PIL import ImageGrab
        x,y,w,h=self.bounds
        path=self.output/(label+'.png')
        ImageGrab.grab().crop((x,y,x+w,y+h)).save(path)
        self.images.append(str(path));self.log('screenshot',path=str(path),stage=self.latest.get('_stage'))

    def start_recording(self):
        if not self.args.record:return
        x,y,w,h=self.bounds
        self.record_log=(self.output/'recording.log').open('w')
        path=self.output/'qte-normal-input.mp4'
        self.recorder=subprocess.Popen(['ffmpeg','-nostdin','-v','error','-f','x11grab','-draw_mouse','0','-framerate','24','-video_size',f'{w}x{h}','-i',f'{os.environ.get("DISPLAY",":0")}+{x},{y}','-t','55','-vf','scale=1280:-2','-c:v','libx264','-threads','2','-preset','veryfast','-crf','21','-pix_fmt','yuv420p',str(path)],stdout=self.record_log,stderr=subprocess.STDOUT)
        self.log('recording_started',pid=self.recorder.pid,path=str(path))

    def continue_checkpoint(self):
        self.wait_stage('inactive',timeout=20,allow_dead=True)
        end=min(self.deadline,time.monotonic()+15)
        while time.monotonic()<end:
            row=self.observe()
            if row.get('screen',row.get('preview_screen'))==0 and not row.get('loading',False):break
            self.sleep(.05)
        else:raise DriverError('Title not ready for normal Continue input.')
        self.shot('00-title')
        # BEGIN is the title's default focused control; one normal navigation
        # step selects CONTINUE CHECKPOINT. This uses the same keys as a tester.
        self.tap('Down');self.sleep(.22);self.tap('Return');self.log('continue_checkpoint')
        self.wait_stage('flee_to_ambush',timeout=20)
        self.release_all();self.sleep(.30)
        self.shot('01-before-ambush')

    def wait_playable(self,timeout=45):
        end=min(self.deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            row=self.observe()
            if self.playing(row):return row
            self.sleep(.05)
        raise DriverError('Destination did not become playable: '+json.dumps(self.latest))

    def walk_to_duck(self,label):
        self.wait_playable()
        end=min(self.deadline,time.monotonic()+12)
        moving=False
        try:
            while time.monotonic()<end:
                row=self.observe();stage=row.get('_stage')
                if stage=='duck':break
                if stage=='dead':raise DriverError('Unexpected death before the ambush prompt.')
                if stage!='flee_to_ambush':
                    self.sleep(.025);continue
                if self.playing(row):
                    if not moving:self.key('w',True);moving=True
                elif moving:
                    self.key('w',False);moving=False
                self.sleep(.025)
            else:raise DriverError('Walking forward did not reach ambush within 12 seconds.')
        finally:self.key('w',False)
        self.shot(label)

    def run(self):
        self.setup_controls()
        if self.args.from_playing:
            row=self.observe()
            if row.get('_stage')!='flee_to_ambush':raise DriverError('--from-playing requires the real pre-ambush stage.')
            self.wait_playable();self.shot('01-before-ambush')
        else:
            self.continue_checkpoint();self.wait_playable()
        self.start_recording()
        self.walk_to_duck('02-duck-missed')
        # Intentionally send no defensive input: this must fail through the
        # real timed swing and produce a local retry, not progress automatically.
        self.wait_stage('dead',timeout=6,allow_dead=True)
        self.missed_duck=True;self.shot('03-intentional-death')
        end=time.monotonic()+4
        while time.monotonic()<end:
            row=self.observe();elapsed=float(row.get('elapsed',row.get('stage_elapsed',0)))
            if row.get('can_retry',False) or elapsed>=.90:break
            self.sleep(.025)
        else:raise DriverError('Death never became retryable.')
        self.tap('Return')
        self.wait_stage('flee_to_ambush',timeout=10,allow_dead=True)
        self.retried=True;self.release_all();self.sleep(.30)
        self.walk_to_duck('04-duck-second-attempt')
        end=time.monotonic()+2
        while time.monotonic()<end:
            row=self.observe()
            if row.get('_stage')!='duck':raise DriverError('Duck window ended before the defensive press.')
            if row.get('duck_window',False) or float(row.get('elapsed',row.get('stage_elapsed',0)))>=.38:break
            self.sleep(.015)
        else:raise DriverError('Observer never exposed the duck window.')
        self.tap('q',.12)
        self.wait_stage('tackle',timeout=3)
        self.ducked=True;self.shot('05-tackle')
        self.wait_stage('struggle',timeout=4)
        self.shot('06-struggle')
        for _ in range(18):
            if self.stage()!='struggle':break
            self.tap('f',.10);self.tap_count+=1;self.sleep(.10)
        self.wait_stage('stab',timeout=2)
        self.stole=True;self.shot('07-stab')
        escaped=self.wait_stage('escape',timeout=5)
        if not escaped.get('sword',escaped.get('sword_owned',False)):raise DriverError('Escape observer did not confirm sword ownership.')
        self.shot('08-escape-with-sword')
        self.verify_receipts()
        self.log('completed',taps=self.tap_count,receipts=self.receipts)

    @staticmethod
    def read_inventory(bytes_):
        parts=shlex.split(bytes_)
        if parts[:2]!=['VALWOOD_INVENTORY','1']:raise DriverError('Unrecognized inventory file schema.')
        start=parts.index('items')+1;end=parts.index('equipment')
        counts=[int(v) for v in parts[start:end]]
        return {'sword_count':counts[1],'weapon':int(parts[end+1]),'receipts':int(parts[parts.index('receipts')+1])}

    def verify_receipts(self):
        end=min(self.deadline,time.monotonic()+4)
        last_error=''
        while time.monotonic()<end:
            try:
                opening=shlex.split((self.project/'saves/opening.progress').read_text())
                if opening[:2]!=['VALWOOD_OPENING','1']:raise DriverError('Unexpected opening schema.')
                record=self.read_inventory(opening[opening.index('inventory')+1])
                bf=shlex.split(opening[opening.index('battlefield_progress')+1])
                bf_receipt={'checkpoint':int(bf[bf.index('checkpoint')+1]),'sword':int(bf[bf.index('sword')+1])}
                profile=shlex.split((self.project/'saves/player.progress').read_text())
                durable=self.read_inventory(profile[profile.index('inventory')+1])
                if record['receipts']&16 and record['sword_count']==1 and record['weapon']==1 and durable['receipts']&16 and durable['sword_count']==1 and durable['weapon']==1 and bf_receipt=={'checkpoint':1,'sword':1}:
                    self.receipts={'opening':record,'durable_profile':durable,'battlefield':bf_receipt};return
                last_error=json.dumps({'opening':record,'durable':durable,'battlefield':bf_receipt})
            except (OSError,ValueError,IndexError,DriverError) as exc:last_error=str(exc)
            self.sleep(.05)
        raise DriverError('Escape did not produce the expected real saved sword receipt: '+last_error)

    def finish(self,error=None):
        self.release_all()
        if self.recorder is not None:
            if self.recorder.poll() is None:
                self.recorder.send_signal(signal.SIGINT)
                try:self.recorder.wait(timeout=5)
                except subprocess.TimeoutExpired:self.recorder.kill();self.recorder.wait(timeout=2)
            self.log('recording_stopped',exit_code=self.recorder.returncode)
        if self.record_log:self.record_log.close()
        result={'passed':error is None,'error':str(error) if error else None,'private_project':str(self.project),'pid':self.pid,'seconds':round(time.monotonic()-self.start,3),'normal_physical_inputs_only':True,'intentional_missed_duck_death':self.missed_duck,'local_retry':self.retried,'fresh_duck_success':self.ducked,'tap_struggle_success':self.stole,'f_taps':self.tap_count,'stages':self.history,'receipts':self.receipts,'screenshots':self.images,'last_observer':self.latest,'game_left_running':True}
        (self.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2),flush=True)
        self.logfile.close()
        return 0 if error is None else 1

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--review',default=str(DEFAULT_REVIEW))
    parser.add_argument('--output',default='qte-driver')
    parser.add_argument('--timeout',type=float,default=120)
    parser.add_argument('--record',action='store_true')
    parser.add_argument('--from-playing',action='store_true',help='Start at the already loaded real pre-ambush stage.')
    args=parser.parse_args()
    if not 30<=args.timeout<=120:parser.error('--timeout must be between 30 and 120 seconds')
    if Path(args.output).name!=args.output:parser.error('--output must be a simple folder name')
    driver=None
    try:
        driver=Driver(args)
        driver.run()
        return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr)
        return 1

if __name__=='__main__':raise SystemExit(main())
