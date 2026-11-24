#!/usr/bin/env python3
"""Separate actual sole contact vertices from a heel/toe-rolling centroid."""
from pathlib import Path
import hashlib,importlib.util,json
import numpy as np
HERE=Path(__file__).resolve().parent;OUT=HERE/'opponent-render-review'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
    spec=importlib.util.spec_from_file_location('owned_orc_glb',HERE/'check_orc_assets.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    inputs={str(Path(__file__).resolve()):sha(__file__)};body=HERE/'arena_orcs/body';cache={}
    for pose in list(range(12)):
        path=body/f'combat_body_{pose:03}.glb';g=mod.Glb(path,inputs);primitive=g.doc['meshes'][0]['primitives'][0]
        cache[pose]=g.accessor(primitive['attributes']['POSITION']).astype(np.float64)
    trace=OUT/'motion-candidate/motion-trace.jsonl';probes=OUT/'MOTION_PROBES.json';inputs[str(trace)]=sha(trace);inputs[str(probes)]=sha(probes)
    indices=json.loads(probes.read_text())['probes'];rows=[json.loads(s)for s in trace.read_text().splitlines()];rows=[r for r in rows if r['chapter']=='locomotion']
    source_path=HERE/'arena_orcs/body-semantic.npz';inputs[str(source_path)]=sha(source_path);source=np.load(source_path,allow_pickle=False);names=list(map(str,source['bone_names']))
    def pair(slot):
        if slot<4:return slot,min(3,slot+1)
        if slot<12:return slot,4+(slot-3)%8
        return {172:(0,4),173:(4,0),174:(8,0)}[slot]
    results={};max_error=0
    for name in ['sole_l','sole_r']:
        side=name[-1];columns=[names.index(n)for n in ['foot_'+side,'ball_'+side]]
        # Include the whole genuinely weighted foot, not only its rest-pose
        # bottom quartile: a rolled toe can contact on a different surface.
        full_indices=np.flatnonzero(source['weights_raw_0'][:,columns].sum(axis=1)>.65)
        positions=[]
        for r in rows:
            a,b=pair(r['slot']);q=cache[a][full_indices]*(1-r['weight'])+cache[b][full_indices]*r['weight'];yaw=r['yaw'];cs=np.cos(yaw);sn=np.sin(yaw)
            world=q.copy();world[:,0]=cs*q[:,0]+sn*q[:,2];world[:,2]=-sn*q[:,0]+cs*q[:,2];world+=r['root']
            selected=[int(np.flatnonzero(full_indices==i)[0])for i in indices[name]]
            max_error=max(max_error,float(np.linalg.norm(world[selected].mean(axis=0)-r['probes'][name]['p'])))
            positions.append(world)
        p=np.array(positions);speed=np.linalg.norm(np.diff(p[:,:,[0,2]],axis=0)*60,axis=2)
        contact=(p[:,:,1]<.015)&(p[:,:,1]<=p[:,:,1].min(axis=1)[:,None]+.005)
        min_speed=[];all_fast=[];steady=[];steady_steps=[]
        for f in range(1,len(rows)):
            valid=contact[f]&contact[f-1]
            if valid.any():
                min_speed.append(float(speed[f-1,valid].min()));all_fast.append(bool((speed[f-1,valid]>.3).all()))
                if 3.2<=rows[f]['t']<=5.8:
                    best=np.flatnonzero(valid)[np.argmin(speed[f-1,valid])]
                    steady.append(float(speed[f-1,best]));steady_steps.append(dict(t=rows[f]['t'],vertex=int(full_indices[best]),speed_m_s=float(speed[f-1,best]),world_step_m=(p[f,best]-p[f-1,best]).tolist()))
            else:all_fast.append(False)
        longest=cur=0
        for flag in all_fast:cur=cur+1 if flag else 0;longest=max(cur,longest)
        anchored=[]
        for vertex in range(p.shape[1]):
            start=None
            for f,flag in enumerate([*contact[:,vertex],False]):
                if flag and start is None:start=f
                if not flag and start is not None:
                    if f-start>=5:
                        travel=float(np.linalg.norm((p[start:f,vertex]-p[start,vertex])[:,[0,2]],axis=1).max());anchored.append(dict(seconds=(f-start)/60,drift_m=travel))
                    start=None
        results[name]=dict(source_foot_vertex_count=len(full_indices),contact_frames=len(min_speed),minimum_contact_vertex_speed_m_s=dict(median=float(np.median(min_speed)),p95=float(np.quantile(min_speed,.95)),maximum=float(max(min_speed))),
                           steady_3m_s_contact_samples=steady_steps,
                           steady_3m_s_minimum_contact_speed_m_s=dict(median=float(np.median(steady))if steady else None,p95=float(np.quantile(steady,.95))if steady else None),
                           longest_all_contact_vertices_above_0_3m_s_seconds=longest/60,
                           continuous_same_vertex_ground_contacts=len(anchored),max_continuous_same_vertex_ground_drift_m=max((r['drift_m']for r in anchored),default=0),
                           median_continuous_same_vertex_ground_drift_m=float(np.median([r['drift_m']for r in anchored])))
    assert max_error<3e-6,('Reconstructed sole differs from actual native PNT trace',max_error)
    result=dict(schema='valwood.orc.run-sole-contact-review.v1',passed=True,actual_native_pnt_centroid_agreement_max_m=max_error,
                passed_meaning='Reconstruction agrees with actual native PNT; this does not accept planted-foot quality.',
                foot_skating_confirmed=False,contact_detector_sufficient_for_stance_acceptance=False,
                source='Exact committed body GLB pairs/weights from actual60Hz trace, independently checked against native PNT probe centroids.',
                contact_detector='Vertices within15mm of floor and5mm of lowest sole vertex; require same vertex near floor across adjacent frames. A contact-speed diagnostic, not authored foot-lock metadata.',
                feet=results,inputs=inputs,limits=['Centroid travel during heel/toe roll does not imply foot skating.',
                'Source Run support minima commonly remain2–6cm above the fixture floor, so15mm contact threshold produces insufficient steady contacts. Do not treat these sparse samples or centroid drift as a reliable planted-foot acceptance.',
                'This metric is diagnostic and must be interpreted with the full film; no gait was modified to force a centroid to stay fixed.'])
    result['inputs'][str(HERE/'check_orc_assets.py')]=sha(HERE/'check_orc_assets.py')
    (OUT/'RUN_CONTACT_VERTEX_REVIEW.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
