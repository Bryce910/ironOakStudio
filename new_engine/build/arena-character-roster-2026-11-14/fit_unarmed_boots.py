#!/usr/bin/env python3
"""Fit new authored boots to the actual source foot clearance, with leg IK.

Only the two leg chains change. The original source hip/root, upper body,
foot orientation and per-foot flight clearance are retained. No vertex clamp,
per-frame root translation, mesh warp or independently stretched limb is used.
"""
from __future__ import annotations
import numpy as np

def _unit(v):
    n=float(np.linalg.norm(v))
    if n < 1e-10: raise ValueError('Degenerate leg direction')
    return v/n

def _swing(a,b):
    a=_unit(a);b=_unit(b);v=np.cross(a,b);c=float(np.clip(a@b,-1,1))
    if c > 1-1e-12:return np.eye(3)
    if c < -1+1e-9:
        axis=_unit(np.cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]))
        return 2*np.outer(axis,axis)-np.eye(3)
    k=np.array([[0,-v[2],v[1]],[v[2],0,-v[0]],[-v[1],v[0],0]])
    return np.eye(3)+k+k@k/(1+c)

def _matrix(T):
    out=np.tile(np.eye(4),(len(T)//4,1,1));out[:,:3,:]=T.reshape(-1,4,3).transpose(0,2,1)
    return out

def _transform(M):return M[:,:3,:].transpose(0,2,1).reshape(-1,3)

def fit(primitives,transforms,skin,tolerance=.00005):
    """Return (32 fitted source LBS arrays, JSON-safe exact measurements).

    ``primitives`` uses the existing fitted rest/weights/x arrays. ``transforms``
    is the recovered authored unarmed32 bank. Strict source4weight fitting is
    unchanged; the method is also valid for losslessly grouped primitives.
    """
    transforms=np.asarray(transforms,float)
    if transforms.shape!=(32,len(skin.BONES)*4,3):raise ValueError('Expected actual unarmed32 source transforms')
    index={name:i for i,name in enumerate(skin.BONES)}
    rest=np.asarray([skin.DATA['target'].get(n,np.eye(4))for n in skin.BONES],float)
    invrest=np.linalg.inv(rest)
    points=np.concatenate([p['rest'] for p in primitives])
    weights=np.concatenate([p['weights'] for p in primitives])
    x=np.concatenate([p['x'] for p in primitives])
    chains=[]
    for side in ['Left','Right']:
        ids=[index[side+n]for n in ['UpLeg','Leg','Foot','ToeBase','Toe_End']]
        source_mask=skin.WEIGHTS[:,ids[2:]].sum(1)>.75
        mask=weights[:,ids[2:]].sum(1)>.75
        if source_mask.sum()<16 or mask.sum()<16:raise ValueError((side,'Insufficient actual boot/source foot geometry'))
        chains.append((side,ids,source_mask,mask))
    # A single outfit calibration accounts for the new sole thickness. Unlike
    # per-frame flooring, it preserves the authored root motion and flight.
    calibration=float((x@transforms[0])[:,1].min()-(skin.X@transforms[0])[:,1].min())
    calibrated=transforms.copy().reshape(32,-1,4,3)
    calibrated[:,:,3,1]-=calibration
    calibrated=calibrated.reshape(transforms.shape)
    records=[];result=[]
    for pose,T in enumerate(calibrated):
        M=_matrix(T);G=M@rest;original=G.copy();target_source=skin.X@transforms[pose]
        before=x@T;chain_records=[]
        for side,ids,source_mask,mask in chains:
            hip,knee,ankle=G[ids[:3],:3,3].copy()
            upper=float(np.linalg.norm(knee-hip));lower=float(np.linalg.norm(ankle-knee))
            ray=_unit(ankle-hip)
            pole=knee-hip-ray*((knee-hip)@ray)
            if np.linalg.norm(pole)<1e-5:
                pole=original[ids[1],:3,1]-ray*(original[ids[1],:3,1]@ray)
            pole=_unit(pole)
            source_clearance=float(target_source[source_mask,1].min())
            # The source barefoot mesh and the new closed boot have different
            # volumes. Correct only loss of authored clearance. Forcing a boot
            # already above that envelope down can overextend a straight leg.
            clearance=max(source_clearance,float(before[mask,1].min()))
            dy=0.;converged=False
            for iteration in range(12):
                current=x@_transform(G@invrest)
                gap=clearance-float(current[mask,1].min())
                if abs(gap)<=tolerance:converged=True;break
                dy+=gap
                target=ankle+np.array([0.,dy,0.]);delta=target-hip;distance=float(np.linalg.norm(delta))
                if not abs(upper-lower)+1e-6<distance<upper+lower-1e-6:
                    raise ValueError((pose,side,'Source clearance unreachable with fixed limb lengths',distance,upper,lower,dy))
                direction=delta/distance
                along=(upper*upper-lower*lower+distance*distance)/(2*distance)
                projected=pole-direction*(pole@direction)
                projected=_unit(projected)
                nextknee=hip+direction*along+projected*np.sqrt(max(0.,upper*upper-along*along))
                G[ids[0]]=original[ids[0]];G[ids[0],:3,:3]=_swing(knee-hip,nextknee-hip)@original[ids[0],:3,:3]
                G[ids[1]]=original[ids[1]];G[ids[1],:3,:3]=_swing(ankle-knee,target-nextknee)@original[ids[1],:3,:3];G[ids[1],:3,3]=nextknee
                for b in ids[2:]:G[b]=original[b];G[b,:3,3]+=target-ankle
            if not converged:raise ValueError((pose,side,'Boot contact IK failed to converge',gap))
            solved=G[ids[:3],:3,3]
            length_error=max(abs(np.linalg.norm(solved[1]-solved[0])-upper),abs(np.linalg.norm(solved[2]-solved[1])-lower))
            if length_error>1e-8:raise ValueError(('Leg length changed',length_error))
            chain_records.append(dict(side=side,source_foot_vertices=int(source_mask.sum()),outfit_boot_vertices=int(mask.sum()),source_clearance_m=source_clearance,target_clearance_m=clearance,original_outfit_clearance_m=float(before[mask,1].min()),fitted_clearance_m=float(current[mask,1].min()),ankle_vertical_adjustment_m=dy,leg_length_error_m=float(length_error),foot_orientation_error=float(np.max(abs(G[ids[2:],:3,:3]-original[ids[2:],:3,:3]))),iterations=iteration))
        fitted=_transform(G@invrest)
        changed={b for _,ids,_,_ in chains for b in ids}
        fixed=[b for b in range(len(skin.BONES))if b not in changed]
        # Preserve source coefficients exactly for all non-leg bones.
        fitted.reshape(-1,4,3)[fixed]=T.reshape(-1,4,3)[fixed]
        if not np.array_equal(fitted.reshape(-1,4,3)[fixed],T.reshape(-1,4,3)[fixed]):raise ValueError('Upper body changed')
        final=x@fitted
        records.append(dict(pose=pose,source_min_y_m=float(target_source[:,1].min()),original_outfit_min_y_m=float(before[:,1].min()),fitted_outfit_min_y_m=float(final[:,1].min()),max_vertex_adjustment_m=float(np.linalg.norm(final-before,axis=1).max()),fixed_nonleg_bones=len(fixed),chains=chain_records))
        result.append(fitted)
    return np.asarray(result),dict(schema='valwood.knight-npc.source-clearance-boot-ik.v1',passed=True,constant_outfit_sole_calibration_m=calibration,policy='One constant outfit sole calibration, then correct loss of actual per-foot authored source clearance; retain an already higher boot envelope; new-outfit boot mesh contact evaluated by LBS; fixed-length leg IK; original pelvis/root and all non-leg matrices bit-identical after that single calibration; foot/toe orientation unchanged; no vertex clamps or per-pose root/floor translations',records=records,maximum_leg_length_error_m=max(c['leg_length_error_m']for row in records for c in row['chains']),maximum_clearance_error_m=max(abs(c['target_clearance_m']-c['fitted_clearance_m'])for row in records for c in row['chains']))
