#!/usr/bin/env python3
"""Private normal-input campsite -> real first quest -> village ending review.

Start only when the root reviewer confirms the private game is unlocked at the
campsite after the actual battlefield escape. Uses ordinary walking, E dialogue,
LMB/MMB attacks and RMB guard. Reads real checksummed quest saves; never writes
progress, grants a win, changes difficulty, teleports or skips the NPC cinematic.
Stops on genuine combat death, blocked navigation, missing receipts or ending.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import math
import shlex
import sys
import time
import quest_road_navigation

N=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('camp_normal_inputs',N/'tools/play_battlefield_escape.py')
escape=importlib.util.module_from_spec(spec);spec.loader.exec_module(escape)
DriverError=escape.DriverError


def fingerprint(body,basis=1469598103934665603):
    value=basis
    for byte in body.encode():value=((value^byte)*1099511628211)&((1<<64)-1)
    return value


def quest_receipt(bytes_):
    header,body=bytes_.split('\n',1);parts=header.split()
    if parts[:2]!=['VALWOOD_FIRST_JOURNEY_PROGRESS','1'] or int(parts[2])!=fingerprint(body):
        raise DriverError('The native first-journey checkpoint checksum is invalid.')
    fields=shlex.split(body)
    at=lambda label:int(fields[fields.index(label)+1])
    progress=fields.index('progress')+1;reward=fields.index('reward')+1
    return {'stage':at('stage'),'camp':at('camp'),'progress':[int(x) for x in fields[progress:progress+5]],
            'reward':[int(x) for x in fields[reward:reward+3]],'checksum_verified':True}


def inventory_receipt(bytes_):
    p=shlex.split(bytes_);begin=p.index('items')+1;end=p.index('equipment')
    return {'counts':[int(x) for x in p[begin:end]],'weapon':int(p[end+1]),'receipts':int(p[p.index('receipts')+1])}


def questless_inventory(value):
    # world_player_profile::strip_quest keeps durable belongings/receipts while
    # quest tokens are projected solely from the story checkpoint.
    counts=list(value['counts']);counts[14]=counts[15]=0
    return dict(value,counts=counts)


def simplify_path(points,tolerance=.2):
    if len(points)<3:return points
    a,b=points[0],points[-1];dx=b[0]-a[0];dz=b[2]-a[2];length=dx*dx+dz*dz
    best=-1.;index=0
    for i,p in enumerate(points[1:-1],1):
        t=0. if length==0 else max(0.,min(1.,((p[0]-a[0])*dx+(p[2]-a[2])*dz)/length))
        distance=math.hypot(p[0]-a[0]-t*dx,p[2]-a[2]-t*dz)
        if distance>best:best=distance;index=i
    if best<=tolerance:return [a,b]
    return simplify_path(points[:index+1],tolerance)[:-1]+simplify_path(points[index:],tolerance)


class CampQuestReview(escape.EscapeDriver):
    def __init__(self,args):
        super().__init__(args)
        self.paths=json.loads((self.project/'opening/first-journey.routes.json').read_text())['paths']
        self.quest_positions={}
        for line in (self.project/'opening/first-journey.settings').read_text().splitlines():
            p=line.split()
            if p and p[0] in ('camp','rowan','bram','encounter','supplies','retry','town'):self.quest_positions[p[0]]=[float(x) for x in p[1:4]]
        self.reviewed_travelers=[];self.quest_snapshots=[];self.quest_fighting=False
        self.raider_attack_count=0;self.raider_real_victory=False;self.ending_seen=False
        self.reward_profile=None
        self.npc_interaction_target=None

    def route_stop_requested(self,row):
        p=self.npc_interaction_target
        if p is None:return False
        root=row['root'];distance=math.hypot(p[0]-root[0],p[2]-root[2])
        # Radius alone can stop between Bram and Ellis with Bram a few
        # centimetres closer, so E opens the wrong traveler's repeat dialogue.
        others=(self.quest_positions['rowan'],self.quest_positions['bram'],[-11987.9,11.8,5168.8])
        return distance<2.6 and all(distance+.15<math.hypot(q[0]-root[0],q[2]-root[2])
            for q in others if math.hypot(q[0]-p[0],q[2]-p[2])>.01)

    def require_live(self):
        row=self.observe()
        if row.get('_stage')!='campsite':raise DriverError('Camp quest continuation requires completed battlefield arrival at Campsite.')
        if float(row.get('health',100))<=0:raise DriverError('The real player died; leaving evidence for review instead of granting a victory.')
        return row

    def aligned(self,target):
        if self.quest_fighting or getattr(self.args,'keyboard_route',False):return
        return super().aligned(target)

    def snapshot(self):
        parts=shlex.split((self.project/'saves/opening.progress').read_text())
        quest=quest_receipt(parts[parts.index('first_journey')+1])
        mask=int(parts[parts.index('camp_spoken')+1])
        if quest['camp']!=mask:raise DriverError('Camp dialogue credits disagree with the real quest checkpoint.')
        quest.update(camp=mask,npc_intro_seen=('npc_intro_seen' in parts and parts[parts.index('npc_intro_seen')+1]=='1'))
        quest['inventory']=inventory_receipt(parts[parts.index('inventory')+1])
        return quest

    def record_checkpoint(self,label):
        row=self.snapshot();self.quest_snapshots.append({'label':label,**row});self.log('native_quest_receipt',label=label,receipt=row);return row

    def profile(self):
        payload=(self.project/'saves/player.progress').read_text()
        body,separator,tail=payload.rpartition('checksum ')
        try:valid=bool(separator) and int(tail.strip(),16)==fingerprint(body,14695981039346656037)
        except ValueError:valid=False
        if not valid:raise DriverError('The native durable player profile checksum is invalid.')
        p=shlex.split(payload)
        return {'story_coins':int(p[p.index('story')+1]),'inventory':inventory_receipt(p[p.index('inventory')+1])}

    def wait_predicate(self,predicate,label,timeout=8):
        end=min(self.deadline,time.monotonic()+timeout)
        while time.monotonic()<end:
            row=self.require_live();saved=self.snapshot()
            if predicate(saved):return saved
            self.playing(row);self.sleep(.07)
        raise DriverError('Timed out waiting for real quest receipt: '+label)

    def press_interact(self):
        # Longer than the old100ms fixture tap, including a genuine release,
        # so ordinary unbuffered camp/quest interactions survive slow frames.
        self.tap('e',.30);self.sleep(.30)

    def walk(self,point,tolerance=.35):
        row=self.require_live();distance=math.hypot(point[0]-row['root'][0],point[2]-row['root'][2])
        if distance<=tolerance:return
        sprint=self.args.sprint and distance>10.
        if sprint:self.key('Shift_L',True)
        try:self.travel((point[0],point[2]),tolerance=tolerance,timeout=min(90,max(15,distance/1.4+12)))
        finally:
            if sprint:self.key('Shift_L',False)

    def route(self,name):
        if name in ('return','town'):
            route=quest_road_navigation.navigation(self.project,name,self.require_live()['root'])
            self.log('actual_road_center_route',**route)
            for point in route['waypoints']:self.walk(point,tolerance=.12)
            return
        authored=self.paths[name];root=self.require_live()['root']
        nearest=min(range(len(authored)),key=lambda i:math.hypot(authored[i][0]-root[0],authored[i][2]-root[2]))
        points=simplify_path(authored[nearest:])
        self.log('authored_quest_route',name=name,authored_points=len(self.paths[name]),navigation_points=len(points),maximum_horizontal_deviation=.2)
        for point in points:self.walk(point,tolerance=.45)

    def camp_route(self,name):
        receipt=json.loads((N/'review/battlefield_camp_talk_routes.json').read_text())
        points=receipt['routes'][name]['short_input_waypoints_xz']
        if name=='bram_to_ellis':
            # Native normal-input review found a cooking crate blocking the
            # proposed east/north detour at(-11985.60,5169.17). Keep the audited
            # corridor, then step west before rounding its northern corner.
            corner=next(i for i,p in enumerate(points) if p[0]<=-11985.5 and p[1]<=5169.3)
            points=points[:corner+1]+[[-11986.2,5169.25]]+points[corner+1:]
        row=self.require_live();nearest=min(range(len(points)),key=lambda i:math.hypot(points[i][0]-row['root'][0],points[i][1]-row['root'][2]))
        self.log('audited_camp_walk',name=name,waypoints=len(points),start_index=nearest)
        npc=self.quest_positions['bram'] if name.endswith('bram') else [-11987.9,11.8,5168.8] if name.endswith('ellis') else None
        self.npc_interaction_target=npc
        try:
            for x,z in points[nearest:]:
                root=self.require_live()['root']
                if self.route_stop_requested({'root':root}):
                    self.log('within_normal_npc_interaction_radius',route=name,root=root);return
                self.walk([x,11.8,z],tolerance=.25)
        finally:self.npc_interaction_target=None

    def talk_traveler(self,actor,point,approach):
        bit=1<<actor
        if self.snapshot()['camp']&bit:return
        target=('rowan','bram','camp')[actor]
        if actor==2:aim=(-11987.9,5168.8)
        else:aim=(self.quest_positions[target][0],self.quest_positions[target][2])
        root=self.require_live()['root']
        if math.hypot(aim[0]-root[0],aim[1]-root[2])>=2.6:
            for p in approach:self.walk(p)
            self.walk(point,tolerance=.15)
        self.aligned(aim);self.shot(f'camp-{actor+1}-before-talk')
        intro_needed=not self.snapshot()['npc_intro_seen'] and self.snapshot()['camp']==0
        self.press_interact()
        if intro_needed:
            # The36-second authored introduction runs normally. Do not send a
            # skip/advance key until its actual persisted completion receipt.
            self.wait_predicate(lambda q:q['npc_intro_seen'],'NPC cinematic completed normally',timeout=150)
            self.shot('camp-npc-introduction-finished');self.sleep(.35)
        for _ in range(12):
            if self.snapshot()['camp']&bit:break
            self.press_interact()
        self.wait_predicate(lambda q:bool(q['camp']&bit),'traveler dialogue credit')
        self.reviewed_travelers.append(actor);self.record_checkpoint('traveler-'+str(actor));self.shot(f'camp-{actor+1}-talk-complete');self.sleep(.40)

    def leave_cooking_corner(self):
        # Return around the real cooking crate through the already traversed
        # corridor. The old eastward shortcut crossed its solid north corner.
        # These are ordinary walk targets; no collision or game state changes.
        root=self.require_live()['root']
        if root[2]<5173.5 and root[0]<-11984.5:
            receipt=json.loads((N/'review/battlefield_camp_talk_routes.json').read_text())
            points=list(reversed(receipt['routes']['bram_to_ellis']['short_input_waypoints_xz']))
            nearest=min(range(len(points)),key=lambda i:math.hypot(points[i][0]-root[0],points[i][1]-root[2]))
            self.log('return_around_cooking_crate',start_index=nearest,waypoints=points[nearest:])
            for x,z in points[nearest:]:self.walk([x,11.8,z],tolerance=.15)
        self.walk([-11983.75,11.8,5174.],tolerance=.2)

    def complete_dialogue(self,next_stage,label,maximum=14):
        for _ in range(maximum):
            if self.snapshot()['stage']>=next_stage:break
            self.press_interact()
        self.wait_predicate(lambda q:q['stage']>=next_stage,label)
        self.record_checkpoint(label);self.shot(label);self.sleep(.4)

    def fight_raider(self):
        if self.snapshot()['stage']>=4:return
        # Keep the authored route. Face the real opponent before entering its
        # trigger, while ordinary exploration mouse look still owns the camera.
        route=quest_road_navigation.navigation(self.project,'encounter',self.require_live()['root'])
        self.log('actual_road_center_route',**route)
        points=route['waypoints']
        for point in points[:-1]:self.walk(point,tolerance=.12)
        if len(points)>1:
            a,b=points[-2:];length=math.hypot(b[0]-a[0],b[2]-a[2])
            if length>12:
                t=1.-9./length
                self.walk([a[i]+(b[i]-a[i])*t for i in range(3)],tolerance=.35)
        target=self.quest_positions['encounter']
        # Keyboard-only travel never clicks to restore exploration capture
        # after a dialogue. Use the same normal click as EscapeDriver.travel
        # before attempting relative look, while still outside the trigger.
        self.click(1,.15);self.sleep(.35)
        escape.EscapeDriver.aligned(self,(target[0],target[2]))
        self.walk(points[-1],tolerance=.45)
        before=int(self.frames.read().get('attack_serial',0))
        self.shot('raider-before-real-encounter');self.press_interact();self.quest_fighting=True
        self.tap('Up',.08);self.button(3,True);self.guard_inputs+=1
        close_key='w'
        if getattr(self.args,'keyboard_route',False):
            row=self.require_live();target=self.quest_positions['encounter']
            close_key=escape.movement_key(target[0]-row['root'][0],target[2]-row['root'][2],float(row['yaw']))
        self.key(close_key,True)
        try:self.sleep(.70)
        finally:self.key(close_key,False)
        self.sleep(2.1);self.button(3,False)
        deadline=min(self.deadline,time.monotonic()+65);attempts=0
        try:
            while time.monotonic()<deadline:
                row=self.require_live()
                if self.snapshot()['stage']>=4:break
                if not self.playing(row):self.sleep(.1);continue
                direction=('Up','Left','Right','Down','t')[attempts%5]
                self.button(3,False);self.tap(direction,.09)
                if direction=='t':self.click(2,.10);self.thrust_inputs+=1
                else:self.click(1,.10);self.attack_inputs+=1
                attempts+=1;self.raider_attack_count+=1;self.sleep(.65)
                if self.snapshot()['stage']>=4:break
                self.tap('Up' if attempts%2 else 'Left',.08);self.button(3,True);self.guard_inputs+=1;self.sleep(.35);self.button(3,False)
                if attempts%5==0:self.sleep(1.0)
            else:raise DriverError('Real raider combat did not produce the checksummed EncounterWon receipt within65 seconds.')
        finally:self.button(3,False);self.quest_fighting=False
        end=min(self.deadline,time.monotonic()+2)
        while int(self.frames.read().get('attack_serial',0))<=before and time.monotonic()<end:self.sleep(.08)
        if int(self.frames.read().get('attack_serial',0))<=before:raise DriverError('The winning raider encounter contains no actual player attack serial advance.')
        self.raider_real_victory=True;self.record_checkpoint('raider-actual-victory');self.shot('raider-actual-victory');self.sleep(2.5)

    def verify_ending(self):
        row=self.require_live();saved=self.snapshot();profile=self.profile();inv=saved['inventory']
        if saved['stage']!=7 or saved['camp']!=7 or saved['progress']!=[1,1,1,1,1] or saved['reward']!=[12,1,1]:
            raise DriverError('The final checksummed quest receipt does not contain the real complete journey.')
        if profile['story_coins']!=12 or inv['counts'][1]!=1 or inv['counts'][11]!=1 or inv['counts'][14]!=0 or inv['counts'][15]!=1 or inv['receipts']&29!=29:
            raise DriverError('Final sword/bread/satchel/letter/12-copper reward receipts are inconsistent.')
        if profile['inventory']!=questless_inventory(inv):
            raise DriverError('Durable non-quest belongings/receipts disagree with the completed story inventory.')
        if self.reward_profile and self.reward_profile['story_coins']!=profile['story_coins']:
            raise DriverError('The story reward was duplicated or lost between Rowan and the ending.')
        if row.get('screen')!=5:raise DriverError('Complete quest did not display the actual Thanks for Playing screen.')
        self.ending_seen=True;self.receipts={'opening':saved,'durable_profile':profile};self.shot('thanks-for-playing-real-ending')

    def run(self):
        self.setup_controls();self.require_live();self.start_recording();self.record_checkpoint('camp-start')
        stage=self.snapshot()['stage']
        if stage==0:
            if self.require_live()['root'][0]<-11993.:
                # Use the same genuine ground corridor as the bridge review.
                # Complete it before approaching the first conversation.
                corridor=json.loads((N/'review/battlefield_camp_walk_route.json').read_text())['short_input_waypoints_xz']
                row=self.require_live();nearest=min(range(len(corridor)),key=lambda i:math.hypot(corridor[i][0]-row['root'][0],corridor[i][1]-row['root'][2]))
                for x,z in corridor[nearest:]:self.walk([x,11.8,z],tolerance=.22)
            self.talk_traveler(0,[-11991.8,11.8,5172.8],[])
            if not self.snapshot()['camp']&2:self.camp_route('rowan_to_bram')
            self.talk_traveler(1,[-11984.25,11.8,5173.25],[])
            if not self.snapshot()['camp']&4:self.camp_route('bram_to_ellis')
            self.talk_traveler(2,[-11988.,11.8,5166.8],[])
            self.wait_predicate(lambda q:q['stage']>=1,'all three travelers met')
        if self.snapshot()['stage']==1:
            root=self.require_live()['root'];bram=self.quest_positions['bram']
            if math.hypot(bram[0]-root[0],bram[2]-root[2])>3.5:
                self.leave_cooking_corner()
            self.aligned((self.quest_positions['bram'][0],self.quest_positions['bram'][2]));self.complete_dialogue(2,'bram-offer-accepted')
            inv=self.snapshot()['inventory']
            if inv['counts'][1]!=1 or inv['receipts']&17!=17:raise DriverError('Bram did not retain the single battlefield sword and mark the loan.')
        if self.snapshot()['stage']==2:
            if self.require_live()['root'][2]<5173.5:
                self.leave_cooking_corner()
            self.fight_raider()
        if self.snapshot()['stage']==4:
            self.route('satchel');self.press_interact()
            self.wait_predicate(lambda q:q['stage']==5 and q['inventory']['counts'][14]==1,'actual provisions pickup');self.record_checkpoint('supplies-recovered');self.shot('supplies-recovered')
        if self.snapshot()['stage']==5:
            self.route('return');self.aligned((self.quest_positions['rowan'][0],self.quest_positions['rowan'][2]));self.complete_dialogue(6,'rowan-provisions-return-and-reward')
            self.reward_profile=self.profile()
            if self.reward_profile['story_coins']!=12:raise DriverError('Rowan did not commit the actual12-copper reward.')
        if self.snapshot()['stage']==6:
            self.route('town');self.aligned((self.quest_positions['town'][0],self.quest_positions['town'][2]));self.complete_dialogue(7,'hugh-village-ending',maximum=8)
        self.verify_ending();self.log('camp_quest_review_complete',receipts=self.receipts)

    def finish(self,error=None):
        code=super().finish(error);path=self.output/'result.json';result=json.loads(path.read_text())
        for key in ('duels','bridge_samples'):result.pop(key,None)
        result.update({'review':'camp_first_quest_normal_inputs','reviewed_travelers':self.reviewed_travelers,
                       'quest_checkpoints':self.quest_snapshots,'raider_attacks':self.raider_attack_count,
                       'raider_real_victory_reviewed':self.raider_real_victory,'thanks_for_playing_observed':self.ending_seen,
                       'saved_progress_written_by_driver':False,'game_state_injected':False})
        path.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'result':str(path),'passed':result['passed'],'error':result['error'],'ending_seen':self.ending_seen},indent=2),flush=True)
        return code


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--review',default=str(N/'review/live-full'))
    p.add_argument('--output',default='camp-quest-driver')
    p.add_argument('--timeout',type=float,default=600)
    p.add_argument('--sensitivity',type=float,default=1.)
    p.add_argument('--sprint',action='store_true',help='Use normal Shift sprint on route walking.')
    p.add_argument('--record',action='store_true')
    p.add_argument('--keyboard-route',action='store_true',help='Use normal WASD route navigation without camera turns or clicks.')
    args=p.parse_args()
    if not 180<=args.timeout<=900:p.error('--timeout must be180–900 seconds')
    if not .3<=args.sensitivity<=3:p.error('--sensitivity must match the normal game setting .3–3')
    if Path(args.output).name!=args.output:p.error('--output must be a simple folder name')
    driver=None
    try:
        driver=CampQuestReview(args);driver.run();return driver.finish()
    except BaseException as exc:
        if driver is not None:return driver.finish(exc)
        print(json.dumps({'passed':False,'error':str(exc),'inputs_sent':False}),file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
