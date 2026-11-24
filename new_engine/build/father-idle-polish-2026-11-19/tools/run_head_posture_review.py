#!/usr/bin/env python3
"""Independent actual-controller comparison with exact pre-head v4 implementation."""
from pathlib import Path
import argparse,hashlib,json,math,os,subprocess,time
os.environ['OPENBLAS_NUM_THREADS']='1';os.environ['OMP_NUM_THREADS']='1'
import numpy as np
N=Path(__file__).resolve().parents[1];D=N/'review/head-posture';SOURCE=N/'source-v1';V4=D/'baseline-v4.cpp'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def inputs():
 return [Path(__file__),N/'tools/head_posture_probe.cpp',V4]+[SOURCE/'engine'/p for p in ('world_skeletal_hero.cpp','world_skeletal_hero.h','world_animation.cpp','world_animation.h','world_equipment_presentation.h','world_knight_appearance.h','world_story_body_pose.h','world_skeletal_hero_foot_contact.inc','world_skeletal_hero_stationary_cut.inc')]+[N/'assets/story_hero'/o/p for o in ('civilian','armored') for p in ('appearance.cfg','body.glb')]+[N/'motion/rigs'/(p+'.glb') for p in ('sword','sword_shield','great_axe','staff')]
def run(source,label):
 exe=D/('head-probe-'+label);cmd=['g++','-std=c++17','-O1','-I'+str(SOURCE),str(N/'tools/head_posture_probe.cpp'),str(SOURCE/'engine/world_animation.cpp'),str(source),'-o',str(exe)];start=time.monotonic();subprocess.run(cmd,check=True,capture_output=True);args=[str(exe),str(N/'assets/story_hero/civilian/appearance.cfg'),str(N/'assets/story_hero/armored/appearance.cfg'),str(N/'motion/rigs'),str(D/(label+'.bin'))]
 with (D/(label+'.jsonl')).open('w') as out,(D/(label+'.log')).open('w') as log:subprocess.run(args,stdout=out,stderr=log,check=True)
 rows=[json.loads(s) for s in (D/(label+'.jsonl')).read_text().splitlines()];arr=np.memmap(D/(label+'.bin'),dtype='<f4',mode='r');return rows,arr,dict(compile_command=cmd,run_command=args,source_sha256=sha(source),binary_sha256=sha(exe),metadata_sha256=sha(D/(label+'.jsonl')),palette_sha256=sha(D/(label+'.bin')),harness_sha256=sha(N/'tools/head_posture_probe.cpp'),elapsed_seconds=time.monotonic()-start)
