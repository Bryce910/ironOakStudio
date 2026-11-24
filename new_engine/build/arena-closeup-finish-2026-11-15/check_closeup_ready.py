#!/usr/bin/env python3
"""Accept only the specific third native close-up candidate actually reviewed."""
from pathlib import Path
import hashlib,importlib.util,json,re,struct,sys
N=Path(__file__).resolve().parent;B=N.parent/'arena-orc-finish-2026-11-15';E=N.parents[1];ROOT=E.parent
READY=N/'CLOSEUP_FINISH_READY.json'
BASELINES={
 'baseline-clash-high-0-8':(B/'runtime-native-v1/native-bin/rpg','clash','High'),
 'baseline-knights-high':(N/'runtime-native-v1/native-bin/rpg','knights','High'),
 'baseline-survey-high-0-7':(N/'runtime-native-v1/native-bin/rpg','survey','High'),
}
CANDIDATES={
 'v3-clash-high-0-8':('clash','High'),
 'v3-clash-high-0-1-knight12':('clash','High'),
 'v3-knights-high':('knights','High'),
 'v3-knights-low':('knights','Low'),
 'v3-survey-high-0-7':('survey','High'),
 'v3-survey-high-7-14':('survey','High'),
}
PARTIAL_DIAGNOSTICS=('v3-baseline-clash-high-12-20','v3-clash-high-12-20')
LABELS=list(BASELINES)+list(CANDIDATES)
ORCS={'orc01','orc02','orc03','orc04','orc05','orc06','azrak','costalmed','dardan','girra','gnur','kanit','magur','ursag'}
FACES={'knight11','knight13','knight17'}
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
def need(ok,message):
 if not ok:raise RuntimeError(message)
def hashes(items):
 need(isinstance(items,dict)and bool(items),'Missing reviewed input hashes')
 for name,h in items.items():need(Path(name).is_file()and sha(name)==h,'Reviewed input changed: '+name)
