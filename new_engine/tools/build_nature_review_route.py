#!/usr/bin/env python3
"""Prepare an audited, segmented fly-through QA route; never launches a GPU."""
from pathlib import Path
import argparse,json,math
import numpy as np
from audit_nature_publisher_cameras import Cameras,Scene

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'projects/valwood_showcase'
# Continuous portions of the actual publisher film; its hard cuts are explicit
# loading boundaries. Closed animated doors are approached, never crossed.
CLIPS=[
 ('gate_snow_approach',336.16666,343.0,'snow banks, entrance silhouette and terrain seams'),
 ('barbican_bridge',52.066666,57.3,'bridge stairs, railings and lower gate'),
 ('main_gate_doors',27.433334,33.,'main gate doors, handles and entrance stonework; stop before closed door'),
 ('lower_street',35.833332,43.333332,'street walls, door surrounds and building density; starts beyond closed gate'),
 ('lower_courtyard',89.25,96.,'lower architecture and arcade continuity'),
 ('middle_gateway',112.566666,122.3,'archway, stair transitions and windows'),
 ('middle_street',145.06667,151.85,'middle level floors, walls and door detail'),
 ('upper_market',165.56667,173.21666,'upper market, forge, braziers and courtyard'),
 ('upper_street',184.96666,193.65,'upper houses, stairs and cliff integration'),
 ('temple_front_doors',237.78334,244.51666,'closed doors, facade, buttresses and ground contact'),
 ('great_hall',251.68333,258.5,'great hall pillars, ceiling weather mask and lighting'),
 ('altar_windows',261.41666,269.51666,'altar and stained glass'),
 ('magic_stone_gate',367.93332,377.86667,'secondary entrance, ornament and stone doors'),
 ('wizard_bridge',0.,3.9166667,'wizard bridge and carved lava cliff'),
 ('lava_terrain_edge',406.75,414.5,'lava flow, rocks and nearby terrain edges'),
 ('outer_terrain_edge',491.63333,499.,'far snow terrain, horizon and plain-ground defects')]

