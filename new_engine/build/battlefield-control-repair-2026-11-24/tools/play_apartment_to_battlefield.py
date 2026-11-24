#!/usr/bin/env python3
"""Private fresh-title -> apartment -> TV -> battlefield normal-input review.

Run only after the root reviewer confirms the new `live_review.py full launch`
window is ready and unlocked. Inputs are normal Enter, W, E and relative mouse
look. The driver does not seed checkpoints, teleport, edit files, skip cinematics,
or issue QA commands. It stops at FleeToAmbush, before walking into the duck QTE.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import math
import shlex
import sys
import time

N=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('escape_input_review',N/'tools/play_battlefield_escape.py')
escape=importlib.util.module_from_spec(spec);spec.loader.exec_module(escape)
base=escape.base
DriverError=base.DriverError


def opening_config(path):
    fields={};waypoints=[]
    for raw in Path(path).read_text().splitlines():
        parts=raw.split()
        if not parts:continue
        if parts[0] in ('house_spawn','house_focus','house_exit'):
            fields[parts[0]]=[float(x) for x in parts[1:]]
        elif parts[0]=='house_waypoint':waypoints.append([float(x) for x in parts[1:]])
    if set(fields)!= {'house_spawn','house_focus','house_exit'} or len(waypoints)<2:
        raise DriverError('The authored apartment navigation configuration is incomplete.')
    fields['waypoints']=waypoints
    return fields


def movement_key(dx,dz,yaw):
    forward=dx*math.sin(yaw)-dz*math.cos(yaw)
    right=dx*math.cos(yaw)+dz*math.sin(yaw)
    if abs(forward)>=abs(right):return 'w' if forward>=0 else 's'
    return 'd' if right>=0 else 'a'


class ApartmentDriver(escape.EscapeDriver):
    def __init__(self,args):
        super().__init__(args)
        self.home=opening_config(self.project/'opening.config')
        self.apartment_started=False;self.bills_read=False;self.console_interacted=False
        self.tv_confirmed=False;self.arrival_observed=False;self.apartment_route=[]

    def require_live(self):
        row=self.observe()
        if row.get('_stage')=='dead':raise DriverError('Unexpected battlefield death before the escape review.')
        if row.get('_stage') not in ('inactive','dazed','recover','flee_to_ambush'):
            raise DriverError('Unexpected stage during the apartment/arrival review: '+str(row))
        return row

    def checkpoint(self):
        try:
            parts=shlex.split((self.project/'saves/opening.progress').read_text())
            if parts[:2]!=['VALWOOD_OPENING','1']:return None
            return int(parts[parts.index('checkpoint')+1])
        except (OSError,ValueError,IndexError):return None

    def wait_checkpoint(self,expected,timeout=4):
        end=min(self.deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            self.observe()
            if self.checkpoint()==expected:return
            self.sleep(.04)
        raise DriverError('Normal interaction did not save opening checkpoint '+str(expected))

    def wait_home_controls(self,timeout=80):
        end=min(self.deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            row=self.require_live();view=self.frames.read()
            if row.get('screen')==1 and not row.get('loading',False):
                # This observation flag is false for the authored home shots
                # and true only after normal third-person control is released.
                if view.get('camera_safety_playable') and view.get('third_person') and not view.get('paused'):
                    self.release_all();self.sleep(.20);return
            self.playing(row);self.sleep(.06)
        raise DriverError('The apartment did not release normal third-person controls after its cinematic.')

    def walk_home(self,point,label):
        row=self.require_live()
        if row.get('_stage')!='inactive':raise DriverError('Refusing apartment movement outside the apartment stage.')
        target=(point[0],point[2]);self.log('apartment_walk',target=point,label=label)
        # The apartment interactions are omnidirectional. Its short authored
        # walkway needs no mouse turn: use ordinary camera-relative WASD from
        # the fresh logical yaw, including a sideways step toward the console.
        # This also avoids treating missing synthetic SDL mouse feedback as a
        # failed game interaction when the player is already beside the bills.
        end=min(self.deadline,time.monotonic()+12);held=None
        try:
            while time.monotonic()<end:
                row=self.require_live()
                if not self.playing(row):self.sleep(.05);continue
                dx=target[0]-row['root'][0];dz=target[1]-row['root'][2]
                if math.hypot(dx,dz)<=.10:break
                yaw=float(row.get('yaw',self.home['house_spawn'][3]))
                key=movement_key(dx,dz,yaw)
                if key!=held:
                    if held:self.key(held,False)
                    self.key(key,True);held=key
                self.sleep(.04)
            else:raise DriverError('Normal apartment WASD movement did not reach '+label)
        finally:
            for key in ('w','a','s','d'):self.key(key,False)
        self.sleep(.20)
        row=self.require_live();self.apartment_route.append({'target':point,'actual':row['root'],'label':label})
        if math.hypot(point[0]-row['root'][0],point[2]-row['root'][2])>.32:
            raise DriverError('The real player did not reach the authored apartment waypoint.')

    def begin_apartment(self):
        if self.args.from_house:
            row=self.require_live()
            if row.get('screen')!=1 or row.get('_stage')!='inactive' or self.checkpoint() not in (0,1):
                raise DriverError('--from-house requires an ordinary apartment checkpoint already playing.')
            self.apartment_started=True;return
        if (self.project/'saves/opening.progress').exists():
            raise DriverError('Fresh-title review requires the empty private full-scenario save folder. Use --from-house for an already playing apartment.')
        end=min(self.deadline,time.monotonic()+85)
        while time.monotonic()<end:
            row=self.observe()
            if row.get('screen')==0 and not row.get('loading',False):break
            self.sleep(.05)
        else:raise DriverError('Fresh title is not ready for BEGIN THE JOURNEY.')
        self.shot('00-fresh-title');self.tap('Return',.10)
        # The first durable HouseStart save is committed on the handoff from
        # the complete16-second cinematic. Slow rendering may extend its wall
        # time; the ordinary introduction must not be skipped to meet a15s cap.
        self.wait_checkpoint(0,timeout=80);self.apartment_started=True
        self.log('begin_journey_normal_enter')

    def approach_console(self):
        # A real stop can settle a few centimetres outside the1.7m console
        # radius. Observe the actual feet, then take ordinary short WASD steps;
        # an interaction attempt beside an unavailable prompt proves nothing.
        target=self.home['house_exit']
        for _ in range(12):
            row=self.require_live()
            dx=target[0]-row['root'][0];dz=target[2]-row['root'][2]
            distance=math.hypot(dx,dz)
            if distance<=1.62:
                self.log('console_reachable',distance=distance,root=row['root'])
                return
            key=movement_key(dx,dz,float(row.get('yaw',self.home['house_spawn'][3])))
            self.tap(key,.08);self.sleep(.25)
        raise DriverError('Normal short WASD steps did not bring the console interaction within reach.')

    def verify_arrival(self):
        row=self.require_live()
        if row.get('_stage') not in ('dazed','recover','flee_to_ambush'):
            raise DriverError('The TV did not transport into the actual battlefield sequence.')
        if row.get('sword') or int(row.get('wins',0))!=0:
            raise DriverError('The battlefield arrival incorrectly began with a sword or fabricated wins.')
        if math.hypot(row['root'][0]+12115,row['root'][2]-5150)>.4:
            raise DriverError('The normal TV transport landed away from the authored battlefield arrival.')
        if int(row.get('failures',0))!=0:raise DriverError('Battlefield actors reported animation/render failures on arrival.')
        parts=shlex.split((self.project/'saves/opening.progress').read_text())
        payload=shlex.split(parts[parts.index('battlefield_progress')+1])
        receipt={'opening_checkpoint':int(parts[parts.index('checkpoint')+1]),
                 'battlefield_checkpoint':int(payload[payload.index('checkpoint')+1]),
                 'sword':int(payload[payload.index('sword')+1]),'wins':int(payload[payload.index('wins')+1])}
        if receipt!={'opening_checkpoint':2,'battlefield_checkpoint':0,'sword':0,'wins':0}:
            raise DriverError('Unexpected real transport checkpoint: '+json.dumps(receipt))
        self.receipts=receipt;self.arrival_observed=True

    def run(self):
        self.setup_controls();self.start_recording();self.begin_apartment()
        self.wait_home_controls();self.shot('01-apartment-player-control')
        if self.checkpoint()==0:
            # The same authored clear walkway is used by the apartment route
            # audit. It stops beside the bills rather than entering the table.
            self.walk_home(self.home['waypoints'][1],'bills-side-walkway')
            self.shot('02-bills-before-interaction');self.tap('e',.30);self.sleep(.20)
            self.wait_checkpoint(1);self.bills_read=True;self.shot('03-bills-read')
        elif self.checkpoint()==1:self.bills_read=True
        else:raise DriverError('Unexpected apartment checkpoint before reading the bills.')
        self.walk_home(self.home['waypoints'][-1],'television-walkway')
        self.approach_console()
        self.shot('04-console-before-interaction');self.tap('e',.30);self.sleep(.20)
        self.console_interacted=True;self.sleep(.65);self.shot('05-television-new-game')
        # Enter is the ordinary focused NEW GAME action. Use a fresh press,
        # never the story skip hold, so the actual TV transport plays in full.
        self.tap('Return',.30);self.tv_confirmed=True
        row=self.wait_stage({'dazed','recover','flee_to_ambush'},timeout=100)
        self.verify_arrival();self.shot('06-battlefield-arrival')
        self.release_all()
        # Dazed is saved before cold destination preparation completes. Wait
        # for the actual load and recovery rather than treating it as a hang.
        self.wait_stage('flee_to_ambush',timeout=100)
        self.verify_arrival();self.shot('07-battlefield-normal-control')
        self.log('apartment_to_battlefield_complete',receipts=self.receipts)

    def finish(self,error=None):
        code=super().finish(error)
        path=self.output/'result.json';result=json.loads(path.read_text())
        for key in ('duels','attack_clicks','thrust_clicks','guard_presses','bridge_samples'):result.pop(key,None)
        result.update({'review':'fresh_apartment_to_battlefield_normal_inputs',
                       'apartment_started':self.apartment_started,'bills_read':self.bills_read,
                       'console_interacted':self.console_interacted,'new_game_confirmed':self.tv_confirmed,
                       'battlefield_arrival_observed':self.arrival_observed,'apartment_route':self.apartment_route,
                       'cutscenes_skipped':False,'game_state_injected':False})
        path.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({'result':str(path),'passed':result['passed'],'error':result['error'],'arrival':self.arrival_observed},indent=2),flush=True)
        return code


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--review',default=str(N/'review/live-full'))
    parser.add_argument('--output',default='apartment-to-battlefield-driver')
    parser.add_argument('--timeout',type=float,default=200)
    parser.add_argument('--sensitivity',type=float,default=1.)
    parser.add_argument('--record',action='store_true')
    parser.add_argument('--from-house',action='store_true',help='Continue normal interaction from an already playing private apartment, not the title.')
    args=parser.parse_args()
    if not 80<=args.timeout<=240:parser.error('--timeout must be80–240 seconds')
    if not .3<=args.sensitivity<=3:parser.error('--sensitivity must match the normal game setting .3–3')
    if Path(args.output).name!=args.output:parser.error('--output must be a simple folder name')
    driver=None
    try:
        driver=ApartmentDriver(args);driver.run();return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