def inherited():
 spec=importlib.util.spec_from_file_location('accepted_orcs',B/'check_orc_ready.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.inherited()
 d=read(B/'ORC_FINISH_READY.json');need(d['ready'],'Accepted Orc baseline incomplete')
 for name,h in d['inputs'].items():
  p=Path(name)
  if p.parent==ROOT and p.name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):p=N/'previous-launchers'/p.name
  need(p.is_file()and sha(p)==h,'Accepted Orc baseline changed: '+name)
 for name,t in d['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==t,'Accepted Orc link changed: '+name)
def capture(path,width=1600,height=900):
 need(path.is_file()and path.stat().st_size>32,'Missing native capture: '+str(path))
 if path.suffix=='.png':
  with path.open('rb')as stream:header=stream.read(24)
  need(header[:8]==b'\x89PNG\r\n\x1a\n'and header[12:16]==b'IHDR'and struct.unpack('>II',header[16:24])==(width,height),'Wrong native PNG dimensions: '+str(path))
def posed(raw,d,label,mode):
 need(raw['capture_posed_not_combatproof']and raw['ordinary_start_and_loading']and not raw['events_injected']and raw['private_profile'],'Posed survey provenance incomplete: '+label)
 count=3 if mode=='knights'else 7;views=2 if mode=='knights'else 4
 need(raw['characters']==raw['expected_characters']==count and raw['captures']==raw['expected_captures']==count*views,'Incomplete posed material survey: '+label)
 need(len(raw['rows'])==count and len(raw['views'])==count*views and len(raw['selected_ids'])==count and len(set(raw['selected_ids']))==count,'Repeated/omitted posed source identity: '+label)
 need({row['id']for row in raw['rows']}==set(raw['selected_ids']),'Posed row identity mismatch: '+label)
 if mode=='knights':need(set(raw['selected_ids'])==FACES,'Uncovered knight faces missing: '+label)
 else:need(set(raw['selected_ids']).issubset(ORCS),'Unknown Orc appearance: '+label)
 expected_views={'face-front','face-three-quarter'}if mode=='knights'else{'fullbody-front','face-front','face-three-quarter','fullbody-back'}
 for row in raw['rows']:
  need(row['GL_errors']==0 and row['compact_loaded']and row['compact_failures']==0 and row['actor_mask']==2 and row['resident_body_assets']==1 and row['health_unchanged'],'Native model rendering/ownership error: '+label)
  need(row['source_guard']=='low'and row['guard_weight']>.99,'Actual source guard not settled: '+label)
 for identity in raw['selected_ids']:
  entries=[view for view in raw['views']if view['id']==identity]
  need(len(entries)==views and {view['view']for view in entries}==expected_views,'Missing/duplicated source view: '+label+' '+identity)
 for view in raw['views']:
  need(view['GL_errors']==0 and view['compact_failures']==0 and view['capture_posed_not_combatproof']and view['width']==1600 and view['height']==900,'Invalid native source view: '+label)
  name=Path(view['file']);need(name.name==view['file']and name.suffix=='.bmp','Invalid capture filename: '+label)
  capture(d/name);capture(d/name.with_suffix('.png'))
def clashes(raw,d,label):
 need(raw['actual_training_events']and raw['actual_owned_geometry']and not raw['events_injected'],'Missing real combat provenance: '+label)
 first,last=(0,1)if label.endswith('knight12')else(12,20)if label.endswith('12-20')else(0,8)
 need(raw['first_case']==first and raw['last_case_exclusive']==last and raw['reviewed_cases']==raw['expected_cases']==last-first and len(raw['cases'])==last-first and raw['captures']==4*(last-first),'Incomplete real combat range: '+label)
 need(len({row['name']for row in raw['cases']})==last-first,'Repeated real combat case: '+label)
 for row in raw['cases']:clash_row(row,d,label)
def clash_row(row,d,label):
 need(row['one_outcome_through_recovery']and row['anatomical']and row['damage']==0 and row['metal_bursts']>0 and row['body_bursts']==0 and row['metal_audio']>0 and row['body_audio']==0,'Steel contact replayed or became body damage: '+label)
 for phase in('before','contact','recoil','recovery'):
  name=row['name']+'-'+phase;capture(d/(name+'.bmp'));capture(d/(name+'.png'))
def paired_fixture_failure(binary):
 # These attempts remain failures. Their exact reproduction on the accepted
 # binary documents an unresolved diagnostic limitation, not weapon coverage.
 report=read(N/'review/PREEXISTING_WEAPON_FIXTURE.json')
 need(report['schema']=='valwood.closeup.preexisting-weapon-fixture.v1'and report['status']=='unresolved_QA_limitation'and not report['native_batch_passed']and not report['complete_weapon_coverage'],'Failed optional weapon diagnostic was relabelled as a pass')
 need(report['labels']==list(PARTIAL_DIAGNOSTICS)and report['unverified_weapons']==['sword_shield','great_axe','staff'],'Partial diagnostic scope changed')
 hashes(report['inputs'])
 need(report['accepted_baseline_binary']==str(B/'runtime-native-v1/native-bin/rpg')and report['candidate_binary']==str(binary),'Partial diagnostic report names another binary')
 expected_sources={'engine/'+name for name in('world_player_inventory_runtime.inc','world_combat_anatomy.h','world_combat_anatomy_runtime.inc','world_skeletal_hero_loadouts.cpp','world_equipment_presentation.h','world_combat_anatomy_actions.inc','world_skeletal_hero_loadouts.h','world_combat_arena_runtime.inc')}
 need(set(report['unchanged_source_sha256'])==expected_sources,'Inherited equip/contact source coverage missing')
 for relative,expected in report['unchanged_source_sha256'].items():
  need(relative.startswith('engine/')and '..'not in Path(relative).parts,'Invalid inherited diagnostic source')
  need(sha(B/'source-v1'/relative)==sha(N/'source-v1'/relative)==expected,'Inherited equip/contact source changed: '+relative)
 paired=[]
 for label in PARTIAL_DIAGNOSTICS:
  d=N/'review'/label;run=read(d/'EXECUTION.json');baseline=label==PARTIAL_DIAGNOSTICS[0];expected_binary=B/'runtime-native-v1/native-bin/rpg'if baseline else binary
  need(run['exit_code']==1 and run['native_passed']is False and run['baseline']==baseline and run['mode']=='clash'and run['quality']=='High'and run['private_saves']and run['native_GPU'],'Preexisting diagnostic failure signature changed: '+label)
  hashes(run['inputs']);command=run['command']
  need(command[0]==str(expected_binary)and command[1:3]==['--project',str(d/'private-project')]and '--hidden'in command and '--frames'in command,'Paired diagnostic used another native execution: '+label)
  need(run['inputs'][str(expected_binary)]==sha(expected_binary)and str(N/'review/run_closeup_review_v3.py')in run['inputs'],'Paired diagnostic binary/driver mismatch: '+label)
  origin=(B if baseline else N)/'arena-project'
  for name in('world_foliage.fs.glsl','world_foliage_shadow.fs.glsl','world_post.fs.glsl','rig_first_person.fs.glsl','world_material_response.glsl'):
   need(sha(d/'private-project/shaders'/name)==sha(origin/'shaders'/name),'Paired diagnostic did not retain its original shaders: '+label)
  need(sha(d/'private-project/world.studio')==sha(origin/'world.studio'),'Paired diagnostic lighting changed: '+label)
  raw=read(d/'NATIVE_CLASH_RAW.json')
  need(raw['passed']is False and raw['error']=='No actual geometric fixture: gnur-shield-clash-third'and raw['partial_scope']and raw['first_case']==12 and raw['last_case_exclusive']==20 and raw['expected_cases']==8 and raw['reviewed_cases']==2 and raw['captures']==8,'Paired native failure differs: '+label)
  need(raw['actual_training_events']and raw['actual_owned_geometry']and not raw['events_injected'],'Partial diagnostic provenance missing: '+label)
  need([row['name']for row in raw['cases']]==['gnur-sword-parry-third','gnur-sword-parry-first'],'Unexpected completed partial cases: '+label)
  for row in raw['cases']:clash_row(row,d,label)
  log=(d/'native.log').read_text()
  need(log.count('Contact rejected: Contact requested before explicit loadout transition seed')==725 and re.search(r'contacts=725 contact_samples=0 contact_failures=725',log),'Deferred-equip failure signature differs: '+label)
  need('Safe legacy fallback:'not in log,'Paired diagnostic failed rendering before its known fixture search: '+label)
  paired.append(raw['cases'])
 for before,after in zip(*paired):
  need(all(before[key]==after[key]for key in before),'Completed paired sword parry behavior changed')
 return report
def hair_binding(gpu,label,mask=False,blend=False):
 hair=gpu['hair'];need(hair['native_forwarded_gl_only']and hair['color_ordinary_draws']>0 and hair['shadow_ordinary_draws']>0,'Ordinary color/shadow witness missing: '+label)
 programs=hair['programs'];need(programs and all(len(p['draws'])==3 and p['last_kind']==0 for p in programs),'Invalid/leaked hair material kind: '+label)
 for required,kind in((mask,1),(blend,2)):
  if not required:continue
  name='mask'if kind==1 else'blend'
  need(hair['color_'+name+'_hair_draws']>0 and hair['shadow_'+name+'_hair_draws']>0,'Owned source hair color/shadow draw missing: '+label)
  need(any(p['color']and p['draws'][kind]>0 for p in programs)and any(not p['color']and p['draws'][kind]>0 for p in programs),'Hair kind absent from actual color/shadow programs: '+label)
 if mask or blend:need(hair['uniform_resets_after_hair']>0 and hair['ordinary_draws_after_reset']>0,'Hair uniform not restored for ordinary rendering: '+label)
def organic_binding(gpu,label,quality,orc_only=False):
 body=gpu['orc_body'] # Historical observer key also covers explicitly owned knight heads.
 need(body['native_forwarded_gl_only']and body['passed']and body['draws_with_organic']>0 and body['complete_texture_draws']==body['draws_with_organic']and body['uniform_reset_observed'],'Organic face/body draw or reset missing: '+label)
 witnesses=body['texture_witnesses'];need({w['role']for w in witnesses}=={'base','normal','arm'},'Organic source map roles missing: '+label)
 limit={'Low':512,'Medium':1024,'High':2048}[quality]
 for witness in witnesses:
  need(witness['enabled']and witness['storage_observed']and witness['encounters']>0,'Organic map not observed at actual draw: '+label)
  maximum=min(limit,1024)if witness['role']=='arm'else limit
  need(0<witness['width']<=maximum and 0<witness['height']<=maximum,'Organic texture profile exceeded: '+label)
  if orc_only:
   need(witness['width']==witness['height']==maximum,'Owned Orc body texture silently downgraded: '+label)
  elif quality=='Low':need(witness['width']==witness['height']==512,'Low owned head texture profile not loaded: '+label)
  elif label in('v3-knights-high','v3-clash-high-0-1-knight12'):
   expected=2048 if witness['role']=='base'else 1024
   need(witness['width']==witness['height']==expected,'High owned head texture profile not loaded: '+label)
 need(all(p['last_uniforms'].get('uOrganicBody',0)==0 for p in body['programs']),'Organic material flag leaked after drawing: '+label)
def texture_budget_proof():
 report=read(N/'review/CHARACTER_TEXTURE_BUDGET_CPU.json')
 need(report['passed']and report['test_exit_code']==0 and report['cpu_only']and report['bank_identities']==673,'Character texture budget CPU proof failed')
 hashes(report['source_and_bank_sha256'])
 need(report['actual_roster_profiles']==54 and report['adapted_profiles']==3 and report['protected_image_checks']>0,'Character texture roster/profile coverage incomplete')
 for field in('high_and_medium_knight12_loadable','medium_to_low_under_tighter_explicit_budget','unchanged_source_bytes','exact_registered_variant_bytes','deterministic_selection','unknown_and_corrupt_fail_closed','derived_arm_and_color_space_accounting','neutral_unchanged','loading_scope_restored'):
  need(report[field],'Texture budget invariant failed: '+field)
 policy=report['budget_policy'];need(policy['High_MiB']==768 and policy['Medium_and_Low_MiB']==256 and policy['caller_ceiling_preserved']and not policy['runtime_resizing']and not policy['draw_time_io']and not policy['original_asset_or_bank_mutations'],'Texture budget ceiling/loading policy changed')
 rows=report['cases'];keys={(row['knight'],row['profile'])for row in rows}
 expected={(f'knight{index:02}',profile)for index in range(1,19)for profile in range(3)}
 need(len(rows)==54 and keys==expected,'All eighteen knights across three profiles were not budgeted')
 adapted={}
 for row in rows:
  need(row['budget']==(768 if row['profile']==2 else 256)*1024*1024 and 0<row['bytes']<=row['budget']and row['bytes']<=row['initial_bytes'],'Character profile exceeds its unchanged ceiling')
  need(len(row['levels'])==row['images']and all(0<=level<=row['profile']for level in row['levels'])and sum(level!=row['profile']for level in row['levels'])==row['downsampled_images'],'Invalid registered texture fallback levels')
  if row['downsampled_images']:adapted[(row['knight'],row['profile'])]=row['downsampled_images']
 need(adapted=={('knight12',2):1,('knight13',2):1,('knight16',2):3},'Texture fallbacks changed beyond the three reviewed High profiles')
 return next(row for row in rows if row['knight']=='knight12'and row['profile']==2)
def native_budget_witness(log,expected):
 # Equipment composition retains the verified selected player's materials but
 # replaces Asset.source_path with its loadout rig path. The native clash raw
 # receipt separately certifies knight12 against the actual player registry;
 # do not infer identity from this composed-asset diagnostic filename.
 pattern=r'^\[Skeletal textures\] profile=High initial_bytes=(\d+) reference_bytes=(\d+) budget_bytes=(\d+) downsampled_images=(\d+) protected_images=(\d+) source=(.+\.glb)\s*$'
 rows=re.findall(pattern,log,re.MULTILINE)
 signature=(expected['initial_bytes'],expected['bytes'],expected['budget'],expected['downsampled_images'],expected['protected_images'])
 need(any(tuple(map(int,row[:5]))==signature for row in rows),'Actual High knight12 protected-face texture fallback was not observed at native loading')
def verify_reviews():
 runtime=N/'runtime-native-v3';binary=runtime/'native-bin/rpg';build=read(runtime/'READY.json')
 need(build['ready']and build['native_only']and not build['Windows_work_performed']and build['application_units']==104 and build['full_application_dependency_closure_verified'],'Native v3 dependency closure incomplete')
 need(build['binary']==str(binary)and sha(binary)==build['sha256'],'Native v3 receipt names another binary')
 need('exec "$arena_stage/runtime-native-v3/native-bin/rpg" --project "$arena_stage/arena-project"'in(N/'launch_arena.sh').read_text(),'Public arena launcher does not select the reviewed native v3/project')
 with binary.open('rb')as stream:need(stream.read(4)==b'\x7fELF','Reviewed demo must remain a native Linux build')
 need({str(p.relative_to(N/'source-v1')):sha(p)for p in(N/'source-v1').rglob('*')if p.is_file()}==build['source_sha256'],'Compiled source changed')
 for file,key in [('review/HAIR_CONTRACT_CHECK.json','source_hashes'),('review/CAMERA_CPU_REVIEW.json','inputs')]:
  report=read(N/file);need(report['passed'],'CPU checks failed: '+file);hashes(report[key])
 contract=read(N/'review/HAIR_CONTRACT_CHECK.json');need(contract['actual_skeletal_bodies_loaded']==18 and contract['organic_head_materials']>0 and contract['posed_organic_head_primitives']>0 and contract['separate_eyes_mouth_hair_hands_excluded'],'Owned face/source classification incomplete')
 reproduction=contract['reproduction'];need(reproduction['test_exit_code']==0,'Owned head/hair classifier reproduction failed')
 for field in('raw_output','binary'):need(sha(reproduction[field])==reproduction[field+'_sha256'],'Classifier reproduction evidence changed: '+field)
 head=read(N/'review/HEAD_SKIN_REVIEW.json');need(head['passed'],'Owned head source/channel proof failed');hashes(head['shader_sha256'])
 for field in('classifier','actual_cpu_loading_receipt'):
  need(sha(head[field]['path'])==head[field]['sha256'],'Owned head classifier receipt changed: '+field)
 for record in head['map_proof']['records']:
  for mapping in record['map_variants']:
   for variant in mapping['variants']:need(sha(variant['path'])==variant['sha256'],'Reviewed head/eye/hair source map changed')
 high_knight12_budget=texture_budget_proof()
 gate=read(N/'review/CLOSEUP_GATE_CPU.json');need(gate['passed']and gate['cpu_only']and gate['complete_candidate_native_runs_validated']==6 and gate['baseline_native_runs_validated']==3 and gate['failed_diagnostics_retained_as_failed']==2 and len(gate['rejected_corruptions'])>=13,'Release gate negative checks incomplete');hashes(gate['inputs'])
 report=read(N/'review/HAIR_SHADER_REVIEW.json');need(report['passed'],'Hair shader/source review failed');hashes(report['shader_sha256'])
 need(all(str(N/'arena-project/shaders'/name)in report['shader_sha256']for name in('world_foliage.fs.glsl','world_foliage_shadow.fs.glsl')),'Actual color/shadow shader proof missing')
 report=read(N/'review/visual-agent/KNIGHT_FACE_INPUTS.json');hashes(report['source_inputs'])
 for model in report['models']:
  for field in('NPC_body','skeletal_body'):
   if field in model:need(sha(model[field])==model[field+'_sha256'],'Owned face source changed: '+model[field])
 visual=read(N/'review/VISUAL_REVIEW.json');need(visual['passed'],'Native v3 visual review incomplete')
 if 'inputs'in visual:hashes(visual['inputs'])
 seen=set()
 for label in LABELS:
  d=N/'review'/label;run=read(d/'EXECUTION.json');is_baseline=label in BASELINES
  expected_binary,mode,quality=BASELINES[label]if is_baseline else(binary,*CANDIDATES[label])
  need(run['exit_code']==0 and run['native_passed']and run['private_saves']and run['native_GPU'],'Native capture failed: '+label)
  need(run['baseline']==is_baseline and run['mode']==mode and run['quality']==quality,'Native capture mode/profile mismatch: '+label)
  hashes(run['inputs']);command=run['command']
  need(command[0]==str(expected_binary)and run['inputs'][str(expected_binary)]==sha(expected_binary),'Wrong native capture binary: '+label)
  need(command[1:3]==['--project',str(d/'private-project')]and '--hidden'in command and '--frames'in command,'Capture did not use private bounded native project: '+label)
  driver=N/'review'/('run_orc_review.py'if is_baseline else'run_closeup_review_v3.py')
  need(str(driver)in run['inputs'],'Native review driver missing from execution hashes: '+label)
  rawfile='ORC_PRESENTATION_RAW.json'if mode=='survey'else'KNIGHT_PRESENTATION_RAW.json'if mode=='knights'else'NATIVE_CLASH_RAW.json'
  raw=read(d/rawfile);need(raw['passed'],'Raw native report failed: '+label);gpu=read(d/'TEXTURE_GPU.json')
  if mode in('survey','knights'):
   posed(raw,d,label,mode)
   if not is_baseline and mode=='survey':seen.update(raw['selected_ids'])
  else:clashes(raw,d,label)
  if not is_baseline:
   need(run['inputs'][str(binary)]==build['sha256'],'Candidate review used an earlier native binary: '+label)
   need(sha(d/'private-project/world.studio')==sha(N/'arena-project/world.studio'),'Reviewed light differs from public arena')
   for name in('world_foliage.fs.glsl','world_foliage_shadow.fs.glsl','world_post.fs.glsl','rig_first_person.fs.glsl','world_material_response.glsl'):
    need(sha(d/'private-project/shaders'/name)==sha(N/'arena-project/shaders'/name),'Reviewed shader differs from public arena: '+name)
   hair_binding(gpu,label,mask=label=='v3-survey-high-0-7',blend=label in('v3-knights-high','v3-knights-low','v3-clash-high-0-1-knight12'))
   organic_binding(gpu,label,quality,orc_only=mode=='survey')
   limit={'Low':512,'High':2048}[quality]
   need(f'[Character textures] Loaded {quality}, color limit {limit}, registered source images 673'in(d/'native.log').read_text(),'Reviewed character texture bank missing: '+label)
   if mode=='clash'and any(row['first_person']for row in raw['cases']):need(gpu['first_person_ibl']['observed']and gpu['first_person_ibl']['scene_ibl_bound'],'First-person scene reflection witness missing: '+label)
   if label=='v3-clash-high-0-1-knight12':
    need(raw['requested_player']=='knight12'and all(row['player']=='knight12'and row['player_registry_identity_verified']for row in raw['cases']),'Actual skeletal hair/face player not verified')
    native_budget_witness((d/'native.log').read_text(),high_knight12_budget)
 need(seen==ORCS,'Fourteen Orcs not reviewed')
 paired_fixture_failure(binary)
 # The gameplay comparison uses the accepted native binary, rather than new
 # camera code with old shaders. Require identical case sets before comparing.
 old=read(N/'review/baseline-clash-high-0-8/NATIVE_CLASH_RAW.json')['cases'];new=read(N/'review/v3-clash-high-0-8/NATIVE_CLASH_RAW.json')['cases']
 need(len(old)==len(new)==8,'Matched combat case count differs')
 for before,after in zip(old,new):
  for key in('name','enemy','weapon','mode','first_person','damage','distance','direction','guard','actual_phase','event_serial','attack_serial','anatomical','metal_bursts','body_bursts','metal_audio','body_audio','one_outcome_through_recovery'):
   need(before[key]==after[key],'Combat behavior changed in framing comparison: '+key)
 for mode,suffix,rawfile in [('knights','knights-high','KNIGHT_PRESENTATION_RAW.json'),('survey','survey-high-0-7','ORC_PRESENTATION_RAW.json')]:
  old=read(N/'review'/('baseline-'+suffix)/rawfile);new=read(N/'review'/('v3-'+suffix)/rawfile)
  need(old['selected_ids']==new['selected_ids']and len(old['views'])==len(new['views']),'Matched source face views differ')
  for before,after in zip(old['views'],new['views']):
   for key in('id','view','camera','source_slot','source_blend','guard_direction','guard_weight'):need(before[key]==after[key],'Material comparison camera/pose mismatch: '+key)
 return build
def publish(write=True):
 need(not READY.exists(),'Existing release receipt is immutable');inherited();verify_reviews()
 pins={}
 def pin(p):pins[str(p.absolute())]=sha(p)
 for folder in('source-v1','arena-project/shaders','arena-project/environments','arena-project/scenery'):
  for p in(N/folder).rglob('*'):
   if p.is_file():pin(p)
 for name in('check_closeup_ready.py','promote_closeup.py','launch_arena.sh','CLOSEUP_FINISH_NOTES.md','SOURCE_BASELINE.json','SOURCE_ORIGIN.json','CACHE_SEED_REVIEW.json','build_native_candidate.py','seed_native_cache.py','seed_v3_cache.py','runtime-native-v1/READY.json','runtime-native-v1/native-bin/rpg','runtime-native-v3/READY.json','runtime-native-v3/native-bin/rpg','arena-project/world.studio'):
  pin(N/name)
 for name in read(N/'review/CHARACTER_TEXTURE_BUDGET_CPU.json')['source_and_bank_sha256']:pin(Path(name))
 reproduction=read(N/'review/HAIR_CONTRACT_CHECK.json')['reproduction']
 for field in('raw_output','binary'):pin(Path(reproduction[field]))
 for p in(N/'review').iterdir():
  if p.is_file()and p.suffix in('.py','.cpp','.so','.json','.png'):pin(p)
 for label in LABELS+list(PARTIAL_DIAGNOSTICS):
  for p in(N/'review'/label).iterdir():
   if p.is_file():pin(p)
 for name in('Launch_Valwood_Arena.sh','Launch_Valwood_Arena_Visual.sh'):pin(ROOT/name)
 links={str(p):str(p.resolve())for p in(N/'arena-project').iterdir()if p.is_symlink()}
 if write:READY.write_text(json.dumps(dict(ready=True,native_only=True,windows_work_performed=False,native_runtime='runtime-native-v3',inputs=pins,project_links=links,complete_candidate_runs=len(CANDIDATES),partial_diagnostics=dict(labels=list(PARTIAL_DIAGNOSTICS),native_batch_passed=False,complete_weapon_coverage=False,unresolved='Deferred equip / fixture ordering prevents shield search; axe/staff are not reached.'),scope='Owned knight head response, source-tagged hair/shadows/fiber shading and collision-checked close-melee third-person framing/HUD. Actual matched face review plus real source-geometry contact regressions. Six complete candidate runs plus separate failed paired weapon diagnostics. Sword/shield, axe and staff contact scenarios are not certified by this review. No new facial animation/cornea, Tomb Raider parity or sustained crowd FPS claim.'),indent=2,sort_keys=True)+'\n')
if __name__=='__main__':
 try:
  if '--preflight'in sys.argv:publish(False);print('PASS native v3 close-up release preflight.');sys.exit(0)
  if '--publish'in sys.argv:publish()
  inherited();receipt=read(READY);need(receipt['ready'],'Close-up release incomplete');hashes(receipt['inputs'])
  for name,target in receipt['project_links'].items():need(Path(name).is_symlink()and str(Path(name).resolve())==target,'Reviewed asset link changed: '+name)
  print('PASS: native v3 close-combat framing, owned faces/hair and inherited arena verified.')
 except(OSError,RuntimeError,ValueError,KeyError,TypeError,IndexError)as ex:sys.exit(str(ex))
