from pathlib import Path
import subprocess,json,csv
n=Path(__file__).resolve().parents[1];root=n.parents[2];r=n/'review'
cmd=['g++','-std=c++17','-O2','-I',str(n/'source-v1'),str(n/'tools/audit_feet.cpp')]+[str(n/'runtime-native-v1/native-objects/engine'/(x+'.o')) for x in ['world_skeletal_hero.cpp','world_animation.cpp','world_animation_render.cpp','world_training_combat.cpp','texture_cache.c']]+['-lSDL2','-lGL','-o',str(r/'audit_feet_rates')]
subprocess.run(cmd,check=True)
rows=[];analyzer=root/'new_engine/build/modular-knights-2026-11-13/locomotion-polish-v4/analyze_support_feet.py'
for name,prefix in [('sword',''),('sword_shield','Shield_'),('great_axe','Axe_'),('staff','Staff_')]:
 for fps in [30,60,120]:
  out=r/'feet-contract-clearance'/name/str(fps)
  subprocess.run([str(r/'audit_feet_rates'),str(root/'new_engine/build/cinematic-studio-2026-11-15/equipment-assets/rigs'/(name+'.glb')),str(out),prefix,str(fps)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['python3',str(analyzer),'--data-dir',str(out),'--output-dir',str(out)],check=True,stdout=(out/'analysis.log').open('w'))
  data=json.loads((out/'SUPPORT_FOOT_BASELINE.json').read_text());cases={x['scenario']:x for x in data['controller']};samples=list(csv.DictReader((out/'controller-feet.csv').open()))
  penetration=max(p['depth_m'] for x in cases.values() for p in x['boot_penetration'])
  lengths={k:max(float(x[k]) for x in samples)-min(float(x[k]) for x in samples) for k in ['upper_l_length','lower_l_length','upper_r_length','lower_r_length']}
  row={'weapon':name,'hertz':fps,'samples':len(samples),'boot_penetration_m':penetration,'max_bone_length_change_m':max(lengths.values()),'sideways_near_ground_travel_m':cases['walk_strafe']['worst_support_interval']['max_displacement_from_support_start_m'],'walking_stop_near_ground_travel_m':cases['walk_abrupt']['worst_support_interval']['max_displacement_from_support_start_m'],'report':str(out/'SUPPORT_FOOT_BASELINE.json')}
  row['passed']=penetration<.0011 and row['max_bone_length_change_m']<.0001 and row['sideways_near_ground_travel_m']<.11
  rows.append(row);print(json.dumps(row),flush=True)
report={'passed':all(x['passed'] for x in rows),'rows':rows,'scope':'Actual production CPU controller and weighted boot vertices on scripted flat ground. Near-ground travel includes rolling and steps; it is not a direct planted-foot error measure.'}
(r/'FOOT_CONTRACT_CLEARANCE_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
if not report['passed']:raise SystemExit(1)
