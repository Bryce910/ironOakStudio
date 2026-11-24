#!/usr/bin/env python3
"""Private normal-input escape review: three sword duels, bridge, campsite.

Run ONLY after the root reviewer confirms the private window is unlocked and
ready at Escape. Reads observation-only traces; sends real XTest keyboard/mouse
inputs. Never changes difficulty, health, enemy state, receipts, or world files.
WASD uses camera heading atan2(dx,-dz); mouse sensitivity is .0022 rad/pixel at
its default1. While a live sword duel owns the mouse, motion selects cut direction
rather than camera, so this driver aligns before engaging and uses arrow keys,
LMB cut, RMB guard, and middle-mouse thrust during combat.
"""
from pathlib import Path
import argparse
import ctypes as C
import importlib.util
import json
import math
import sys
import time

N=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('private_qte_controls',N/'tools/play_battlefield_qte.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
DriverError=base.DriverError
HOSTILES=((-12070.,5144.),(-12042.,5150.),(-12029.,5156.))
BRIDGE=((-12028.8,5159.68),)+tuple((float(x),5160.+(x+12028.)*.4) for x in range(-12027,-12006))

class FrameTail:
    def __init__(self,path):
        self.path=Path(path);self.position=0;self.tail='';self.latest={};self.seeded=False
    def read(self):
        if not self.path.exists():return self.latest
        # A long-running private review can accumulate hundreds of MB. Start
        # from its recent complete records rather than replaying its whole
        # history, then read only appended bytes on subsequent polls.
        with self.path.open() as stream:
            size=self.path.stat().st_size
            if not self.seeded or size<self.position:
                self.position=max(0,size-65536);self.tail='';self.seeded=True
                stream.seek(self.position)
                if self.position:stream.readline()
            else:stream.seek(self.position)
            self.tail+=stream.read();self.position=stream.tell()
        lines=self.tail.split('\n');self.tail=lines.pop()
        for line in lines:
            try:row=json.loads(line)
            except json.JSONDecodeError:continue
            self.latest=row
        return self.latest

def heading(root,target):return math.atan2(target[0]-root[0],-(target[1]-root[2]))
def angle_delta(a,b):return math.remainder(a-b,math.tau)
def movement_key(dx,dz,yaw):
    forward=dx*math.sin(yaw)-dz*math.cos(yaw);right=dx*math.cos(yaw)+dz*math.sin(yaw)
    if abs(forward)>=abs(right):return 'w' if forward>=0 else 's'
    return 'd' if right>=0 else 'a'

class EscapeDriver(base.Driver):
    def __init__(self,args):
        super().__init__(args)
        self.frames=FrameTail(self.review/'frames.jsonl')
        self.duels=[];self.bridge_samples=[];self.mouse_pressed=set();self.last_bridge_root=None
        self.attack_inputs=0;self.thrust_inputs=0;self.guard_inputs=0
        self.observation_version=0;self.last_observation=None

    def focus_guard(self):
        self.guard()
        focused=C.c_ulong();revert=C.c_int()
        self.m.X.XGetInputFocus(self.m.D,C.byref(focused),C.byref(revert))
        if focused.value!=self.window:raise DriverError('Private game lost focus; no mouse input sent.')

    def button(self,number,down):
        if down:self.focus_guard()
        self.m.T.XTestFakeButtonEvent(self.m.D,number,1 if down else 0,0);self.m.X.XFlush(self.m.D)
        if down:self.mouse_pressed.add(number)
        else:self.mouse_pressed.discard(number)
        self.log('mouse_button',button=number,down=bool(down))

    def click(self,number,duration=.065):
        self.button(number,True)
        try:self.sleep(duration)
        finally:self.button(number,False)

    def relative(self,dx,dy=0):
        self.focus_guard()
        if abs(dx)>600 or abs(dy)>300:raise DriverError('Relative camera motion exceeds review bound.')
        # SDL X11 warp-relative mode ignores XTest's relative-motion request.
        # A normal absolute pointer motion away from the actual window center
        # yields the intended SDL relative delta (native100px -> .22rad).
        x,y,w,h=self.bounds;target_x=x+w//2+int(dx);target_y=y+h//2+int(dy)
        if not (x+3<=target_x<x+w-3 and y+3<=target_y<y+h-3):
            raise DriverError('Camera pointer target is outside the private game viewport.')
        self.m.T.XTestFakeMotionEvent(self.m.D,-1,target_x,target_y,0);self.m.X.XFlush(self.m.D)
        self.log('mouse_motion',dx=int(dx),dy=int(dy))

    def observe(self):
        row=super().observe()
        if row is not self.last_observation:
            self.observation_version+=1;self.last_observation=row
        root=row.get('root')
        if root and -12026.<root[0]<-12010.:
            line_z=5160.+(root[0]+12028.)*.4
            unique=self.last_bridge_root is None or math.dist(root,self.last_bridge_root)>.08
            if abs(root[2]-line_z)<1.2 and unique:
                self.bridge_samples.append({'root':root,'stage':row.get('_stage'),'seconds':round(time.monotonic()-self.start,3)})
                self.last_bridge_root=list(root)
        return row

    def require_live(self):
        row=self.observe()
        if row.get('_stage')=='dead':raise DriverError('The real player died; no wins were fabricated. Retry manually before rerunning.')
        if row.get('_stage') not in ('escape','forest','campsite'):raise DriverError('Expected active escape, forest or campsite; observed '+str(row))
        return row

    def orientation(self,row):
        # New builds flush the logical camera yaw with the battlefield trace.
        # Render-camera samples are a delayed fallback and may arrive in large
        # buffered batches; their frame IDs identify genuine fresh feedback.
        yaw=row.get('yaw',row.get('camera_yaw'))
        if isinstance(yaw,(int,float)) and math.isfinite(yaw):
            return float(yaw),('battlefield',self.observation_version)
        view=self.frames.read();eye=view.get('eye')
        if eye and len(eye)>3 and math.isfinite(float(eye[3])):
            return float(eye[3]),('frames',view.get('frame'))
        return None

    def wait_orientation(self,previous,turned=None,timeout=3.):
        end=min(self.deadline,time.monotonic()+timeout);started=time.monotonic()
        last=previous;seen_turn=False
        while time.monotonic()<end:
            row=self.require_live()
            if not self.playing(row):self.sleep(.05);continue
            sample=self.orientation(row)
            if sample is None or sample[1]==last[1]:self.sleep(.04);continue
            if sample[1][0]!=last[1][0]:
                # Switching to the fresh logical-yaw channel is safe once it
                # has itself supplied a subsequent fresh sample.
                last=sample;self.sleep(.04);continue
            if turned is None:return sample
            if abs(angle_delta(sample[0],previous[0]))>=min(.01,abs(turned)*.25):seen_turn=True
            stable=abs(angle_delta(sample[0],last[0]))<.012
            minimum=.12 if sample[1][0]=='battlefield' else .55
            if seen_turn and stable and time.monotonic()-started>=minimum:return sample
            last=sample;self.sleep(.04)
        raise DriverError('Camera feedback did not become fresh and settled; refusing another correction from stale yaw.')

    def aligned(self,target):
        row=self.require_live()
        if int(row.get('enemy',-1))>=0:return
        fence=int(row.get('frame',0));end=min(self.deadline,time.monotonic()+5.)
        while time.monotonic()<end:
            view=self.frames.read()
            if view.get('third_person') is False:break
            if int(view.get('frame',-1))>=fence and view.get('camera_safety_playable') is True:break
            self.playing(row);self.sleep(.05);row=self.require_live()
        else:raise DriverError('Native camera has not returned to a fresh playable third-person view; no turn sent.')
        sample=None;capture_retry=False
        for _ in range(20):
            row=self.require_live()
            if not self.playing(row):self.sleep(.10);continue
            if int(row.get('enemy',-1))>=0:
                # Ordinary melee owns mouse direction while an enemy is live.
                # Keep the correctly aligned heading established before entry.
                return
            if sample is None:
                sample=self.orientation(row)
                if sample is None:self.sleep(.08);continue
                if sample[1][0]=='frames':sample=self.wait_orientation(sample)
            wanted=heading(row['root'],target);delta=angle_delta(wanted,sample[0])
            if abs(delta)<.045:return
            pixels=max(-180,min(180,round(delta/(.0022*self.args.sensitivity))))
            if pixels:
                self.relative(pixels)
                try:sample=self.wait_orientation(sample,pixels*.0022*self.args.sensitivity)
                except DriverError as exc:
                    if capture_retry or not str(exc).startswith('Camera feedback did not become fresh and settled'):raise
                    # One isolated XTest delta can be consumed by capture's
                    # warp. Re-read actual native yaw and explicitly recapture
                    # once; never accumulate corrections from the old sample.
                    fresh=self.require_live();current=self.orientation(fresh)
                    if current is None or current[1]==sample[1] or not self.playing(fresh):raise
                    capture_retry=True;sample=current
                    self.log('recapture_after_unobserved_motion',yaw=current[0],sample=current[1])
                    self.click(1,.18);self.sleep(.50)
        raise DriverError('Could not align camera using normal relative mouse input.')

    def enemy_distance(self,row,index):
        point=row.get('enemy_root')
        if not isinstance(point,(tuple,list)) or len(point)<3 or int(row.get('enemy',-1))<0:
            point=(HOSTILES[index][0],0,HOSTILES[index][1])
        return math.hypot(float(point[0])-row['root'][0],float(point[2])-row['root'][2])

    def travel(self,target,tolerance=.8,timeout=22,stop_at_camp=False,stop_at_enemy=False):
        if getattr(self.args,'keyboard_route',False):
            return self.travel_keyboard(target,tolerance,timeout,stop_at_camp,stop_at_enemy)
        end=min(self.deadline,time.monotonic()+timeout)
        self.key('w',False)
        # Allow ordinary SDL relative-mode capture to settle before the first
        # synthetic movement. Otherwise that delta can be consumed by the
        # capture warp and the feedback guard correctly rejects a stale yaw.
        self.click(1,.15);self.sleep(.35)
        self.aligned(target)
        last_check=time.monotonic();last_distance=None;last_align=0.
        try:
            while time.monotonic()<end:
                row=self.require_live()
                if row.get('_stage')=='campsite' and stop_at_camp:return row
                if stop_at_enemy and int(row.get('enemy',-1))>=0:return row
                distance=math.hypot(target[0]-row['root'][0],target[1]-row['root'][2])
                if distance<=tolerance:return row
                if not self.playing(row):self.sleep(.08);continue
                if int(row.get('enemy',-1))<0 and time.monotonic()-last_align>.65:
                    self.key('w',False);self.aligned(target);last_align=time.monotonic()
                if 'w' not in self.pressed:self.key('w',True)
                if time.monotonic()-last_check>3:
                    if last_distance is not None and last_distance-distance<.25:
                        self.shot('blocked-'+str(len(self.images)))
                        raise DriverError('Movement/collision made no forward progress toward '+str(target))
                    last_distance=distance;last_check=time.monotonic()
                self.sleep(.055)
        finally:self.key('w',False)
        raise DriverError('Travel timed out toward '+str(target)+'; last='+json.dumps(self.latest))

    def travel_keyboard(self,target,tolerance=.8,timeout=22,stop_at_camp=False,stop_at_enemy=False):
        end=min(self.deadline,time.monotonic()+timeout);held=None
        last_check=time.monotonic();last_distance=None
        try:
            while time.monotonic()<end:
                row=self.require_live()
                if stop_at_camp and row.get('_stage')=='campsite':return row
                if stop_at_enemy and int(row.get('enemy',-1))>=0:return row
                nearby=getattr(self,'route_stop_requested',None)
                if nearby and nearby(row):return row
                dx=target[0]-row['root'][0];dz=target[1]-row['root'][2];distance=math.hypot(dx,dz)
                if distance<=tolerance:return row
                if not self.playing(row):
                    if held:self.key(held,False);held=None
                    self.sleep(.05);continue
                yaw=row.get('yaw')
                if not isinstance(yaw,(int,float)) or not math.isfinite(yaw):raise DriverError('Keyboard-route review requires the fresh logical yaw observer.')
                key=movement_key(dx,dz,yaw)
                if key!=held:
                    if held:self.key(held,False)
                    self.key(key,True);held=key
                if time.monotonic()-last_check>3:
                    if last_distance is not None and last_distance-distance<.25:
                        self.shot('blocked-'+str(len(self.images)));raise DriverError('Keyboard route made no real movement progress toward '+str(target))
                    last_distance=distance;last_check=time.monotonic()
                self.sleep(.04)
        finally:
            for key in ('w','a','s','d'):self.key(key,False)
        raise DriverError('Keyboard route timed out toward '+str(target))

    def face_melee_enemy(self):
        """Use the documented Q+mouse chord; contact remains physical."""
        if not getattr(self.args,'melee_free_look',False):return
        self.button(3,False);self.key('q',True)
        try:
            self.sleep(.12)
            for _ in range(16):
                row=self.require_live()
                if not self.playing(row) or int(row.get('enemy',-1))<0:return
                enemy=row.get('enemy_root')
                if not isinstance(enemy,list) or len(enemy)<3:raise DriverError('No actual enemy root for free-look check.')
                sample=self.orientation(row)
                if sample is None:raise DriverError('No actual logical camera feedback for free look.')
                delta=angle_delta(heading(row['root'],(enemy[0],enemy[2])),sample[0])
                if abs(delta)<.07:
                    self.log('documented_melee_free_look_aligned',yaw=sample[0],enemy=enemy,error_radians=delta)
                    return
                pixels=max(-160,min(160,round(delta/(.0022*self.args.sensitivity))))
                self.relative(pixels);self.wait_orientation(sample,pixels*.0022*self.args.sensitivity)
            raise DriverError('Q+mouse did not align actual combat aim; preserve failure.')
        finally:self.key('q',False)

    def fight(self,index):
        row=self.require_live()
        if int(row.get('wins',0))>index:return
        self.log('approach_soldier',index=index,target=HOSTILES[index])
        self.travel(HOSTILES[index],tolerance=1.95,timeout=45,stop_at_enemy=True)
        end=min(self.deadline,time.monotonic()+45)
        # A rendered observer update can trail the final walking sample. Wait
        # for the real host to own the threat, without creating one ourselves.
        contact_end=min(end,time.monotonic()+2)
        while time.monotonic()<contact_end:
            row=self.require_live()
            if int(row.get('enemy',-1))>=0:break
            self.sleep(.05)
        else:raise DriverError('Approached the soldier but the actual threat did not activate.')
        self.face_melee_enemy()
        self.shot(f'{index+1}a-soldier-activated')
        attacks_before=int(self.frames.read().get('attack_serial',0))
        health_before=float(self.latest.get('health',100))
        enemy_max=float(self.latest.get('enemy_health',90))
        # The foe was activated inside its8.25m leash. Approach using normal
        # movement during its2.5s countdown. Once the moving foe's physical
        # capsule stops us, that is successful contact, not a blocked path to
        # its stale original spawn. The real combat loop owns all later motion.
        self.tap('Up',.045);self.button(3,True);self.guard_inputs+=1
        close_end=min(self.deadline,time.monotonic()+2.)
        last_root=list(self.latest['root']);last_move=time.monotonic()
        try:
            held='w'
            if getattr(self.args,'keyboard_route',False):
                enemy=self.latest.get('enemy_root',[HOSTILES[index][0],0,HOSTILES[index][1]])
                held=movement_key(enemy[0]-self.latest['root'][0],enemy[2]-self.latest['root'][2],float(self.latest['yaw']))
            self.key(held,True)
            while time.monotonic()<close_end:
                row=self.require_live()
                if not self.playing(row):break
                if getattr(self.args,'keyboard_route',False):
                    enemy=row.get('enemy_root',[HOSTILES[index][0],0,HOSTILES[index][1]])
                    key=movement_key(enemy[0]-row['root'][0],enemy[2]-row['root'][2],float(row['yaw']))
                    if key!=held:self.key(held,False);self.key(key,True);held=key
                if math.hypot(row['root'][0]-last_root[0],row['root'][2]-last_root[2])>.15:
                    last_root=list(row['root']);last_move=time.monotonic()
                if self.enemy_distance(row,index)<(1.65 if getattr(self.args,'melee_free_look',False) else 2.4):break
                if time.monotonic()-last_move>.55:break
                self.sleep(.04)
        finally:
            for key in ('w','a','s','d'):self.key(key,False)
        self.sleep(.65);self.button(3,False)
        directions=('Up','Left','Right','Down','t')
        attempts=0;last_damage=time.monotonic();previous_health=float(self.latest.get('enemy_health',90))
        while time.monotonic()<end:
            row=self.require_live()
            wins=int(row.get('wins',0))
            if wins>index:break
            if not self.playing(row):self.sleep(.10);continue
            if int(row.get('enemy',-1))<0:
                raise DriverError('Soldier encounter cancelled unexpectedly; review the threat leash before retrying.')
            current=float(row.get('enemy_health',90))
            if current<previous_health:last_damage=time.monotonic()
            previous_health=current
            if attempts>=12 and time.monotonic()-last_damage>12:
                self.shot(f'{index+1}x-no-contact')
                raise DriverError('Varied physical attacks are not causing body damage; preserve this contact failure for review.')
            if attempts%4==0:self.face_melee_enemy()
            # Vary real cut angles. Middle mouse is the documented physical jab
            # shortcut; all other attacks use normal direction selection+LMB.
            direction=directions[attempts%len(directions)]
            self.button(3,False)
            self.tap(direction,.055)
            if direction=='t':self.click(2);self.thrust_inputs+=1
            else:self.click(1);self.attack_inputs+=1
            attempts+=1
            self.sleep(.62)
            row=self.require_live()
            if int(row.get('wins',0))>index:break
            # Release/press RMB again because a committed cut releases guard.
            # Guard is an actual input, never an injected successful block.
            self.tap('Up' if attempts%2 else 'Left',.045)
            self.button(3,True);self.guard_inputs+=1;self.sleep(.28);self.button(3,False)
            if attempts%5==0:
                # Give the real stamina clock a short unguarded recovery window.
                self.sleep(1.10)
        else:raise DriverError('Real combat did not defeat soldier '+str(index+1)+' in45 seconds.')
        self.button(3,False)
        # frames.jsonl is buffered separately from the per-frame battlefield
        # observer, so allow its normal flush before comparing attack serials.
        serial_end=min(self.deadline,time.monotonic()+.8)
        frame=self.frames.read()
        while int(frame.get('attack_serial',0))<=attacks_before and time.monotonic()<serial_end:
            self.sleep(.05);frame=self.frames.read()
        if int(frame.get('attack_serial',0))<=attacks_before:
            raise DriverError('No authoritative real player attack serial advanced in the winning encounter.')
        self.duels.append({'soldier':index+1,'attempts':attempts,'health_before':health_before,'health_after':self.latest.get('health'),'enemy_initial':enemy_max,'wins':self.latest.get('wins'),'first_attack_serial':attacks_before,'last_attack_serial':frame.get('attack_serial')})
        self.shot(f'{index+1}b-soldier-defeated')
        end=min(self.deadline,time.monotonic()+7)
        while time.monotonic()<end:
            row=self.require_live()
            if int(row.get('enemy',-1))<0:return
            self.sleep(.07)
        raise DriverError('Defeated soldier did not finish the real death/recycle transition.')

    def verify_camp(self):
        row=self.require_live()
        if row.get('_stage')!='campsite' or int(row.get('wins',0))!=3:raise DriverError('Campsite reached without three real combat victories.')
        if not row.get('sword',False):raise DriverError('Recovered sword was lost before the campsite.')
        end=min(self.deadline,time.monotonic()+4)
        while time.monotonic()<end:
            try:
                tokens=__import__('shlex').split((self.project/'saves/opening.progress').read_text())
                payload=__import__('shlex').split(tokens[tokens.index('battlefield_progress')+1])
                cp=int(payload[payload.index('checkpoint')+1]);wins=int(payload[payload.index('wins')+1]);sword=int(payload[payload.index('sword')+1])
                inventory=self.read_inventory(tokens[tokens.index('inventory')+1])
                if cp==3 and wins==3 and sword==1 and inventory['receipts']&16 and inventory['sword_count']==1:
                    self.receipts={'battlefield_checkpoint':cp,'real_wins':wins,'sword':sword,'inventory':inventory};break
            except (OSError,ValueError,IndexError):pass
            self.sleep(.05)
        else:raise DriverError('Real campsite checkpoint was not saved correctly.')
        if len(self.bridge_samples)<3:raise DriverError('Insufficient observed player samples across the actual bridge.')
        if max(x['root'][0] for x in self.bridge_samples)-min(x['root'][0] for x in self.bridge_samples)<10.:
            raise DriverError('Observed bridge samples do not cover a continuous crossing.')
        if min(x['root'][1] for x in self.bridge_samples)<11.95:raise DriverError('Player fell below the bridge deck during the crossing.')

    def run(self):
        self.setup_controls();row=self.require_live()
        continuation=getattr(self.args,'continue_route',False)
        if continuation:
            if row.get('_stage') not in ('forest','campsite') or int(row.get('wins',0))!=3:
                raise DriverError('Route continuation requires observed Forest/Campsite and three actual completed wins.')
        elif row.get('_stage')!='escape':raise DriverError('Start this driver at the real Escape stage after the QTE review.')
        self.start_recording();self.shot('00-escape-start')
        for index in range(3):
            # The large visible rock on the first approach is a real obstacle.
            # Walk its clear north side instead of asking a greedy cardinal
            # navigator to walk straight through its legitimate collider.
            if index==0 and not int(self.require_live().get('wins',0)) and self.latest['root'][0]<-12078.:
                self.travel((-12085.,5155.5),tolerance=.35,timeout=20)
                self.travel((-12074.8,5155.5),tolerance=.35,timeout=15)
            self.fight(index)
        bridge=BRIDGE
        if continuation:
            root=self.require_live()['root']
            closest=min(range(len(bridge)),key=lambda i:math.hypot(bridge[i][0]-root[0],bridge[i][1]-root[2]))
            bridge=bridge[closest+1:]
        for index,target in enumerate(bridge):
            self.travel(target,tolerance=.22,timeout=15)
            if index==10:self.shot('04-bridge-crossing')
        # The campsite has real roots, seating logs and a southern rock.
        # Follow the source-geometry-audited walking corridor instead of
        # cutting through those props or climbing onto the rock.
        camp_route=json.loads((N/'review/battlefield_camp_walk_route.json').read_text())['short_input_waypoints_xz']
        for point in camp_route:self.travel(tuple(point),tolerance=.22,timeout=15)
        self.wait_stage('campsite',timeout=4)
        self.shot('05-campsite-after-battle')
        self.verify_camp();self.log('escape_completed',duels=self.duels,receipts=self.receipts)

    def finish(self,error=None):
        if self.m is not None:
            for button in tuple(self.mouse_pressed):self.button(button,False)
        code=super().finish(error)
        path=self.output/'result.json';result=json.loads(path.read_text())
        for key in ('intentional_missed_duck_death','local_retry','fresh_duck_success','tap_struggle_success','f_taps'):result.pop(key,None)
        result.update({'review':'battlefield_escape_normal_inputs','duels':self.duels,'attack_clicks':self.attack_inputs,'thrust_clicks':self.thrust_inputs,'guard_presses':self.guard_inputs,'bridge_samples':self.bridge_samples})
        path.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':str(path),'passed':result['passed'],'error':result['error'],'actual_duels':len(self.duels),'bridge_samples':len(self.bridge_samples)},indent=2),flush=True)
        return code

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--review',default=str(N/'review/live-battlefield'))
    parser.add_argument('--output',default='escape-driver')
    parser.add_argument('--timeout',type=float,default=180)
    parser.add_argument('--sensitivity',type=float,default=1.)
    parser.add_argument('--record',action='store_true')
    parser.add_argument('--continue-route',action='store_true',help='Continue real Forest/Campsite traversal after three observed actual victories; never grants wins or writes saves.')
    parser.add_argument('--keyboard-route',action='store_true',help='Navigate using ordinary camera-relative WASD and fresh yaw; send no route camera turns or clicks.')
    parser.add_argument('--melee-free-look',action='store_true',help='Test documented Q+mouse melee free look and approach actual blade range; never injects contact, damage or wins.')
    args=parser.parse_args()
    if not 60<=args.timeout<=240:parser.error('--timeout must be60–240 seconds')
    if not .3<=args.sensitivity<=3:parser.error('--sensitivity must match the normal game setting .3–3')
    if Path(args.output).name!=args.output:parser.error('--output must be a simple folder name')
    driver=None
    try:
        driver=EscapeDriver(args);driver.run();return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
