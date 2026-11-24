#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,importlib.util,sys
N=Path(__file__).resolve().parent;E=N.parents[1];ROOT=E.parent;B=N.parent/'arena-character-finish-2026-11-15';READY=N/'ORC_FINISH_READY.json'
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(v,s):
 if not v:raise RuntimeError(s)
def inherited():
 spec=importlib.util.spec_from_file_location('character_baseline',B/'check_character_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.inherited()
 d=read(B/'CHARACTER_FINISH_READY.json');need(d['ready'],'Character baseline incomplete')
 for name,h in d['inputs'].items():
  p=Path(name)
  if p.parent==ROOT and p.name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):p=N/'previous-launchers'/p.name
  need(p.is_file()and sha(p)==h,'Accepted character input changed: '+name)
 for name,t in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==t,'Accepted character link changed: '+name)
def publish(write_receipt=True):
 need(not READY.exists(),'Existing receipt is immutable');inherited()
 build=read(N/'runtime-native-v1/READY.json');need(build['ready']and build['native_only']and not build['Windows_work_performed'],'Native Orc build incomplete')
 need(build['application_units']==104 and build['full_application_dependency_closure_verified'],'Native closure incomplete')
 need({str(p.relative_to(N/'source-v1')):sha(p)for p in(N/'source-v1').rglob('*')if p.is_file()}==build['source_sha256'],'Compiled source changed')
 for name in('ORC_TEXTURE_REVIEW.json','ORC_ORGANIC_CLASSIFICATION.json','review/BODY_RESPONSE_CHECK.json','review/VISUAL_REVIEW.json'):need(read(N/name)['passed'],'Review incomplete: '+name)
 t=read(N/'ORC_TEXTURE_REVIEW.json');need(t['owned_orc_characters']==14 and t['total_bank_identities']==673 and t['high_original_color_normal_and_arm_bytes_preserved'],'Orc texture roster incomplete')
 for report,key in((t,'immutable_inputs_sha256'),(read(N/'ORC_ORGANIC_CLASSIFICATION.json'),'inputs')):
  for name,h in report[key].items():need(sha(name)==h,'CPU review input changed: '+name)
 need(sha(N/'character-textures/textures.manifest')==t['output_manifest_sha256']and sha(N/'prepare_orc_textures.py')==t['script_sha256'],'Texture preparation proof stale')
 visual=read(N/'review/VISUAL_REVIEW.json');need(visual['complete']and visual['accepted_for_current_material_pass']and visual['candidate_character_count']==14 and visual['candidate_native_images_reviewed']==56,'Visual review incomplete')
 for label,row in visual['capture_executions'].items():
  need(sha(N/'review'/label/'EXECUTION.json')==row['execution_sha256']and sha(N/'review'/label/'ORC_PRESENTATION_RAW.json')==row['raw_sha256'],'Visual review execution changed: '+label)
 for name,h in visual['review_image_inputs'].items():need(sha(name)==h,'Review image source changed: '+name)
 for name,h in visual['review_image_outputs'].items():need(sha(N/'review'/name)==h,'Review image changed: '+name)
 labels=['survey-high-0-7','survey-high-7-14','baseline-survey-high-1-5','baseline-survey-high-7-11','approach-high-gnur','approach-medium-orc02','approach-low-orc02','clash-high']
 seen=set()
 for label in labels:
  d=N/'review'/label;run=read(d/'EXECUTION.json');need(run['exit_code']==0 and run['native_passed'],'Native review failed: '+label)
  binary=N/'runtime-native-v1/native-bin/rpg';need(run['command'][0]==str(binary) and run['inputs'].get(str(binary))==build['sha256'],'Wrong native review binary: '+label)
  for name,h in run['inputs'].items():need(sha(name)==h,'Executed input changed: '+name)
  if not run['baseline']:need(sha(d/'private-project/world.studio')==sha(N/'arena-project/world.studio'),'Public arena light differs from review: '+label)
  report='ORC_PRESENTATION_RAW.json'if run['mode']=='survey'else'NATIVE_ROSTER_RAW.json'if run['mode']=='approach'else'NATIVE_CLASH_RAW.json';raw=read(d/report);need(raw['passed'],'Native raw report failed: '+label)
  gpu=read(d/'TEXTURE_GPU.json');body=gpu['orc_body']
  if run['mode']=='survey':
   need(raw['characters']==raw['expected_characters'] and raw['captures']==raw['expected_captures'] and raw['capture_posed_not_combatproof'],'Incomplete posed material survey: '+label)
   for row in raw['rows']:need(row['GL_errors']==0 and row['compact_failures']==0,'Orc rendering failure: '+row['id'])
   if not run['baseline']:seen.update(raw['selected_ids'])
  if not run['baseline'] and run['mode']!='clash':
   need(body['passed']and body['draws_with_organic']>0 and body['uniform_reset_observed'],'Organic body binding/reset missing: '+label)
   limit={'Low':512,'Medium':1024,'High':2048}[run['quality']]
   for w in body['texture_witnesses']:
    if w['enabled']:
     edge=min(limit,1024)if w['role']=='arm'else limit
     need(w['storage_observed']and w['width']==edge and w['height']==edge,'Wrong Orc texture resolution: '+label)
   need({w['role']for w in body['texture_witnesses']if w['enabled']}=={'base','normal','arm'},'Missing Orc body texture witness: '+label)
   need(f"[Character textures] Loaded {run['quality']}, color limit {limit}, registered source images 673"in(d/'native.log').read_text(),'New Orc texture bank not loaded: '+label)
  if run['mode']=='approach':need(raw['rows'][1]['appearance']==('gnur'if label.endswith('gnur')else'orc02')and gpu['first_person_ibl']['scene_ibl_bound'],'Ordinary combat/first-person IBL incomplete: '+label)
  if run['mode']=='clash':need(body['draws_with_organic']==0,'Organic body flag leaked onto knights')
 expected={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'};need(seen==expected,'All fourteen Orcs not reviewed')
 pins={}
 def pin(p):pins[str(p.absolute())]=sha(p)
 for folder in('source-v1','character-textures','arena-project/shaders','arena-project/environments'):
  for p in(N/folder).rglob('*'):
   if p.is_file():pin(p)
 for name in('build_native_candidate.py','seed_native_cache.py','SOURCE_BASELINE.json','SOURCE_ORIGIN.json','CACHE_SEED_REVIEW.json','ORC_TEXTURE_REVIEW.json','ORC_ORGANIC_CLASSIFICATION.json','prepare_orc_textures.py','check_orc_ready.py','launch_arena.sh','promote_orcs.py','ORC_FINISH_NOTES.md','arena-project/world.studio','runtime-native-v1/READY.json','runtime-native-v1/native-bin/rpg','review/run_orc_review.py','review/check_body_response.py','review/BODY_RESPONSE_CHECK.json','review/gl_orc_observer.cpp','review/liborc_observer.so','review/make_review_images.py','review/VISUAL_REVIEW.json','review/ORC_ROSTER.png','review/ORC_COMPARISON.png'):pin(N/name)
 for label in labels:
  for p in(N/'review'/label).iterdir():
   if p.is_file():pin(p)
 for name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):pin(ROOT/name)
 links={str(p):str(p.resolve())for p in(N/'arena-project').iterdir()if p.is_symlink()}
 if write_receipt:READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,reviewed_orcs=sorted(seen),inputs=pins,project_links=links,scope='All14 owned Orcs: exact source-body response, complete texture-quality coverage, face-readable arena lighting. Native posed material survey plus separate ordinary combat proof; no Tomb Raider parity or sustained maximum-crowd performance claim.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--preflight'in sys.argv:
   publish(False);print('PASS: native Orc release preflight; launchers unchanged.');sys.exit(0)
  if '--publish'in sys.argv:publish()
  inherited();d=read(READY);need(d['ready'],'Orc presentation not accepted')
  for name,h in d['inputs'].items():need(Path(name).is_file()and sha(name)==h,'Reviewed Orc input changed: '+name)
  for name,t in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==t,'Orc asset link changed: '+name)
  print('PASS: all14 reviewed Orcs, organic materials, complete texture options and inherited combat.')
 except(OSError,RuntimeError,ValueError,KeyError)as e:sys.exit(str(e))
