"""Five readable axe guards solved on the actual PowerOrc14 arm and edge.

Targets describe the visible forged edge, never a hidden collision offset.
The immutable source axe, hand-local grip, fixed arm lengths and finger poses
are retained. Both orientations of the same undirected edge are considered.
"""
import math
import numpy as np
from mathutils import Matrix,Vector,Quaternion

TARGETS={
    'left':((.35,1.075,.45),(0.,1.,0.)),
    'right':((-.30,1.825,.60),(0.,1.,0.)),
    'high':((0.,1.86,.58),(1.,0.,0.)),
    'low':((.095,.975,.70),(0.,1.,0.)),
    'thrust':((.065,1.175,.30),(1.,0.,0.)),
}

def fit(neutral,bones,index,hand_local,edge_local,scale,origin,basis,ik_pose):
    hand_index=index['hand_r'];hand=neutral[hand_index]
    hp,hq,hs=hand.decompose()
    mid=(hand_local@edge_local[0]+hand_local@edge_local[1])*.5
    world_edges=[hand@hand_local@v for v in edge_local]
    old_axis=(world_edges[1]-world_edges[0]).normalized()
    shoulder=neutral[index['upperarm_r']].translation
    elbow=neutral[index['lowerarm_r']].translation
    reach=(elbow-shoulder).length+(hp-elbow).length
    poses={};records={}
    for direction,(center_actor,axis_actor)in TARGETS.items():
        desired_center=origin+Vector(basis.T@np.asarray(center_actor))/scale
        desired_axis=Vector(basis.T@np.asarray(axis_actor)).normalized()
        candidates=[]
        for sign in (1.,-1.):
            axis=desired_axis*sign
            first=old_axis.rotation_difference(axis)@hq
            for step in range(72):
                roll=(step-36)*(2.*math.pi/72.)
                q=Quaternion(axis,roll)@first
                offset=Matrix.LocRotScale(Vector((0.,0.,0.)),q,hs)@mid
                goal=desired_center-offset
                distance=(goal-shoulder).length
                unreachable=max(0.,distance-reach+.002)
                angle=hq.rotation_difference(q).angle
                score=unreachable*unreachable*1000.+(goal-hp).length_squared*.12+angle*angle*.002
                candidates.append((score,q,goal,roll,sign,unreachable))
        _,q,goal,roll,sign,unreachable=min(candidates,key=lambda c:c[0])
        pose=ik_pose(neutral,'r',goal)
        old_hand=pose[hand_index];p,oldq,s=old_hand.decompose()
        rotation=q@oldq.inverted()
        for i,n in enumerate(bones.keys()):
            bone=bones[n]
            if n=='hand_r'or any(parent.name=='hand_r'for parent in bone.parent_recursive):
                pp,qq,ss=pose[i].decompose()
                pose[i]=Matrix.LocRotScale(p+rotation@(pp-p),rotation@qq,ss)
        pose[index['Weapon1']]=pose[hand_index]@hand_local
        actual=[pose[index['Weapon1']]@v for v in edge_local]
        actual_center=(actual[0]+actual[1])*.5
        error=(actual_center-desired_center).length*scale
        print('GUARD_SOLVE',direction,'error',error,'unreachable',unreachable,'goal',list(goal),'actual_hand',list(pose[hand_index].translation),'shoulder',list(shoulder),'reach',reach,'best_reach',min(c[-1]for c in candidates),flush=True)
        assert abs((pose[index['lowerarm_r']].translation-pose[index['upperarm_r']].translation).length-(elbow-shoulder).length)<1.e-5
        assert abs((pose[hand_index].translation-pose[index['lowerarm_r']].translation).length-(hp-elbow).length)<1.e-5
        poses[direction]=pose
        records[direction]={'target_center_metres':list(center_actor),'target_axis':list(axis_actor),'edge_center_error_metres':error,'fixed_limb_lengths':True,'source_finger_grip_unchanged':True,'selected_edge_orientation':sign,'wrist_roll_radians':roll,'unreachable_distance_source_metres':unreachable}
    assert all(r['edge_center_error_metres']<.006 for r in records.values()),('Actual edge targets unreachable',records)
    return poses,records
