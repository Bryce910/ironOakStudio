#!/usr/bin/env python3
"""Compile/run only CPU production scene checks. Never launches the game or writes authoring."""
from pathlib import Path
import hashlib,json,re,shlex,subprocess,sys
from review_controlled_tree_pairs import audit as audit_tree_pairs
from review_production_scene_parser import audit as audit_scene_parser
N=Path(__file__).resolve().parents[1]
B=N.parent/'river-swim-2026-11-19'
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
def collision_rows(project):
 # Derive the same collision ownership from the unmodified production table
 # and document asset overrides. Unknown aliases fail rather than disappear.
 source=(S/'engine/world_scenery.cpp').read_text()
 collision={m[0]:m[1]!='None' for m in re.findall(r'\{"([^"]+)",[^\n]*?Collision::(\w+)',source)}
 definitions={}
 for line in (project/'world.studio').read_text().splitlines():
  q=shlex.split(line)
  if q and q[0]=='asset':
   collision[q[1]]=int(q[4])!=0
   definitions[q[1]]=q
 solids={};allrows={};unknown=[]
 for file in sorted((project/'scenery').glob('*.txt')):
  rows=[];solid=[]
  for number,line in enumerate(file.read_text().splitlines(),1):
   q=line.split()
   if not q or q[0].startswith('#'):continue
   if q[0] not in collision:unknown.append((str(file),number,q[0]))
   elif collision[q[0]]:solid.append(line)
   rows.append(line)
  solids[file.name]=solid;allrows[file.name]=rows
 return solids,allrows,unknown,{alias:q for alias,q in definitions.items()if collision[alias]}
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 inputs={};source_pins={};source_unchanged=True;shader_derivatives={}
 # The reused production objects are from the exact published104unit closure.
 # Pin all non-shader source bytes, not just selected compilation units.
 # The two intentional runtime GLSL derivatives are separately reviewed;
 # historical source-v1 shader copies were not the production shader bank.
 for p in (B/'source-v1').rglob('*'):
  if p.is_file():
   q=S/p.relative_to(B/'source-v1');h=sha(p)
   if p.relative_to(B/'source-v1').parts[0]=='shaders':
    if not q.is_file() or sha(q)!=h:shader_derivatives[str(q)]={'historical_baseline':h,'private_current':sha(q)if q.is_file()else None}
    continue
   source_unchanged &= q.is_file() and sha(q)==h
   source_pins[str(q)]=sha(q) if q.is_file() else None
 if set(str(p.relative_to(S))for p in S.rglob('*')if p.is_file()and p.relative_to(S).parts[0]!='shaders')!=set(str(p.relative_to(B/'source-v1'))for p in(B/'source-v1').rglob('*')if p.is_file()and p.relative_to(B/'source-v1').parts[0]!='shaders'):source_unchanged=False
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
 oldsolids,oldrows,oldunknown,olddefs=collision_rows(B/'world-project')
 newsolids,newrows,newunknown,newdefs=collision_rows(N/'world-project')
 controlled_updates,tree_review=audit_tree_pairs(N,B,sha)
 expected_solids={file:[controlled_updates.get((file,row),row)for row in rows]for file,rows in oldsolids.items()}
 matched_updates=sum((file,row)in controlled_updates for file,rows in oldsolids.items()for row in rows)
 preserved_solid_count=sum(len(rows)for rows in oldsolids.values())-matched_updates
 solid_placements_unchanged=expected_solids==newsolids and olddefs==newdefs and not oldunknown and not newunknown and matched_updates==12 and preserved_solid_count==1685
 inputs.update(tree_review['inputs_sha256'])
 for rel in ['review/forest-scene/CONTROLLED_TREE_REVIEW.json','review/forest-scene/controlled-tree-queries.txt']:
  inputs[str(N/rel)]=sha(N/rel)
 parser_path=OUT/'PRODUCTION_SCENE_PARSER_REVIEW.json'
 parser_review=json.loads(parser_path.read_text())if parser_path.is_file()else{}
 parser_current=parser_review.get('passed')and all(Path(p).is_file()and sha(p)==h for p,h in parser_review.get('inputs_sha256',{}).items())
 if not parser_current:parser_review=audit_scene_parser(N,B,sha)
 inputs.update(parser_review['inputs_sha256'])
 for p in [parser_path,OUT/'production-scene-parser.cpp',OUT/'production-scene-parser']:
  inputs[str(p)]=sha(p)
 for base in (N,B):
  for p in(base/'world-project/scenery').glob('*.txt'):inputs[str(p)]=sha(p)
 for rel in ['source-v1/engine/world_scenery.cpp','review/camp-composition/COMPOSITION_PATCH.json']:
  inputs[str(N/rel)]=sha(N/rel)
 before={p:sha(p) for p in inputs}
 cmd=['g++','-std=c++17','-O2','-I'+str(S),str(N/'tools/check_forest_scene.cpp')]+[str(OBJ/(name+'.cpp.o')) for name in NAMES]+[str(LIB),'-pthread','-o',str(OUT/'check-forest-scene')]
 built=subprocess.run(cmd,text=True,capture_output=True)
 (OUT/'build.log').write_text(built.stdout+built.stderr)
 if built.returncode:raise RuntimeError('CPU compile failed; see build.log')
 run=[str(OUT/'check-forest-scene'),str(N/'world-project'),str(B/'world-project'),tree_review['queries_path']]
 tested=subprocess.run(run,text=True,capture_output=True)
 (OUT/'RAW.json').write_text(tested.stdout)
 (OUT/'run.log').write_text(tested.stderr)
 raw=json.loads(tested.stdout)
 after={p:sha(p) for p in inputs};unchanged=before==after
 receipt={'schema':'valwood.forest-scene-production-cpu-review.v3','passed':tested.returncode==0 and raw['passed'] and unchanged and source_unchanged and collision_unchanged and solid_placements_unchanged and tree_review['passed'] and parser_review['passed'],'gpu_used':False,'project_or_live_modified':False,'production_Cpp_source_modified':False,'current_nonshader_source_exact_baseline':source_unchanged,'intentional_source_shader_derivatives':shader_derivatives,'existing_collision_files_exact_baseline':collision_unchanged,'existing_collision_file_count':len(collision_after),'solid_placements_match_exact_controlled_12_pair_manifest':solid_placements_unchanged,'unmodified_solid_placements':preserved_solid_count,'controlled_tree_pairs':tree_review,'production_placement_parser':parser_review,'solid_placement_counts':{name:len(rows)for name,rows in newsolids.items()},'unknown_scene_aliases':oldunknown+newunknown,'inputs_unchanged_during_review':unchanged,'inputs_sha256':inputs,'nonshader_source_sha256':source_pins,'build_command':cmd,'run_command':run,'executable_sha256':sha(OUT/'check-forest-scene'),'raw_sha256':sha(OUT/'RAW.json'),'raw_result':raw,'scope':'Production studio decode, entity binding, weather pack/sample, surface/paint/terrain bake, derived river query, dry camp/NPC/control roots, retained road/bridge ground, production Jolt adapter ray queries using declared final-height2m test mesh and production-generated deck triangle geometry; exactly12 manifest-controlled off-route tree replacements, unchanged remaining1685solid rows and collision-bearing asset definitions, exact production trunk-bound constructor and independently transformed full-mesh route clearance; actual full production placement-parser loops with production atlas/pond/river/water callbacks, and rejection of the original failed shoreline file at line3097.','limitations':['No GPU/rendering certification or gameplay input/camera review.','The Jolt terrain fixture is independently constructed from production final-height samples at2m spacing; it is a CPU test fixture, not the full runtime scenery world.','The controlled tree adapter executes the production trunk-bound constructor without loading a GPU scenery scene. Existing trunk cylinders approximate the central trunk and do not reproduce decorative low root flares as triangle-exact collision.','CPU water/ground checks do not certify artistic shoreline appearance, fish, rain visibility, fog readability, splash behavior or sustained frame pacing.']}
 (OUT/'CPU_REVIEW.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps({'passed':receipt['passed'],'report':str(OUT/'CPU_REVIEW.json'),'sha256':sha(OUT/'CPU_REVIEW.json'),'checks':len(raw['checks']),'failures':[c for c in raw['checks'] if not c['passed']],'creek':raw['creek'],'camp':raw['camp'],'routes':raw['routes'],'jolt':raw['jolt']},indent=2))
 return 0 if receipt['passed'] else 1
if __name__=='__main__':sys.exit(main())
