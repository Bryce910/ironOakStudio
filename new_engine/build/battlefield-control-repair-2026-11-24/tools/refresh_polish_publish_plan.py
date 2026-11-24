#!/usr/bin/env python3
"""Refresh only receipted N3 art postimages, retaining the published CAS baseline.
Default is read-only. --apply atomically replaces only review/PUBLISH_PLAN.json.
It grants no native acceptance and never writes the live world or manifest.
"""
from __future__ import annotations
import argparse,ast,copy,datetime as dt,hashlib,json,os,stat,secrets
from pathlib import Path,PurePosixPath
N=Path(__file__).resolve().parents[1]
PUBLISHER_SHA='8ab4bde9f15af2fe0ca828578d23f7d65fd8ee7859f7c5da74a58409923ef596'
SETUP_SHA='f086972abb5dcc132b3a2d6e04730191426de598f2b8cb9897ea2674697eec2c'
V1_RECEIPT_SHA='fb316dab80b7063e4e726ada2eb4f951c410e037b6ce8a4210022ad829d42c2d'
V2_INDEPENDENT_SHA='1be71d6eba9000bf9d8656b32c171962945429d4dc24871f6dd3abe22bc5fd14'
V2_MANIFEST_SHA='f033488182c36f7966db3ff7913900d1ed9603123e52adaf58f9b56ced485728'
V1_WORLD='c2080e36d41f7614d2ab0ba8df5f13d1c5411cc6dc446bbc4e281af27f8b5511'
V2_WORLD='cd6a8c1667e3aef50c0611501c3ad27ccac51d1a4d3ce467f0d1246e8655ffdf'
V1_INSTANCES='0f4392b039986527f7eff146a707409f6707f16dbaa7289e53ca8e7a2b72161a'
WORLD='world.studio';INSTANCES='scenery/battlefield_instances.txt';ART={WORLD,INSTANCES}
SCENE=frozenset({'world.studio','opening/battlefield.settings','opening/road-traffic.settings','scenery/battlefield_instances.txt',
 'models/battlefield/barricade.glb','models/battlefield/barricade_mid.glb','models/battlefield/discarded_arms.glb','models/battlefield/discarded_arms_mid.glb','models/battlefield/casualty_blood.glb','models/battlefield/corpse_back.glb','models/battlefield/corpse_back_mid.glb','models/battlefield/corpse_side.glb','models/battlefield/corpse_side_mid.glb','models/battlefield/arrival_blood.glb',
 *('scenery/'+name+'_instances.txt' for name in ('authored','starting_area','rainforest','castle_village','tanthoon_bandit_camp','castle_streets','bep_environment','demo_finish','demo_polish'))})
class Refusal(RuntimeError):pass
def digest(data):return hashlib.sha256(data).hexdigest()
def plain(path):
 path=Path(path).absolute()
 for p in reversed((path,*path.parents)):
  try:mode=p.lstat().st_mode
  except FileNotFoundError as e:raise Refusal('Missing required path: '+str(p)) from e
  if stat.S_ISLNK(mode):raise Refusal('Symlink in guarded path: '+str(p))
 return path
class Reads:
 def __init__(self):self.hashes={}
 def read(self,path):
  path=plain(path);fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
  try:
   if not stat.S_ISREG(os.fstat(fd).st_mode):raise Refusal('Not a regular file: '+str(path))
   with os.fdopen(fd,'rb',closefd=False) as f:data=f.read()
  finally:os.close(fd)
  h=digest(data)
  if path in self.hashes and self.hashes[path]!=h:raise Refusal('Input changed while checking: '+str(path))
  self.hashes[path]=h;return data
 def document(self,path):
  try:value=json.loads(self.read(path))
  except (json.JSONDecodeError,UnicodeDecodeError) as e:raise Refusal('Invalid JSON: '+str(path)) from e
  if not isinstance(value,dict):raise Refusal('Expected JSON object: '+str(path))
  return value
 def recheck(self):
  for p,h in tuple(self.hashes.items()):
   if digest(self.read(p))!=h:raise Refusal('Guarded input changed before commit: '+str(p))
