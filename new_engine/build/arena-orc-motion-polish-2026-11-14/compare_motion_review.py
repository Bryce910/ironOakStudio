#!/usr/bin/env python3
"""Compare freshly captured production motion; never inherit old visual acceptance."""
from pathlib import Path
import argparse,hashlib,json,subprocess
import numpy as np
HERE=Path(__file__).resolve().parent;OUT=HERE/'opponent-render-review'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(label):
    p=OUT/('motion-'+label)/'MOTION_DIAGNOSTIC.json';r=json.loads(p.read_text());assert r['passed']
    for key in ['inputs_sha256','outputs_sha256']:
        for name,h in r[key].items():assert sha(name)==h,('Stale actual motion diagnostic',name)
    return p,r
def travel(label,chapter):
    rows=[json.loads(s)for s in (OUT/('motion-'+label)/'motion-trace.jsonl').read_text().splitlines()]
    q=[r for r in rows if r['chapter']==chapter]
    return max(float(np.linalg.norm(np.diff(np.array([r['probes'][name]['p']for r in q]),axis=0),axis=1).sum())for name in ['sole_l','sole_r'])
def compare():
    bp,b=read('baseline');cp,c=read('candidate');bm=b['metrics']['phases'];cm=c['metrics']['phases']
    right_before=max(bm['attack_1'][n]['max_height_m']for n in ['sole_l','sole_r'])
    right_after=max(cm['attack_1'][n]['max_height_m']for n in ['sole_l','sole_r'])
    left_before=travel('baseline','attack_0');left_after=travel('candidate','attack_0')
    reaction_before=b['reaction_prediction']['first_contact_step_max_m'];reaction_after=c['reaction_prediction']['first_contact_step_max_m']
    floor=min(cm[name][sole]['min_vertex_height_m']for name in cm for sole in ['sole_l','sole_r'])
    checks=dict(actual_before_and_after_continuous_frames_1260_each=b['result']['continuous_frames']==c['result']['continuous_frames']==1260,
                actual_pose_and_blade_samples_2520_each=b['result']['actual_pnt_samples']==c['result']['actual_pnt_samples']==2520,
                right_attack_excessive_leg_lift_reduced=right_after<right_before*.5,
                left_attack_full_spin_foot_travel_reduced=left_after<left_before*.75,
                first_contact_pose_preserved=reaction_before>.10 and reaction_after<1e-5,
                guard_and_reaction_physics_prediction_matches=c['guard_prediction']['max_error_m']<1e-4 and c['reaction_prediction']['max_error_m']<1e-4,
                no_sole_floor_penetration_over_2cm=floor>=-.02)
    changes=dict(right_attack_peak_sole_height_m=dict(before=right_before,after=right_after),
                 left_attack_max_sole_total_path_m=dict(before=left_before,after=left_after),
                 reaction_first_contact_blade_step_m=dict(before=reaction_before,after=reaction_after),
                 minimum_actual_sole_vertex_m=floor)
    for phase in bm:
        changes[phase]={name:{key:dict(before=bm[phase][name][key],after=cm[phase][name][key])for key in
                             (['max_speed_m_s','max_frame_step_m','peak_acceleration_m_s2']+(['max_detected_stance_drift_m']if name.startswith('sole')else[]))}
                        for name in ['sole_l','sole_r','hand_r','head']}
    return bp,b,cp,c,checks,changes
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write',action='store_true');args=parser.parse_args()
    bp,b,cp,c,checks,changes=compare();print(json.dumps(dict(checks=checks,changes=changes),indent=2))
    if not args.write:return
    review=OUT/'MOTION_FRAME_VISUAL_REVIEW.json';visual=json.loads(review.read_text())
    assert visual['reviewed_baseline_frames']==visual['reviewed_candidate_frames']==1260 and visual['passed']
    assert visual['baseline_diagnostic_sha256']==sha(bp) and visual['candidate_diagnostic_sha256']==sha(cp)
    assert all(checks.values()),'A target motion improvement or contact/geometry safety check failed'
    combined=OUT/'orc-motion-before-after.mp4'
    command=['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(bp.parent/'motion-continuous.mp4'),'-i',str(cp.parent/'motion-continuous.mp4'),
             '-filter_complex',"[0:v]drawtext=text='BEFORE':x=24:y=20:fontsize=24:fontcolor=white[a];[1:v]drawtext=text='POLISHED':x=24:y=20:fontsize=24:fontcolor=white[b];[a][b]hstack=inputs=2[v]",
             '-map','[v]','-c:v','libx264','-threads','4','-preset','veryfast','-crf','20','-pix_fmt','yuv420p','-movflags','+faststart',str(combined)]
    subprocess.run(command,check=True)
    deps={str(Path(__file__).resolve()):sha(__file__),str(bp):sha(bp),str(cp):sha(cp),str(review):sha(review)}
    for data in [b,c]:deps.update(data['inputs_sha256']);deps.update(data['outputs_sha256'])
    result=dict(schema='valwood.orc.continuous-motion-comparison.v1',passed=True,targeted_polish_improved=True,
                baseline_stage=str(HERE.parent/'arena-orcs-textured-2026-11-14'),candidate_stage=str(HERE),
                continuous_review=True,video_frames_per_version=1260,actual_samples_per_version=2520,
                seconds_per_version=42,actual_chase_speed_m_s=3.,checks=checks,changes=changes,
                visual_review=visual,before_sha256=deps,after_sha256={p:sha(p)for p in deps},
                outputs_sha256={str(combined):sha(combined)},comparison_video=str(combined),
                limits=['Targeted source/transition improvement, not AAA animation certification or an interactive rehearsal.',
                        'Actual scripted production pose selection, PNT geometry, rigid blade and predictor are sampled; no hardware frame-rate/controller-feel claim.',
                        *visual.get('remaining_limitations',[])])
    assert result['before_sha256']==result['after_sha256'],'Motion review dependencies changed during comparison'
    (OUT/'ORC_MOTION_COMPARISON_REVIEW.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
if __name__=='__main__':main()