def native(shot,project):
    p=shot['source_position'];o=project['origin'];floor=project['floor']
    return [o[0]+p[0]-500,floor+p[1],o[1]+500-p[2],shot['native_yaw_radians'],shot['native_pitch_radians'],math.radians(60)]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--project',type=Path,default=DEFAULT);parser.add_argument('--output',type=Path);parser.add_argument('--audit',action='store_true',default=True);parser.add_argument('--step',type=float,default=.5);args=parser.parse_args()
    project=json.loads((args.project/'publisher_showcase.json').read_text());out=args.output or args.project/'qa/source_review';out.parent.mkdir(parents=True,exist_ok=True)
    cameras=Cameras();scene=Scene()if args.audit else None;nodes=[];clips=[];blocked=[]
    def add_clip(label,shots,purpose):
        start=len(nodes);distance=0.;hits=[]
        for k,shot in enumerate(shots):
            pose=native(shot,project);cut=k==0
            duration=0 if cut else max(1,round((shot['time']-shots[k-1]['time'])*60))
            node={'label':label,'transition_frames':duration,'hold_frames':45 if cut else (30 if k==len(shots)-1 else 0),'capture':int(k==len(shots)-1),'pose':pose,'source_time':shot['time'],'source_position':shot['source_position']}
            if k and scene:
                a=np.array(shots[k-1]['source_position']);b=np.array(shot['source_position']);delta=b-a;length=np.linalg.norm(delta);distance+=float(length)
                if length>.02:
                    d=delta/length;right=np.cross(d,[0,1,0]);right/=max(np.linalg.norm(right),1e-8)
                    for side,up in[(0,0),(.20,0),(-.20,0),(0,.15),(0,-.15)]:
                        origin=a+right*side+np.array([0,up,0]);hit=scene.ray(origin,d,length,near_clip=.01)
                        if hit and hit['distance']<length-.02:
                            hits.append({'time':shot['time'],'offset':[side,up],'hierarchy':hit['hierarchy'],'distance':hit['distance'],'step_length':float(length)})
                            break
            if k and not scene:distance+=float(np.linalg.norm(np.array(shot['source_position'])-np.array(shots[k-1]['source_position'])))
            # An obstructed publisher-camera span becomes an explicit QA cut.
            # It is never falsely counted as a traversable visual segment.
            if hits and hits[-1]['time']==shot['time']:
                node['transition_frames']=0;node['hold_frames']=max(node['hold_frames'],30);node['obstruction_cut']=True
            nodes.append(node)
        clips.append({'label':label,'purpose':purpose,'first_node':start,'last_node':len(nodes)-1,'source_times':[shots[0]['time'],shots[-1]['time']],'sampled_distance_m':distance,'obstruction_cuts':len(hits),'hits':hits})
        blocked.extend(hits);print('ROUTE',label,len(shots),'samples',round(distance,2),'m',len(hits),'blocked spans',flush=True)
    for label,start,end,purpose in CLIPS:
        count=math.ceil((end-start)/args.step);times=np.linspace(start,end,count+1)
        shots=[cameras.at(float(t))for t in times]
        if label=='outer_terrain_edge'and scene:
            for shot in shots:
                terrain=scene.terrain_at(shot['source_position'])
                if terrain and terrain['solid']:
                    shot['source_position'][1]=max(shot['source_position'][1],terrain['height']+2.5)
        add_clip(label,shots,purpose)
    # Source-floor verified residents stand in the central nave. Sweep at eye
    # level in front of the group rather than treating a distant hall as a face QA.
    residents=[]
    for line in(args.project/'showcase.residents').read_text().splitlines():
        if line and not line.startswith('#'):
            values=line.split();residents.append([float(v)for v in values[1:4]])
    first=np.array(residents[0]);target=first+np.array([0,1.20,0]);o=project['origin'];floor=project['floor'];shots=[]
    for k,dx in enumerate(np.linspace(0,.8,9)):
        # The king faces native -Z. Match the verified baseline portrait side;
        # keep the sweep to his right, clear of the nearby resident on his left.
        eye=first+np.array([dx,1.50,-3.5]);d=target-eye;yaw=math.atan2(d[0],-d[2]);pitch=math.atan2(d[1],np.linalg.norm(d[[0,2]]))
        source=[eye[0]-o[0]+500,eye[1]-floor,500-(eye[2]-o[1])]
        shots.append({'time':k*.5,'source_position':source,'native_yaw_radians':yaw,'native_pitch_radians':pitch})
    add_clip('dwarf_residents',shots,'dwarf faces, hands, cloth, scale and floor contact')
    lines=['VALWOOD_SOURCE_REVIEW 1 60','# label transitionFrames holdFrames capture x y z yaw pitch verticalFovRadians']
    for n in nodes:
        lines.append('{} {} {} {} {}'.format(n['label'],n['transition_frames'],n['hold_frames'],n['capture'],' '.join(f'{v:.12g}'for v in n['pose'])))
    out.with_suffix('.route').write_text('\n'.join(lines)+'\n')
    report={'version':1,'kind':'segmented source-derived visual fly-through; not a walking/collision rehearsal','route':str(out.with_suffix('.route')),'source':'publisher-camera-timeline.json corrected infinite offset rotation','project':str(args.project),'hz':60,'nodes':nodes,'segments':clips,'capture_checkpoints':sum(n['capture']for n in nodes),'requested_ticks':len(nodes)+sum(n['transition_frames']+n['hold_frames']for n in nodes),'cpu_geometry_audit':bool(scene),'obstruction_cuts':len(blocked),'cpu_audit_limitations':'Two-sided source geometry and terrain; five camera clearance rays per sampled span, radius.20m, vertical.15m. Alpha cutouts/glass tested conservatively as solid. Dynamic dwarves excluded. Deliberate cuts use native loading and probe recapture.'}
    out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print('ROUTE READY',report['route'],report['requested_ticks'],'max expectedticks',report['capture_checkpoints'],'captures',flush=True)

if __name__=='__main__':main()