def require(value,message):
 if not value:raise Refusal(message)
def relative(value):
 require(isinstance(value,str),'Invalid relative path');parts=PurePosixPath(value).parts
 require(value in SCENE and not value.startswith('/') and all(p not in ('','..','.') for p in parts),'Unexpected scene path: '+value)
 return value
def publisher_scene(data):
 # Evaluate just the allowlist expression, never import/execute publisher code.
 tree=ast.parse(data.decode());nodes=[x for x in tree.body if isinstance(x,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SCENE' for t in x.targets)]
 require(len(nodes)==1,'Publisher SCENE declaration changed');expression=ast.Expression(nodes[0].value)
 allowed=(ast.Expression,ast.Call,ast.Name,ast.Load,ast.Set,ast.Starred,ast.GeneratorExp,ast.comprehension,ast.Store,ast.BinOp,ast.Add,ast.Constant,ast.Tuple)
 for node in ast.walk(expression):
  require(isinstance(node,allowed),'Unsupported publisher SCENE expression')
  if isinstance(node,ast.Call):require(isinstance(node.func,ast.Name) and node.func.id=='frozenset' and len(node.args)==1 and not node.keywords,'Unsupported publisher SCENE call')
 value=eval(compile(expression,'publisher-allowlist','eval'),{'__builtins__':{},'frozenset':frozenset})
 require(type(value)==frozenset and value==SCENE,'Publisher SCENE must remain exact23paths')
def file_rows(receipt):
 files=receipt.get('files');require(isinstance(files,list) and len(files)==2,'Art receipt must name exactly2files')
 out={}
 for row in files:
  require(isinstance(row,dict),'Invalid art receipt file');rel=row.get('relative_path');require(rel in ('world-project/'+WORLD,'world-project/'+INSTANCES) and rel not in out,'Unexpected or duplicate art receipt path');out[rel]=row
 return out
def v1_receipt(reads,path,proposal,expected_before,expected_after):
 d=reads.document(path)
 require(d.get('status')=='staged_unbuilt_native_pending','Unexpected art adoption status')
 require(d.get('source_proposal')==str(proposal),'Art proposal provenance mismatch')
 require(d.get('native_visual_approval') is False,'V1 adoption cannot grant native approval')
 require(d.get('published_files_changed') is False and d.get('N2_diagnostic_files_changed') is False,'Adoption must preserve published/N2 inputs')
 require(d.get('source_receipt_sha256')==V1_RECEIPT_SHA and digest(reads.read(proposal/'V1_RECEIPT.json'))==V1_RECEIPT_SHA,'V1 source receipt mismatch')
 rows=file_rows(d)
 for rel in ART:
  row=rows['world-project/'+rel];require(row.get('before')==expected_before[rel] and row.get('after')==expected_after[rel],'Art receipt hash chain mismatch: '+rel)
 return d
def v2_receipt(reads,path,proposal):
 d=reads.document(path)
 require(d.get('status')=='staged_native_pending','Unexpected V2 adoption status')
 require(d.get('source_proposal')==str(proposal),'V2 proposal provenance mismatch')
 require(d.get('manifest_sha256')==V2_MANIFEST_SHA and digest(reads.read(proposal/'V2_MANIFEST.json'))==V2_MANIFEST_SHA,'V2 source manifest mismatch')
 require(d.get('independent_review_sha256')==V2_INDEPENDENT_SHA and digest(reads.read(proposal/'INDEPENDENT_REVIEW.json'))==V2_INDEPENDENT_SHA,'V2 independent review mismatch')
 require(d.get('relative_path')=='world-project/'+WORLD and d.get('before_sha256')==V1_WORLD and d.get('after_sha256')==V2_WORLD and d.get('instances_sha256')==V1_INSTANCES,'V2 receipt hash chain mismatch')
 require(d.get('published_changed') is False and d.get('N2_diagnostic_changed') is False,'V2 adoption must preserve published/N2 inputs')
 require(d.get('native_visual_movement_performance_approval') is False,'V2 adoption receipt cannot grant native approval')
 return d
def prepare(candidate=N,setup_sha=SETUP_SHA,publisher_sha=PUBLISHER_SHA):
 n=Path(candidate).absolute();reads=Reads();setup_path=n/'review/SETUP_RECEIPT.json'
 require(digest(reads.read(setup_path))==setup_sha,'Original setup receipt changed');setup=reads.document(setup_path)
 require(setup.get('candidate')==str(n),'Setup candidate path mismatch');baseline=setup.get('scene_baseline_sha256');require(isinstance(baseline,dict) and set(baseline)==SCENE and len(SCENE)==23,'Setup must bind exact23scene preimages')
 publisher=reads.read(n/'tools/publish_candidate.py');require(digest(publisher)==publisher_sha,'Publisher implementation changed');publisher_scene(publisher)
 baseline_manifest=reads.read(n/'review/BASELINE_MANIFEST.json');require(digest(baseline_manifest)==setup.get('baseline_manifest_sha256'),'Original baseline manifest changed')
 manifest=json.loads(baseline_manifest);live_manifest=Path(manifest['runtime_generation']).parents[2]/'CURRENT_VALWOOD.json'
 require(reads.read(live_manifest)==baseline_manifest,'Live runtime manifest drift')
 plan_path=n/'review/PUBLISH_PLAN.json';plan=reads.document(plan_path);require(plan.get('candidate_project')==str(n/'world-project'),'Candidate project path changed')
 live=Path(manifest['working_world']);require(plan.get('active_project')==str(live),'Active project path changed')
 rows=plan.get('files');require(isinstance(rows,list) and len(rows)==23,'Plan must contain exact23rows');seen=set()
 for row in rows:
  require(isinstance(row,dict),'Invalid scene plan row');rel=relative(row.get('relative_path'));require(rel not in seen,'Duplicate scene plan row');seen.add(rel)
 require(seen==SCENE,'Plan allowlist drift')
 n2=n.with_name('battlefield-finish-2026-11-23');v1=n2/'review/battlefield-art-proposal';v2=n2/'review/battlefield-terrain-proposal-v2'
 original={r:baseline[r] for r in ART};v1_after={WORLD:V1_WORLD,INSTANCES:V1_INSTANCES};final={WORLD:V2_WORLD,INSTANCES:V1_INSTANCES}
 v1_path=n/'review/art-v1-adoption/RECEIPT.json';v2_path=n/'review/terrain-v2-adoption/RECEIPT.json'
 v1_receipt(reads,v1_path,v1,original,v1_after);v2_receipt(reads,v2_path,v2)
 result=copy.deepcopy(plan)
 for row in result['files']:
  rel=row['relative_path'];candidate_path=n/'world-project'/rel;live_path=live/rel
  live_bytes=reads.read(live_path);before={'exists':True,'sha256':baseline[rel],'bytes':len(live_bytes)}
  require(digest(live_bytes)==baseline[rel],'Live scene drift: '+rel)
  require(row.get('baseline')==before,'Original CAS preimage changed: '+rel)
  require(row.get('operation')=='replace_regular_file','Unexpected scene operation: '+rel)
  require(row.get('status')=='safe_replace_if_hash_matches','Unexpected scene CAS status: '+rel)
  old=row.get('candidate',{});require(old.get('path')==str(candidate_path) and old.get('resolved_path')==str(candidate_path) and old.get('is_symlink') is False and old.get('exists') is True,'Candidate path metadata changed: '+rel)
  allowed={baseline[rel]}
  if rel in ART:allowed.update((v1_after[rel],final[rel]))
  require(old.get('sha256') in allowed,'Unreviewed prior candidate hash: '+rel)
  data=reads.read(candidate_path);expected=final.get(rel,baseline[rel]);require(digest(data)==expected,'Unreceipted candidate change: '+rel)
  row['candidate']={'path':str(candidate_path),'exists':True,'is_symlink':False,'resolved_path':str(candidate_path),'bytes':len(data),'sha256':expected}
  row['live_at_audit']={'path':str(live_path),'exists':True,'is_symlink':False,'resolved_path':str(live_path),'bytes':len(live_bytes),'sha256':baseline[rel]}
  if rel==WORLD:row['scope']='ReceiptedV2:296height nodes on2existing tiles and7of8existing field projectors; other V1 records unchanged. Native visual/movement/performance review required.'
  elif rel==INSTANCES:row['scope']='ReceiptedV1:54additional owned prop/plant instances; unchanged inV2. Existing source order and original prefix preserved.'
 result['created_utc']=dt.datetime.now(dt.timezone.utc).isoformat();result['source_status']='N3 receiptedV1instances +V2terrain data only. Published23CAS preimages unchanged; native visual/performance acceptance remains separate.'
 require(isinstance(result.get('snapshot_result'),dict),'Invalid plan snapshot')
 result['snapshot_result'].update(replace_files=23,create_files=0,total_files=23,conflicts=[],conflict_count=0,safe_only_while_hashes_match=True,bytes_to_stage=sum(r['candidate']['bytes'] for r in result['files']))
 evidence=result.get('evidence');require(isinstance(evidence,dict),'Unexpected evidence structure')
 evidence['polish_art_refresh']={'setup_receipt_sha256':setup_sha,'baseline_manifest_sha256':digest(baseline_manifest),'v1_adoption_receipt_sha256':reads.hashes[v1_path],'v2_adoption_receipt_sha256':reads.hashes[v2_path],'v2_manifest_sha256':V2_MANIFEST_SHA,'native_acceptance':False}
 # Replace stale N1 transaction counts without changing its guarded algorithm.
 for step in result.get('transaction_steps',[]):
  if step.get('step')==4:step['action']='Back up23existing scene files and the active runtime manifest with exact bytes/modes; no new scene paths.'
  if step.get('step')==7:step['action']='Commit exactly23CAS scene replacements with world.studio last, then verified runtime pointer last; retain rollback for all23files and manifest.'
  if step.get('step')==10:step['action']='On failure restore23exact scene backups and runtime manifest; preserve all saves/settings/user edits.'
 require([r['baseline'] for r in result['files']]==[r['baseline'] for r in plan['files']],'CAS baseline altered internally')
 reads.output_path=plan_path;reads.recheck();return result,reads,plan_path

def apply(result,reads,path):
 """The sole write path: atomic candidate review plan, never a world file."""
 require(Path(path)==getattr(reads,'output_path',None),'Only the validated candidate plan may be replaced')
 path=plain(path);reads.recheck();directory=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
 name='.polish-plan-'+secrets.token_hex(12)+'.tmp';created=False
 try:
  mode=stat.S_IMODE(os.stat(path.name,dir_fd=directory,follow_symlinks=False).st_mode)
  fd=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=directory);created=True
  with os.fdopen(fd,'w') as out:
   json.dump(result,out,indent=2);out.write('\n');out.flush();os.fchmod(out.fileno(),mode);os.fsync(out.fileno())
  reads.recheck();held=os.fstat(directory);current=path.parent.stat()
  require((held.st_dev,held.st_ino)==(current.st_dev,current.st_ino),'Review directory changed before commit')
  os.replace(name,path.name,src_dir_fd=directory,dst_dir_fd=directory);created=False;os.fsync(directory)
 finally:
  if created:
   try:os.unlink(name,dir_fd=directory)
   except FileNotFoundError:pass
  os.close(directory)

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
 result,reads,path=prepare()
 if args.apply:apply(result,reads,path)
 print(json.dumps({'validated':True,'applied':args.apply,'scene_files':23,'changed_art_paths':sorted(ART),'baseline_preimages_preserved':True,'baseline_manifest_preserved':True,'native_acceptance':False,'live_mutations_performed':False,'candidate_plan_sha256':digest((json.dumps(result,indent=2)+'\n').encode())},indent=2))
if __name__=='__main__':
 try:main()
 except (Refusal,KeyError,ValueError) as e:raise SystemExit('REFUSED: '+str(e))
