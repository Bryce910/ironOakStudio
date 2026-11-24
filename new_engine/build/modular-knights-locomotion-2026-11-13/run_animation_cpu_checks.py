from pathlib import Path
import hashlib,json,subprocess,datetime
root=Path(__file__).resolve().parents[2]
base=Path(__file__).resolve().parent
asset=root/'assets/purchased/modular_knights/derived/polish-v4.1/modular_knight_hero_v4_1.glb'
files=[base/'source-v1/engine/world_skeletal_hero.h',base/'source-v1/engine/world_skeletal_hero.cpp',base/'test_skeletal_hero.cpp',base/'source-v1/engine/world_animation.h',base/'source-v1/engine/world_animation.cpp',base/'source-v1/engine/world_training_combat.h',asset]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fingerprint():return {str(p):sha(p) for p in files}
before=fingerprint();started=datetime.datetime.now(datetime.timezone.utc).isoformat()
command=['/usr/bin/x86_64-linux-gnu-g++-13','-std=c++17','-O2','-Wall','-Wextra','-I'+str(base/'source-v1'),str(base/'test_skeletal_hero.cpp'),str(base/'source-v1/engine/world_skeletal_hero.cpp'),str(base/'source-v1/engine/world_animation.cpp'),'-o',str(base/'test_skeletal_hero')]
subprocess.run(command,check=True)
result=subprocess.run([str(base/'test_skeletal_hero'),str(asset)],capture_output=True,text=True,check=True)
assert before==fingerprint(),'Source/asset changed during checks'
report=json.loads(result.stdout)
report.update(schema='valwood.skeletal-hero-cpu-regression.locomotion-v4.1',scope='CPU regression only; rendered/world/hardware acceptance is separate.',source_sha256=before,test_executable_sha256=sha(base/'test_skeletal_hero'),support_test_rates_hz=[30,60,120],support_test_production_speeds_mps={'free_jog':3.5,'sprint':5.95,'combat_side_and_back':2.8,'analog_walk':1.65},test_scopes={'grip_stress':'Upper-body transitions with grounding disabled; grounded upper-body preservation checked separately.','run_parity':'Protected pelvis/spine/upper-body two-cycle Run; legs use matched support phase.','cross_rate_sole_variance':'Diagnostic only; no blanket frame-rate pose-equivalence acceptance.'},known_limits=['Captured Turn clips are imported but not played; stationary yaw releases foot locks.','Support and free-foot trajectories can differ by render sampling; independent cross-rate scene review is required.','Pelvis/root physics stay unchanged; no full-body balance or general stair IK is claimed.','Supported third-person longsword candidate only.'])
p=base/'hero-animation-polish-check.json';p.write_text(json.dumps(report,indent=2)+'\n')
inputs=dict(schema='valwood.animation-cpu-test-inputs.v1',started_utc=started,finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),before_sha256=before,after_sha256=fingerprint(),source_unchanged=True,compile_command=command,execution_command=[str(base/'test_skeletal_hero'),str(asset)],test_executable_sha256=sha(base/'test_skeletal_hero'),report=str(p),report_sha256=sha(p),returncode=result.returncode)
(base/'ANIMATION_TEST_INPUTS.json').write_text(json.dumps(inputs,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['accepted','grip_transition_samples','support_samples','settled_support_samples','settled_support_anchor_error_m','flat_boot_penetration_m','lateral_reversal_first_local_sole_step_m']}));print(inputs['report_sha256'])