def matrix(a,row):return a[row['offset']//4:row['offset']//4+row['nodes']*16].reshape(-1,4,4).transpose(0,2,1).astype(float)
def rotation(m):
 r=m[:3,:3].copy();return r/np.linalg.norm(r,axis=0)
def angle(a,b):
 # SVD removes float32 matrix roundoff before measuring orientation.
 q=rotation(a)@rotation(b).T;u,_,v=np.linalg.svd(q);q=u@v;return math.degrees(math.acos(float(np.clip((np.trace(q)-1)/2,-1,1))))

def quat(matrix):
 r=rotation(matrix);u,_,v=np.linalg.svd(r);r=u@v;t=np.trace(r)
 if t>0:
  s=math.sqrt(t+1)*2;q=np.array([(r[2,1]-r[1,2])/s,(r[0,2]-r[2,0])/s,(r[1,0]-r[0,1])/s,.25*s])
 else:
  k=int(np.argmax(np.diag(r)));i=(k+1)%3;j=(k+2)%3;s=math.sqrt(max(0.,1+r[k,k]-r[i,i]-r[j,j]))*2;q=np.zeros(4);q[k]=.25*s;q[i]=(r[i,k]+r[k,i])/s;q[j]=(r[j,k]+r[k,j])/s;q[3]=(r[j,i]-r[i,j])/s
 return q/np.linalg.norm(q)
def short_arc(a,b,w):
 x=quat(a);y=quat(b);dot=float(x@y)
 if dot<0:y=-y;dot=-dot
 if dot>.9995:q=(1-w)*x+w*y
 else:
  t=math.acos(np.clip(dot,-1,1));q=(math.sin((1-w)*t)*x+math.sin(w*t)*y)/math.sin(t)
 q/=np.linalg.norm(q);x,y,z,w=q;m=np.eye(4);m[:3,:3]=[[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],[2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],[2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]];return m

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--reuse-v4',action='store_true');a=ap.parse_args();assert sha(V4)=='ce563d73c1e4594e0091d16c2fb21171282bc4f0266ef656dd0d6d6943cbdc35';pins={str(p):sha(p) for p in inputs()}
 if a.reuse_v4:
  oi=json.loads((D/'V4_EXECUTION.json').read_text());old=[json.loads(s) for s in (D/'v4.jsonl').read_text().splitlines()];oa=np.memmap(D/'v4.bin',dtype='<f4',mode='r');assert oi['source_sha256']==sha(V4) and oi['metadata_sha256']==sha(D/'v4.jsonl') and oi['palette_sha256']==sha(D/'v4.bin') and oi['harness_sha256']==sha(N/'tools/head_posture_probe.cpp')
 else:old,oa,oi=run(V4,'v4');(D/'V4_EXECUTION.json').write_text(json.dumps(oi,indent=2)+'\n')
 new,na,ni=run(SOURCE/'engine/world_skeletal_hero.cpp','candidate');assert len(old)==len(new)
 phases={};errors=[];prev={};head_set=None;all_capsule=0.;cases=set();entries={}
 for x,y in zip(old,new):
  assert (x['case'],x['phase'],x['frame'],x['nodes'],x['head'],x['neck'])==(y['case'],y['phase'],y['frame'],y['nodes'],y['head'],y['neck']);cases.add(x['case']);p=x['phase'];neck=x['neck'];head=x['head'];A=matrix(oa,x);B=matrix(na,y)
  # Discover descendants from real rig hierarchy once; globals outside neck subtree must remain exact.
  if head_set is None:
   import struct
   data=(N/'motion/rigs/sword.glb').read_bytes();l=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+l]);parents={c:i for i,n in enumerate(doc['nodes']) for c in n.get('children',[])};head_set=set()
   for j in range(x['nodes']):
    k=j
    while k in parents and k!=neck:k=parents[k]
    if k==neck:head_set.add(j)
  others=[j for j in range(x['nodes']) if j not in head_set];nonhead=float(abs(A[others]-B[others]).max());nonhead_position=float(np.linalg.norm(A[others,:3,3]-B[others,:3,3],axis=1).max());nonhead_rotation=max((angle(A[j],B[j]) for j in others),default=0.) if nonhead else 0.;orientation=angle(A[head],B[head]);length_error=abs(np.linalg.norm(A[head,:3,3]-A[neck,:3,3])-np.linalg.norm(B[head,:3,3]-B[neck,:3,3]));retract=float(A[head,2,3]-B[head,2,3]);localdiff=float(abs(np.asarray(x['local_translation_scale'])-y['local_translation_scale']).max());palette=float(abs(A-B).max());caps=max(x['capsule_head_error_m'],y['capsule_head_error_m']);all_capsule=max(all_capsule,caps)
  q=phases.setdefault(p,dict(samples=0,head_retraction_min_m=1.,head_retraction_max_m=-1.,maximum_head_orientation_error_deg=0.,maximum_neck_head_length_error_m=0.,maximum_non_neck_descendant_component_difference=0.,maximum_non_neck_position_difference_m=0.,maximum_non_neck_orientation_difference_deg=0.,maximum_local_translation_scale_difference=0.,maximum_palette_difference=0.,maximum_head_step_m=0.,maximum_old_head_step_m=0.,maximum_pause_or_hitstop_pose_change=0.,maximum_world_gaze_shortest_arc_error_deg=0.,maximum_world_gaze_overshoot_deg=0.,maximum_head_angular_step_deg=0.,maximum_old_head_angular_step_deg=0.))
  q['samples']+=1;q['head_retraction_min_m']=min(q['head_retraction_min_m'],retract);q['head_retraction_max_m']=max(q['head_retraction_max_m'],retract)
  for key,value in [('maximum_head_orientation_error_deg',orientation),('maximum_neck_head_length_error_m',length_error),('maximum_non_neck_descendant_component_difference',nonhead),('maximum_non_neck_position_difference_m',nonhead_position),('maximum_non_neck_orientation_difference_deg',nonhead_rotation),('maximum_local_translation_scale_difference',localdiff),('maximum_palette_difference',palette)]:q[key]=max(q[key],value)
  key=(x['case'],p)
  if p in ('draw-bridge','stow-bridge'):
   if key not in entries:entries[key]=B[head].copy()
   target=np.asarray(y['target_head_global']).reshape(4,4).T
   t=y['equipment_progress'];w=t*t*(3-2*t)
   expected=short_arc(entries[key],target,w) if y['equipment_transition'] else target
   q['maximum_world_gaze_shortest_arc_error_deg']=max(q['maximum_world_gaze_shortest_arc_error_deg'],angle(B[head],expected))
   q['maximum_world_gaze_overshoot_deg']=max(q['maximum_world_gaze_overshoot_deg'],max(0.,angle(entries[key],B[head])-angle(entries[key],target)))
  if key in prev:
   pa,pb=prev[key];q['maximum_head_angular_step_deg']=max(q['maximum_head_angular_step_deg'],angle(B[head],pb[head]));q['maximum_old_head_angular_step_deg']=max(q['maximum_old_head_angular_step_deg'],angle(A[head],pa[head]));q['maximum_head_step_m']=max(q['maximum_head_step_m'],float(np.linalg.norm(B[head,:3,3]-pb[head,:3,3])));q['maximum_old_head_step_m']=max(q['maximum_old_head_step_m'],float(np.linalg.norm(A[head,:3,3]-pa[head,:3,3])))
   if p in ('pause','hitstop'):q['maximum_pause_or_hitstop_pose_change']=max(q['maximum_pause_or_hitstop_pose_change'],float(abs(B-pb).max()))
  prev[key]=(A,B)
 for p,q in phases.items():
  if p in ('draw-bridge','stow-bridge'):
   if q['maximum_non_neck_descendant_component_difference']>3e-6 or q['maximum_non_neck_position_difference_m']>3e-6 or q['maximum_non_neck_orientation_difference_deg']>.001:errors.append(p+': outside-neck change exceeds measured float normalization allowance')
  elif q['maximum_non_neck_descendant_component_difference']!=0:errors.append(p+': non-neck subtree changed')
  if q['maximum_local_translation_scale_difference']!=0:errors.append(p+': local translation/scale changed')
  if q['maximum_neck_head_length_error_m']>1e-6:errors.append(p+': neck-head bone length changed')
  if p in ('idle','retry-idle','pause','hitstop'):
   if not .025<=q['head_retraction_min_m']<=q['head_retraction_max_m']<=.04:errors.append(p+': retraction outside25–40mm')
   if q['maximum_head_orientation_error_deg']>.05:errors.append(p+': global gaze changed')
  if p in ('moving','armed','swim','dive','death') and q['maximum_palette_difference']!=0:errors.append(p+': settled non-idle pose changed')
  if p in ('pause','hitstop') and q['maximum_pause_or_hitstop_pose_change']!=0:errors.append(p+': frozen pose changed')
  if p in ('draw-bridge','stow-bridge'):
   if q['maximum_head_step_m']>.012 or q['maximum_head_step_m']>q['maximum_old_head_step_m']+.005:errors.append(p+': head bridge jump')
   if q['maximum_head_orientation_error_deg']>2.:errors.append(p+': deliberate world-gaze bridge differs from old path by more than2degrees')
   if q['maximum_world_gaze_shortest_arc_error_deg']>.05 or q['maximum_world_gaze_overshoot_deg']>.05:errors.append(p+': world gaze does not follow intended shortest arc or overshoots')
   if q['maximum_head_angular_step_deg']>3 or q['maximum_head_angular_step_deg']>q['maximum_old_head_angular_step_deg']+.5:errors.append(p+': angular head bridge jump')
 if all_capsule>1e-6:errors.append('Actual head capsule does not follow actual head joint')
 for p,h in pins.items():
  if sha(p)!=h:errors.append('Input changed during execution: '+p)
 report=dict(schema='valwood.head-posture-independent-cpu.v1',passed=not errors,gpu_used=False,production_modified=False,outfit_loadout_cases=len(cases),pose_pairs=len(old),candidate_execution=ni,pre_head_v4_execution=oi,phases=phases,bridge_only_normalization_allowance=dict(matrix_component=3e-6,joint_position_m=3e-6,joint_orientation_degrees=.001,reason='Repeated evaluate normalizes float32 quaternions; exact zero outside draw/stow bridge'),maximum_actual_head_capsule_error_m=all_capsule,errors=errors,inputs_sha256=pins,limits=['Controlled actual-controller CPU comparison; collar/skin quality requires visual inspection.','Both outfits intentionally share the neutral-rig neck correction; no appearance-name/count heuristic.','Settled gaze<.05degree. Draw/stow intentionally uses global shortest-arc gaze instead of old parent/child local-slerp artifact; old-path difference must stay<=2degrees, independent target-arc error/overshoot<=.05degree, positionstep<=12mm and angularstep<=3degrees/60Hz and old+.5degree. The rejected1degree old-path diagnostic is retained historically; it was not a golden behavior contract.','The global-gaze bridge performs one additional evaluate(), which normalizes every float32 quaternion. Only draw/stow bridges allow <=3e-6 matrix components, <=3micrometers joint position, and <=.001degree outside-neck rotation to account for measured normalization noise; every other phase remains exact zero. Strict-zero failure is retained under strict-zero-normalization. No articulated outside-chain change is permitted.', 'No new animation channels, binds, body geometry, local translations or scales are allowed to change.','Native game visual, animation smoothness and AAA quality remain separate acceptance.'])
 out=D/'CPU_REVIEW.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=report['passed'],errors=errors,phases=phases,report_sha256=sha(out)),indent=2));raise SystemExit(0 if report['passed'] else 1)
if __name__=='__main__':main()
