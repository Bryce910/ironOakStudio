"""Verify the data-only apartment repair over the immutable stance runtime."""
from pathlib import Path
import argparse,hashlib,json,os,sys
N=Path(__file__).resolve().parent;E=N.parents[1];BASE=N.with_name('opening-stance-2026-11-18');READY=N/'UNIFIED_READY.json'
NATIVE=('apartment-walk','opening-natural')
CPU=E/'docs/apartment-collision/2026-11-18/cpu/CPU_REVIEW.json'
def sha(p):
 with Path(p).open('rb') as h:return hashlib.file_digest(h,'sha256').hexdigest()
def need(ok,msg):
 if not ok:raise RuntimeError(msg)
def read(p):
 with Path(p).open() as h:return json.load(h)
def ignored_cache(path):
 p=Path(path)
 return '__pycache__' in p.parts and p.suffix in ('.pyc','.pyo')
def stamp(p):
 s=Path(p).stat();return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns]
def verify_hashes(pins):
 for path,digest in pins.items():need(sha(path)==digest,'Reviewed input changed '+str(path))
def verify_pins(d):
 for path,digest in d['pins'].items():
  s=stamp(path)
  if s[2]<1024*1024 or s!=d.get('stats',{}).get(path):need(sha(path)==digest and stamp(path)==s,'Pinned dependency changed '+path)
def verify_chain(base):
 seen=set()
 while (base/'UNIFIED_READY.json').is_file():
  need(base not in seen,'Baseline cycle');seen.add(base);d=read(base/'UNIFIED_READY.json');need(d['ready'],'Baseline not ready')
  if base.name=='unified-runtime-2026-11-16':d=dict(d,pins={p:h for p,h in d['pins'].items() if p!=str(E.parent/'CURRENT_VALWOOD.json')})
  verify_pins(d)
  if not d.get('baseline'):break
  path=Path(d['baseline']);base=path if path.is_absolute() else base.with_name(str(path))
def resources():
 from launch import configuration,AREAS
 modes=[configuration(m) for m in ('demo','editor','arena','joust','world')]
 need(len({x[0][0] for x in modes})==1,'Modes use different executables')
 for p in [x[2] for x in modes]+[p for p,_ in AREAS.values()]:
  need((p/'world.studio').is_file() and (p/'project.valwood').is_file(),'Missing scene '+str(p))
  need((p/'shaders').resolve()==(N/'shared/shaders').resolve(),'Shader mismatch '+str(p))
  need((p/'character-textures').resolve()==(N/'shared/character-textures').resolve(),'Character bank mismatch '+str(p))
  need((p/'saves').is_dir() and not (p/'saves').is_symlink(),'Invalid save directory '+str(p))
def exact_runtime():
 baseline=read(N/'BASELINE.json');base_ready=read(BASE/'UNIFIED_READY.json');origin=read(N/'runtime/ORIGIN_BUILD.json')
 need(Path(baseline['baseline'])==BASE and baseline.get('source_unchanged') is True and baseline.get('runtime_reused') is True,'Wrong data-only baseline')
 need(sha(BASE/'UNIFIED_READY.json')==baseline['ready_sha256'],'Baseline readiness differs')
 binary=sha(N/'runtime/rpg')
 need(binary==baseline['binary_sha256']==base_ready['one_binary']==sha(BASE/'runtime/rpg'),'Runtime differs from accepted stance runtime')
 need(sha(N/'runtime/ORIGIN_BUILD.json')==sha(BASE/'runtime/ORIGIN_BUILD.json') and origin.get('ready') is True and origin.get('application_units')==104 and origin['sha256']==binary,'Runtime origin differs')
 need((N/'source-v1').resolve()==(BASE/'source-v1').resolve(),'Source must remain the exact baseline source')
 expected={p:h for p,h in origin['source_sha256'].items() if not ignored_cache(p)}
 actual={str(p.relative_to(N/'source-v1')):sha(p) for p in (N/'source-v1').rglob('*') if p.is_file() and not ignored_cache(p.relative_to(N/'source-v1'))}
 need(actual==expected,'Data-only generation changed source inventory')
 return binary,expected

