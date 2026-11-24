#!/usr/bin/env python3
"""Compile actual baseline and candidate controllers; no GL or runtime launch."""
from pathlib import Path
import argparse,hashlib,json,math,os,subprocess,time
N=Path(__file__).resolve().parents[1];B=Path(json.loads((N/'BASELINE.json').read_text())['baseline']);D=N/'review/idle-grounding';D.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build_run(owner,label):
 src=owner/'source-v1';exe=D/('probe-'+label);cpp=N/'tools/idle_grounding_probe.cpp';cmd=['g++','-std=c++17','-O1','-I'+str(src),str(cpp),str(src/'engine/world_animation.cpp'),str(src/'engine/world_skeletal_hero.cpp'),'-o',str(exe)];start=time.monotonic();subprocess.run(cmd,check=True,capture_output=True);args=[str(exe),str(owner/'assets/story_hero/civilian/appearance.cfg'),str(owner/'assets/story_hero/armored/appearance.cfg'),str(owner/'motion/rigs')];out=D/(label+'.jsonl');env=dict(os.environ,OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1')
 with out.open('w') as f:subprocess.run(args,stdout=f,stderr=(D/(label+'.log')).open('w'),env=env,check=True)
 rows=[json.loads(s) for s in out.read_text().splitlines() if s.startswith('{')]
 return rows,dict(compile_command=cmd,run_command=args,elapsed_seconds=time.monotonic()-start,binary_sha256=sha(exe),output_sha256=sha(out),source_cpp_sha256=sha(src/'engine/world_skeletal_hero.cpp'))
def analyze(rows):
 result=[];errors=[]
 for c in range(8):
  cr=[r for r in rows if r['case']==c];case=dict(case=c,outfit=c//4,weapon=cr[0]['weapon'],phases={})
  for phase in ('fresh-stow-idle','walk-stop-idle','restow-idle','slope-idle'):
   rs=[r for r in cr if r['phase']==phase];assert len(rs)==180
   diffs=[abs(r['knees'][s]-r['raw_knees'][s]) for r in rs for s in (0,1)];soles=[v for r in rs for v in r['sole_min']];angles=[v for r in rs for v in r['knees']]
   steps=[math.dist(a['joint_points'][j],b['joint_points'][j]) for a,b in zip(rs[:-1],rs[1:]) for j in range(4)];rawsteps=[math.dist(a['raw_joint_points'][j],b['raw_joint_points'][j]) for a,b in zip(rs[:-1],rs[1:]) for j in range(4)]
   q=dict(maximum_quiet_joint_step_m=max(steps),maximum_raw_quiet_joint_step_m=max(rawsteps),quiet_joint_step_limit_m=max(.006,3*max(rawsteps)+.002),samples=len(rs),maximum_raw_grounded_knee_difference_deg=max(diffs),actual_knee_min_deg=min(angles),actual_knee_max_deg=max(angles),actual_sole_min_m=min(soles),actual_sole_max_m=max(soles),foot_reach_release_delta=rs[-1]['foot_reach_releases']-rs[0]['foot_reach_releases'])
   case['phases'][phase]=q
   if any(not r['full_unarmed'] or not r['settled'] or r['weapon_visible'] for r in rs):errors.append(f'case{c} {phase}: missing settled unarmed state')
   if phase!='slope-idle' and q['maximum_raw_grounded_knee_difference_deg']>2:errors.append(f'case{c} {phase}: flat grounding changes captured knee by more than2deg')
   if q['actual_sole_min_m']<-.008 or q['actual_sole_max_m']>.035:errors.append(f'case{c} {phase}: actual boot surface outside−8..35mm ground tolerance')
   if q['actual_knee_min_deg']<140 or q['actual_knee_max_deg']>180.001:errors.append(f'case{c} {phase}: unsafe joint reach')
   if q['foot_reach_release_delta']!=0:errors.append(f'case{c} {phase}: quiet leg-reach release/replant')
   if q['maximum_quiet_joint_step_m']>q['quiet_joint_step_limit_m']:errors.append(f'case{c} {phase}: quiet ankle/knee step exceeds captured-motion-scaled limit')
  result.append(case)
 return result,errors

def main():
 p=argparse.ArgumentParser();p.add_argument('--reuse-baseline',action='store_true');a=p.parse_args();start_pins={str(N/'source-v1/engine/world_skeletal_hero.cpp'):sha(N/'source-v1/engine/world_skeletal_hero.cpp')}
 if a.reuse_baseline:
  info=json.loads((D/'BASELINE_EXECUTION.json').read_text());old=[json.loads(s) for s in (D/'baseline.jsonl').read_text().splitlines() if s.startswith('{')];assert info['source_cpp_sha256']==sha(B/'source-v1/engine/world_skeletal_hero.cpp') and info['output_sha256']==sha(D/'baseline.jsonl')
 else:old,info=build_run(B,'baseline');(D/'BASELINE_EXECUTION.json').write_text(json.dumps(info,indent=2)+'\n')
 new,ninfo=build_run(N,'candidate');cases,errors=analyze(new);oldcases,olderrors=analyze(old)
 assert len(old)==len(new)
 max_armed=0.;max_moving=0.;max_contact=0.;counts=dict(armed_palette_pairs=0,moving_palette_pairs=0,contact_pairs=0)
 for x,y in zip(old,new):
  assert (x['case'],x['phase'],x.get('frame'),x.get('direction'))==(y['case'],y['phase'],y.get('frame'),y.get('direction'))
  if x['phase']=='contact':max_contact=max(max_contact,max(abs(a-b) for a,b in zip(x['values'],y['values'])));counts['contact_pairs']+=1
  elif 'palette' in x:
   assert len(x['palette'])==len(y['palette']);delta=max(abs(a-b) for a,b in zip(x['palette'],y['palette']))
   if x['phase'].startswith('armed-'):max_armed=max(max_armed,delta);counts['armed_palette_pairs']+=1
   elif x['phase']=='settled-moving':max_moving=max(max_moving,delta);counts['moving_palette_pairs']+=1
 if max_armed!=0 or max_contact!=0:errors.append('Published armed skinning/contact outputs changed')
 if max_moving!=0:errors.append('Settled moving palette differs from published')
 paths=[Path(__file__),N/'tools/idle_grounding_probe.cpp']
 for owner in (B,N):
  paths += [owner/'source-v1/engine'/s for s in ('world_skeletal_hero.cpp','world_skeletal_hero.h','world_animation.cpp','world_animation.h','world_equipment_presentation.h','world_knight_appearance.h','world_story_body_pose.h','world_skeletal_hero_foot_contact.inc','world_skeletal_hero_stationary_cut.inc')]
  paths += [owner/'assets/story_hero'/o/s for o in ('civilian','armored') for s in ('appearance.cfg','body.glb')]
  paths += [owner/'motion/rigs'/(s+'.glb') for s in ('sword','sword_shield','great_axe','staff')]
 for p,h in start_pins.items():
  if sha(p)!=h:errors.append('Candidate source changed during CPU run; rerun')
 report=dict(schema='valwood.idle-grounding-independent-cpu.v1',passed=not errors,gpu_used=False,production_modified=False,actual_controller_and_meshes=True,outfit_loadout_cases=8,candidate_execution=ninfo,baseline_execution=info,cases=cases,baseline_cases=oldcases,baseline_validation_errors=olderrors,baseline_expected_old_crouch_not_accepted=True,comparison=dict(maximum_armed_palette_component_difference=max_armed,maximum_contact_query_difference=max_contact,maximum_settled_moving_palette_component_difference=max_moving,moving_tolerance=0.,**counts),errors=errors,inputs_sha256={str(p):sha(p) for p in paths},limits=['Raw/grounded2degree limit applies only to settled flat idle, not slope adaptation.','Actual weighted boot vertices are measured; this is not a proxy foot-joint height test.','Armed comparisons use independent binaries compiled from actual published and candidate source.','Settled walking is compared after10seconds so changed idle anchors have had multiple real source contact/release cycles; start/stop bridges are intentionally allowed to change and are checked separately by final idle/sole assertions.','CPU proof is not native visual or AAA quality approval.'])
 out=D/'CPU_REVIEW.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=report['passed'],errors=errors,comparison=report['comparison'],sha256=sha(out)),indent=2));raise SystemExit(0 if report['passed'] else 1)
if __name__=='__main__':main()
