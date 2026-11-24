from pathlib import Path
import subprocess,json,csv
N=Path(__file__).resolve().parents[1];analyzer=N.parent/'modular-knights-2026-11-13/locomotion-polish-v4/analyze_support_feet.py';rigs=N.parent/'cinematic-studio-2026-11-15/equipment-assets/rigs';results=[]
for name,prefix in [('sword',''),('sword_shield','Shield_'),('great_axe','Axe_'),('staff','Staff_')]:
 for fps in [30,60,120]:
  pair={}
  for which in ['before','after']:
   out=N/'review/gait-regression'/which/name/str(fps);out.mkdir(parents=True,exist_ok=True)
   subprocess.run([str(N/f'review/audit-locomotion-{which}'),str(rigs/(name+'.glb')),str(out),prefix,str(fps)],check=True,stdout=subprocess.DEVNULL)
   subprocess.run(['python3',str(analyzer),'--data-dir',str(out),'--output-dir',str(out)],check=True,stdout=(out/'analysis.log').open('w'))
   data=json.load(open(out/'SUPPORT_FOOT_BASELINE.json'));samples=list(csv.DictReader(open(out/'controller-feet.csv')));cases={r['scenario']:r for r in data['controller']};lengths=[max(float(r[k]) for r in samples)-min(float(r[k]) for r in samples) for k in ['upper_l_length','lower_l_length','upper_r_length','lower_r_length']]
   pair[which]=dict(penetration_m=max(y['depth_m'] for x in cases.values() for y in x['boot_penetration']),maximum_bone_length_change_m=max(lengths),run_to_walk_near_ground_travel_m=cases['walk_run_change']['worst_support_interval']['max_displacement_from_support_start_m'],strafe_near_ground_travel_m=cases['walk_strafe']['worst_support_interval']['max_displacement_from_support_start_m'],samples=len(samples))
  ok=pair['after']['penetration_m']<.0011 and pair['after']['maximum_bone_length_change_m']<.0001 and pair['after']['strafe_near_ground_travel_m']<.11
  results.append(dict(weapon=name,fps=fps,**pair,passed=ok));print(name,fps,ok,flush=True)
primary=next(r for r in results if r['weapon']=='sword' and r['fps']==60);improvement=100*(1-primary['after']['run_to_walk_near_ground_travel_m']/primary['before']['run_to_walk_near_ground_travel_m'])
report=dict(passed=all(r['passed'] for r in results) and improvement>10,scripted_flat_ground=True,all_four_weapon_rigs=True,rates=[30,60,120],run_to_walk_improvement_percent=improvement,near_ground_travel_includes_rolling_and_steps=True,native_controller_feel_certified=False,results=results)
(N/'review/LOCOMOTION_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n');print('Passed',report['passed'],'Sword 60 Hz improvement',improvement)