def evidence(binary):
 static=read(N/'review/STATIC_ALIGNMENT_REVIEW.json');cpu=read(CPU);patch=read(N/'review/PLACEMENT_PATCH.json');candidate_collision=read(N/'review/CANDIDATE_COLLISION_REVIEW.json')
 refit=read(N/'review/DEPENDENT_REFIT.json');geometry=read(N/'review/DEPENDENT_REFIT_GEOMETRY_CHECK.json');publication=read(N/'review/DEPENDENT_REFIT_PUBLICATION.json');final=read(N/'review/FINAL_REVIEW.json')
 for name,value in [('static alignment',static),('production collision',cpu),('placement patch',patch),('candidate collision',candidate_collision),('dependent refit',refit),('dependent geometry',geometry),('final review',final)]:need(value.get('passed') is True,'Review failed '+name)
 need(final.get('native_visual_passed') is True,'Final native visual review incomplete')
 c=static['checks'];need(c['source_manifest_records']==593 and c['changed_rows']==593 and c['all_after_rows_match_manifest_world_position'] is True and c['maximum_xyz_error_m']<.00001 and c['physics_and_settings_exact_original'] is True and c['physics_body_count']==15,'Apartment alignment/physics review incomplete')
 need(len(patch['changes'])==593 and patch.get('all_other_fields_preserved') is True and patch.get('other_rows_preserved') is True and patch.get('physics_unchanged') is True,'Placement patch exceeds reviewed scope')
 verify_hashes(static['reviewed_inputs_sha256']);verify_hashes(cpu['read_only_inputs_sha256']);verify_hashes(cpu['evidence_files'])
 raw=cpu['raw_result'];need(cpu.get('production_support') is True and cpu.get('production_collision_unchanged') is True and raw.get('passed') is True and raw.get('checks',0)>=300 and raw.get('failures')==0 and raw.get('collision_bodies')==15 and raw.get('connected_route_passed') is True and raw.get('query_heap_fallbacks')==0 and cpu.get('baseline_old_visible_door_directions_blocked')==20 and cpu.get('restored_door_directions_clear')==20,'Production capsule regression incomplete')
 candidate_raw=candidate_collision['raw'];need(candidate_collision.get('production_capsule_unchanged') is True and candidate_collision['source_evidence']==str(CPU) and candidate_collision['source_evidence_sha256']==sha(CPU) and candidate_collision['command'][1]==str(N/'world-project/opening/home.settings') and candidate_raw.get('passed') is True and candidate_raw.get('production_support') is True and candidate_raw.get('checks',0)>=300 and candidate_raw.get('failures')==0 and candidate_raw.get('collision_bodies')==15 and candidate_raw.get('connected_route_passed') is True and candidate_raw.get('query_heap_fallbacks')==0,'Candidate production collision regression incomplete')
 need(refit['corrected_scenery_untouched_sha256']==sha(N/'world-project/scenery/authored_instances.txt'),'Refit changed corrected placements')
 need(publication['project_files']==refit['publication_files'] and publication['shared_shader']==refit['shader'],'Dependent publication recipe differs')
 need(refit.get('world_studio_other_lines_preserved') is True and refit['shader'].get('all_other_bytes_preserved') is True and geometry.get('opening_config_scene_and_cinematic_documents_match') is True,'Dependent refit scope incomplete')
 need(len(refit['geometry'])==2 and len(geometry.get('checks',[]))==2,'Both overlay LODs must be reviewed')
 files=[]
 for row in publication['project_files']:
  p=Path(row['candidate']);need(p==N/'world-project'/row['relative'] and sha(p)==row['after_sha256'],'Dependent snapshot differs '+row['relative']);files.append(p)
  if row.get('new_unique_asset'):
   asset=Path(row['asset']);need(asset.is_relative_to(N/'assets/apartment-aligned') and sha(asset)==row['after_sha256'],'New aligned asset differs');files.append(asset)
 need(sum(bool(row.get('new_unique_asset')) for row in publication['project_files'])==2,'Expected exactly two aligned overlay assets')
 shader=Path(publication['shared_shader']['candidate']);need(shader==N/'shared/shaders/world_foliage.fs.glsl' and sha(shader)==publication['shared_shader']['after_sha256'],'Dependent shader differs');files.append(shader)
 for row in refit['geometry']:need(row.get('all_other_binary_and_json_preserved') is True and sha(row['candidate_asset'])==row['after_sha256'],'Overlay geometry differs')
 for row in geometry['checks']:need(row['screen_fit_max_error_m']<=geometry['screen_fit_tolerance_m'] and row['screen_plane_clearance_m']>0,'Television overlay no longer fits')
 alignment=Path(refit['alignment_review']);need(sha(alignment)==refit['alignment_review_sha256'],'Dependent alignment diagnosis changed');files.append(alignment)
 for name in NATIVE:
  r=read(N/'review'/name/'EXECUTION.json');need(r.get('passed') is True and r.get('binary_sha256')==binary and r.get('exit_code')==0 and r.get('world',{}).get('gl_error')==0 and not r.get('timed_out',False),'Native review failed '+name)
  if name=='apartment-walk':
   q=r['world'].get('opening_prologue_qa',{});need(r.get('test')=='apartment' and q.get('passed') is True and 'room_lights_walked_all_rooms' in q.get('checks',[]) and 'console_only_complete' in q.get('checks',[]) and q.get('qa_player_pose_writes')==0 and q.get('qa_story_state_writes')==0,'Actual apartment room/console controls incomplete')
  else:
   q=r.get('raw',{});need(r.get('test')=='opening' and q.get('passed') is True and q.get('natural_mode') is True and q.get('controller_skip') is False and q.get('shot_mask')==15 and q.get('settled_camera_checked') is True and q.get('early_visible_body_verified') is True and q.get('qa_story_state_writes')==0 and q.get('qa_player_pose_writes')==0,'Natural opening was not exercised')
 return files+[CPU]+[Path(p) for p in cpu['evidence_files']]

