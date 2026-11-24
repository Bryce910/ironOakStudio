#!/usr/bin/env python3
"""Compile/run only CPU production scene checks. Never launches the game or writes authoring."""
from pathlib import Path
import hashlib,json,subprocess,sys
N=Path(__file__).resolve().parents[1]
B=N.parent/'apartment-camera-2026-11-19'
OUT=N/'review/forest-scene'
S=N/'source-v1'
OBJ=B/'runtime-native-v1/native-objects/engine'
ROOT=N.parents[1]
LIB=ROOT/'build/physics/libJolt.a'
NAMES=['world_studio','world_surface_authoring','world_surface_field','world_entity_graph','world_shader_graph','world_authoring_identity','world_physics']
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def leaves(p):
 return {str(q.relative_to(p)):sha(q) for q in p.rglob('*') if q.is_file()}
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 inputs={};source_pins={};source_unchanged=True
 # The reused production objects are from the exact published104unit closure.
 # Pin all source bytes, not just names of the selected C++ compilation units.
 for p in (B/'source-v1').rglob('*'):
  if p.is_file():
   q=S/p.relative_to(B/'source-v1');h=sha(p)
   source_unchanged &= q.is_file() and sha(q)==h
   source_pins[str(q)]=sha(q) if q.is_file() else None
 for name in NAMES:
  p=OBJ/(name+'.cpp.o');inputs[str(p)]=sha(p)
 inputs[str(LIB)]=sha(LIB)
 for p in [Path(__file__),N/'tools/check_forest_scene.cpp',B/'runtime-native-v1/READY.json']:
  inputs[str(p)]=sha(p)
 for base in (N,B):
  for rel in ('world.studio','ground/showcase.height','opening/arrival-camp.settings','opening/first-journey.town-people.settings','city.physics','city.scene'):
   p=base/'world-project'/rel
   if p.is_file():inputs[str(p)]=sha(p)
 for rel in ('opening/arrival-camp.settings','opening/first-journey.town-people.settings','city.physics','ground/showcase.height'):
  a=N/'world-project'/rel;b=B/'world-project'/rel
  if a.exists()!=b.exists() or a.is_file() and sha(a)!=sha(b):raise RuntimeError('Protected actor/source terrain/city collision changed: '+rel)
 collision_before=leaves(B/'world-project/collision');collision_after=leaves(N/'world-project/collision')
 collision_unchanged=collision_before==collision_after
 for rel,h in collision_after.items():inputs[str(N/'world-project/collision'/rel)]=h
 before={p:sha(p) for p in inputs}
 cmd=['g++','-std=c++17','-O2','-I'+str(S),str(N/'tools/check_forest_scene.cpp')]+[str(OBJ/(name+'.cpp.o')) for name in NAMES]+[str(LIB),'-pthread','-o',str(OUT/'check-forest-scene')]
 built=subprocess.run(cmd,text=True,capture_output=True)
 (OUT/'build.log').write_text(built.stdout+built.stderr)
 if built.returncode:raise RuntimeError('CPU compile failed; see build.log')
 run=[str(OUT/'check-forest-scene'),str(N/'world-project'),str(B/'world-project')]
 tested=subprocess.run(run,text=True,capture_output=True)
 (OUT/'RAW.json').write_text(tested.stdout)
 (OUT/'run.log').write_text(tested.stderr)
 raw=json.loads(tested.stdout)
 after={p:sha(p) for p in inputs};unchanged=before==after
 receipt={'schema':'valwood.forest-scene-production-cpu-review.v1','passed':tested.returncode==0 and raw['passed'] and unchanged and source_unchanged and collision_unchanged,'gpu_used':False,'project_or_live_modified':False,'production_source_modified':False,'current_source_exact_baseline':source_unchanged,'existing_collision_files_exact_baseline':collision_unchanged,'existing_collision_file_count':len(collision_after),'inputs_unchanged_during_review':unchanged,'inputs_sha256':inputs,'all_source_sha256':source_pins,'build_command':cmd,'run_command':run,'executable_sha256':sha(OUT/'check-forest-scene'),'raw_sha256':sha(OUT/'RAW.json'),'raw_result':raw,'scope':'Production studio decode, entity binding, weather pack/sample, surface/paint/terrain bake, derived river query, dry camp/NPC/control roots, retained road/bridge ground, production Jolt adapter ray queries using declared final-height2m test mesh and production-generated deck triangle geometry.','limitations':['No GPU/rendering certification or gameplay input/camera review.','The Jolt terrain fixture is independently constructed from production final-height samples at2m spacing; it is a CPU test fixture, not the full runtime scenery world.','Tree/rock/prop capsule clearance for newly authored scenery requires the separate actual transformed-mesh clearance audit and native route test. Existing collision source files are unchanged but dynamic scenery blockers are placement-derived.','CPU water/ground checks do not certify artistic shoreline appearance, fish, rain visibility, fog readability, splash behavior or sustained frame pacing.']}
 (OUT/'CPU_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps({'passed':receipt['passed'],'report':str(OUT/'CPU_REVIEW.json'),'sha256':sha(OUT/'CPU_REVIEW.json'),'checks':len(raw['checks']),'failures':[c for c in raw['checks'] if not c['passed']],'creek':raw['creek'],'camp':raw['camp'],'routes':raw['routes'],'jolt':raw['jolt']},indent=2))
 return 0 if receipt['passed'] else 1
if __name__=='__main__':sys.exit(main())
