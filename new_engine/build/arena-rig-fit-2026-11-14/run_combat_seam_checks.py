from pathlib import Path
import datetime,hashlib,json,subprocess
base=Path(__file__).resolve().parent
root=base.parents[1]
asset=Path(json.loads((base/'HERO_ASSET.json').read_text())['path'])
files=[base/'source-v1/engine/world_skeletal_hero.cpp',base/'source-v1/engine/world_skeletal_hero.h',base/'source-v1/engine/world_animation.cpp',base/'source-v1/engine/world_animation.h',base/'source-v1/engine/world_training_combat.h',base/'test_skeletal_hero.cpp',base/'test_combat_seams.cpp',Path(__file__).resolve(),asset]
files += [base/'source-v1/engine/world_attack_foot_contact.h',base/'source-v1/engine/world_skeletal_hero_foot_contact.inc',base/'source-v1/engine/world_combat_timing.h', base/'source-v1/engine/world_weapon_loadout.h']
files += [base/'HERO_ASSET.json',base/'source-v1/engine/world_skeletal_hero_stationary_cut.inc',Path(__file__).resolve()]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(asset)==json.loads((base/'HERO_ASSET.json').read_text())['sha256'],'Selected hero differs from manifest'
def pins():return {str(p):sha(p) for p in files}
before=pins();started=datetime.datetime.now(datetime.timezone.utc).isoformat()
binary=base/'test_combat_seams'
command=['/usr/bin/x86_64-linux-gnu-g++-13','-std=c++17','-O2','-Wall','-Wextra','-I'+str(base/'source-v1'),str(base/'test_combat_seams.cpp'),str(base/'source-v1/engine/world_skeletal_hero.cpp'),str(base/'source-v1/engine/world_animation.cpp'),'-o',str(binary)]
subprocess.run(command,check=True)
result=subprocess.run([str(binary),str(asset)],capture_output=True,text=True,check=True)
assert before==pins(),'Combat source/asset changed during checks'
report=json.loads(result.stdout)
report.update(schema='valwood.skeletal-combat-seams.v1',scope='CPU event/pose/contact regression on manual legacy phase fixtures; actual new shared timing has separate production-clock proofs.',rates_hz=[30,60,120],all25_combo_pairs=True,all5_attacks_and_5_reactions=True,all5_guard_and_5_reactions=True,physical_timing='manual legacy phase fixtures; actual shared timing validated by ATTACK_FLOW_CPU and COMBAT_TIMING_REVIEW',source_sha256=before,test_executable_sha256=sha(binary),known_limits=['Reaction arms intentionally retain the active attack/guard capture; the source Hit offhand release is not overlaid.','Contact event frames commit before recoil and then hold during hit-stop.','No new first-person or non-longsword support is claimed.'])
path=base/'COMBAT_SEAMS.json';path.write_text(json.dumps(report,indent=2)+'\n')
receipt=dict(schema='valwood.skeletal-combat-seams-inputs.v1',started_utc=started,finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),compile_command=command,execution_command=[str(binary),str(asset)],before_sha256=before,after_sha256=pins(),source_unchanged=True,test_executable_sha256=sha(binary),report=str(path),report_sha256=sha(path),returncode=result.returncode)
(base/'COMBAT_SEAMS_INPUTS.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps({key:report[key] for key in ['accepted','fresh_serial_queries','pre_render_combo_edge_error_m','reaction_samples','preserved_non_torso_local_error','retrigger_dt0_pose_error','clash_cases']}));print(receipt['report_sha256'])