def immutable_review_file(p):
 rel=p.relative_to(N)
 if ignored_cache(rel) or '.logs' in rel.parts or 'saves' in rel.parts or p.suffix in ('.o','.log','.logs'):return False
 # Publication writes these receipts after freezing the generation. Save/progress
 # hash lists remain immutable evidence and are deliberately included.
 return p.name not in ('PUBLICATION.json','PROMOTION.json','PUBLISH_RECEIPT.json','POST_PUBLICATION.json')
def publish():
 need(not READY.exists(),'Published generations are immutable');resources();verify_chain(BASE);binary,source=exact_runtime();dependencies=evidence(binary)
 files=[N/'runtime/rpg',N/'runtime/ORIGIN_BUILD.json',N/'launch.py',Path(__file__),N/'GUIDE.txt',N/'BASELINE.json',N/'run_native_review.py',N/'review/PROGRESS_AFTER_CLOSE.json',BASE/'UNIFIED_READY.json',*dependencies]
 files += [N/'source-v1'/p for p in source]
 files += [N/'world-project'/p for p in ('world.studio','scenery/authored_instances.txt','opening.config','opening.scene','opening/cinematic.settings','opening/cinematic.json','opening/home.physics','opening/home.settings','opening/interior.settings','opening/room-controls.settings','opening/room-lights.settings','opening/game-start.settings','opening/enclosure.json','opening/partition-returns.json')]
 for folder in ('tools','shared/shaders','assets/apartment-aligned','review'):
  files += [p for p in (N/folder).rglob('*') if p.is_file() and immutable_review_file(p)]
 files=sorted(set(files));need(all(p.is_file() for p in files),'Publication input missing')
 # No mutable live scene or player save enters this immutable pin set.
 need(not any((E/'projects').resolve() in p.resolve().parents or 'saves' in p.relative_to(N).parts for p in files if p.is_relative_to(N)),'Mutable authoring/save path in publication pins')
 pins={str(p):sha(p) for p in files};stats={str(p):stamp(p) for p in files};need(all(stamp(p)==stats[str(p)] for p in files),'Publication inputs changed while freezing')
 temporary=READY.with_suffix('.json.tmp');temporary.write_text(json.dumps(dict(ready=True,one_binary=binary,pins=pins,stats=stats,baseline=str(BASE),baseline_ready_sha256=sha(BASE/'UNIFIED_READY.json'),runtime_reused=True,source_unchanged=True,native_only=True,Windows_work_performed=False,mutable_authoring_and_saves_not_frozen=True),indent=2)+'\n');os.replace(temporary,READY)
def check():
 d=read(READY);need(d['ready'],'Not ready');resources();verify_pins(d);verify_chain(BASE)
 m=read(E.parent/'CURRENT_VALWOOD.json');need(Path(m['runtime_generation'])==N and m['binary_sha256']==d['one_binary'],'Current pointer differs')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--publish',action='store_true');p.add_argument('--check-only',action='store_true');a=p.parse_args()
 try:publish() if a.publish else check();print('Apartment collision alignment generation verified.')
 except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
