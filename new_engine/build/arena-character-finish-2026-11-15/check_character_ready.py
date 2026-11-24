#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,importlib.util,sys
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent
B=N.parent/'arena-woodland-2026-11-15';READY=N/'CHARACTER_FINISH_READY.json'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(v,s):
 if not v:raise RuntimeError(s)
def inherited():
 spec=importlib.util.spec_from_file_location('woodland_review',B/'check_woodland_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.inherited()
 d=read(B/'WOODLAND_READY.json');need(d['ready'],'Woodland baseline incomplete')
 for name,h in d['inputs'].items():
  p=Path(name)
  if p.parent==ROOT and p.name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):p=N/'previous-launchers'/p.name
  need(p.is_file()and sha(p)==h,'Accepted woodland input changed: '+name)
 for name,target in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Accepted woodland asset link changed: '+name)
def publish():
 need(not READY.exists(),'Existing receipt is immutable');inherited()
 build=read(N/'runtime-native-v1/READY.json');need(build['ready']and build['native_only']and not build['Windows_work_performed'],'Native character build incomplete')
 need(build['application_units']==104 and build['full_application_dependency_closure_verified'],'Native application closure not verified')
 need({str(p.relative_to(N/'source-v1')):sha(p)for p in(N/'source-v1').rglob('*')if p.is_file()}==build['source_sha256'],'Compiled source changed')
 scope=read(N/'review/SOURCE_SCOPE_CHECK.json');origin=read(N/'SOURCE_ORIGIN.json')['baseline_sha256'];current=build['source_sha256']
 actual=sorted(p for p in origin if origin[p]!=current.get(p))
 need(scope['passed']and actual==scope['changed']and not set(current).symmetric_difference(origin),'Native source scope record stale')
 need(read(N/'HEAD_OCCLUSION_REVIEW.json')['passed'],'Head texture repair incomplete')
 need(read(N/'ORC_OCCLUSION_REVIEW.json')['passed'],'Orc occlusion repair incomplete')
 need(read(N/'review/RESPONSE_CHECK.json')['passed'],'Response check incomplete')
 need(read(N/'review/VISUAL_REVIEW.json')['passed'],'Visual review incomplete')
 for label,report in [('release-high','NATIVE_CLASH_RAW.json'),('release-approach-high','NATIVE_ROSTER_RAW.json'),('release-approach-medium','NATIVE_ROSTER_RAW.json'),('release-approach-low','NATIVE_ROSTER_RAW.json')]:
  run=read(N/'review'/label/'EXECUTION.json');raw=read(N/'review'/label/report)
  need(run['exit_code']==0 and raw['passed'],'Native rehearsal failed: '+label)
  executable=N/'runtime-native-v1/native-bin/rpg';private=N/'review'/label/'private-project'
  need(run['command'][0]==str(executable) and run['inputs'].get(str(executable))==build['sha256'],'Rehearsal used another binary: '+label)
  need(sha(private/'world.studio')==sha(N/'arena-project/world.studio'),'Public scene differs from rehearsal: '+label)
  for name,h in run['inputs'].items():need(sha(name)==h,'Executed input changed: '+name)
  gpu=read(N/'review'/label/'TEXTURE_GPU.json')
  need(gpu['first_person_ibl']['observed']and gpu['first_person_ibl']['scene_ibl_bound'],'First-person scene IBL inactive: '+label)
  quality='Low' if label.endswith('low') else 'Medium' if label.endswith('medium') else 'High'
  limit={'Low':512,'Medium':1024,'High':2048}[quality]
  log=(N/'review'/label/'native.log').read_text()
  need(f'[Character textures] Loaded {quality}, color limit {limit}, registered source images 602' in log,'Requested texture bank not loaded: '+label)
  need('failures=0' in log and 'First person arms target failed' not in log,'First-person rendering failed: '+label)
  if label.startswith('release-approach'):
   need(len(raw['rows'])==2 and raw['rows'][1]['appearance']=='orc02' and raw['rows'][1]['GL_errors']==0,'Affected Orc02 not rehearsed: '+label)
 baseline=read(N/'review/baseline-high/EXECUTION.json')
 need(baseline['exit_code']==0 and read(N/'review/baseline-high/NATIVE_CLASH_RAW.json')['passed'],'Matched baseline incomplete')
 for name,h in baseline['inputs'].items():need(sha(name)==h,'Baseline capture input changed: '+name)
 pins={}
 def pin(p):pins[str(p.absolute())]=sha(p)
 for folder in('source-v1','character-textures','arena-project/shaders','arena-project/environments'):
  for p in(N/folder).rglob('*'):
   if p.is_file():pin(p)
 for name in('build_native_candidate.py','seed_native_cache.py','SOURCE_BASELINE.json','SOURCE_ORIGIN.json','CACHE_SEED_REVIEW.json','check_character_ready.py','launch_arena.sh','prepare_head_occlusion.py','HEAD_OCCLUSION_REVIEW.json','prepare_orc_occlusion.py','ORC_OCCLUSION_REVIEW.json','CHARACTER_FINISH_NOTES.md','arena-project/world.studio','runtime-native-v1/READY.json','runtime-native-v1/native-bin/rpg','review/check_response.py','review/RESPONSE_CHECK.json','review/VISUAL_REVIEW.json','review/SOURCE_SCOPE_CHECK.json','review/summarize_review.py','review/MATCHED_COMPARISON.png','review/run_matched.py','review/run_final.py','review/run_release.py','review/gl_character_observer.cpp','review/libcharacter_observer.so'):
  pin(N/name)
 for label in('baseline-high','release-high','release-approach-high','release-approach-medium','release-approach-low'):
  for p in(N/'review'/label).iterdir():
   if p.is_file():pin(p)
 for name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):pin(ROOT/name)
 pin(N/'promote_character.py');pin(N/'review/APPROACH_CONTACTSHEET.png')
 links={str(p):str(p.resolve())for p in(N/'arena-project').iterdir()if p.is_symlink()}
 READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,inputs=pins,project_links=links,scope='Character material/normal/AO calibration, real first-person scene IBL, static arena probe capture and continuous HDR highlight shoulder. Matched native review, not a claim of Tomb Raider parity or sustained maximum-crowd performance.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--publish'in sys.argv:publish()
  inherited();d=read(READY);need(d['ready'],'Character presentation is not accepted')
  for name,h in d['inputs'].items():need(Path(name).is_file()and sha(name)==h,'Reviewed character input changed: '+name)
  for name,t in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==t,'Character asset link changed: '+name)
  print('PASS: reviewed native character materials, scene reflections and Low/Medium/High texture options.')
 except(OSError,RuntimeError,ValueError,KeyError)as e:sys.exit(str(e))
