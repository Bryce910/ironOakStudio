#!/usr/bin/env python3
"""Isolated temporary fixtures only. Never runs actual candidate --apply."""
import copy,hashlib,importlib.util,json,os,stat,tempfile,unittest
from pathlib import Path
from unittest import mock
spec=importlib.util.spec_from_file_location('polish_refresh',Path(__file__).with_name('refresh_polish_publish_plan.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sha=lambda b:hashlib.sha256(b).hexdigest()
class Fixture:
 def __init__(self,root):
  self.root=root/'engine-root';build=self.root/'new_engine/build';self.n=build/'battlefield-polish-2026-11-23';self.n2=build/'battlefield-finish-2026-11-23';n1=build/'battlefield-opening-2026-11-22';self.live=self.root/'new_engine/projects/valwood_current';self.review=self.n/'review';self.project=self.n/'world-project';self.plan=self.review/'PUBLISH_PLAN.json'
  self.review.mkdir(parents=True);(self.n/'tools').mkdir();self.live.mkdir(parents=True)
  self.proposal1=self.n2/'review/battlefield-art-proposal';self.proposal2=self.n2/'review/battlefield-terrain-proposal-v2';self.proposal1.mkdir(parents=True);self.proposal2.mkdir(parents=True)
  def write(path,data):path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
  self.write=write
  self.v1world=b'reviewed world V1';self.v2world=b'reviewed world V2';self.instances=b'reviewed instances V1'
  m.V1_WORLD=sha(self.v1world);m.V2_WORLD=sha(self.v2world);m.V1_INSTANCES=sha(self.instances)
  for name,path in [('V1_RECEIPT_SHA',self.proposal1/'V1_RECEIPT.json'),('V2_MANIFEST_SHA',self.proposal2/'V2_MANIFEST.json'),('V2_INDEPENDENT_SHA',self.proposal2/'INDEPENDENT_REVIEW.json')]:
   data=json.dumps({'source':name}).encode();write(path,data);setattr(m,name,sha(data))
  baselines={};rows=[]
  for rel in sorted(m.SCENE):
   data=('original:'+rel).encode();write(self.live/rel,data);write(self.project/rel,data);baselines[rel]=sha(data);p=self.project/rel
   rows.append(dict(operation='replace_regular_file',relative_path=rel,baseline=dict(exists=True,bytes=len(data),sha256=sha(data)),status='safe_replace_if_hash_matches',candidate=dict(path=str(p),resolved_path=str(p),is_symlink=False,exists=True,bytes=len(data),sha256=sha(data)),live_at_audit={}))
  write(self.project/m.WORLD,self.v2world);write(self.project/m.INSTANCES,self.instances)
  self.original=baselines
  manifest=dict(runtime_generation=str(n1),working_world=str(self.live));mb=(json.dumps(manifest)+'\n').encode();write(self.root/'CURRENT_VALWOOD.json',mb);write(self.review/'BASELINE_MANIFEST.json',mb)
  setup=dict(candidate=str(self.n),scene_baseline_sha256=baselines,baseline_manifest_sha256=sha(mb));self.setup_bytes=(json.dumps(setup)+'\n').encode();write(self.review/'SETUP_RECEIPT.json',self.setup_bytes);self.setup_sha=sha(self.setup_bytes)
  self.publisher=self.n/'tools/publish_candidate.py';write(self.publisher,('SCENE=frozenset('+repr(set(m.SCENE))+')\n').encode());self.publisher_sha=sha(self.publisher.read_bytes())
  plan=dict(candidate_project=str(self.project),active_project=str(self.live),files=rows,snapshot_result={},evidence={},transaction_steps=[dict(step=i,action='original') for i in range(1,11)])
  write(self.plan,(json.dumps(plan)+'\n').encode());self.initial_plan=self.plan.read_bytes()
  d=dict(status='staged_unbuilt_native_pending',source_proposal=str(self.proposal1),source_receipt_sha256=m.V1_RECEIPT_SHA,published_files_changed=False,N2_diagnostic_files_changed=False,native_visual_approval=False,files=[dict(relative_path='world-project/'+r,before=baselines[r],after=m.V1_WORLD if r==m.WORLD else m.V1_INSTANCES) for r in sorted(m.ART)])
  self.v1=self.review/'art-v1-adoption/RECEIPT.json';self.doc(self.v1,d)
  d=dict(status='staged_native_pending',source_proposal=str(self.proposal2),manifest_sha256=m.V2_MANIFEST_SHA,independent_review_sha256=m.V2_INDEPENDENT_SHA,relative_path='world-project/'+m.WORLD,before_sha256=m.V1_WORLD,after_sha256=m.V2_WORLD,instances_sha256=m.V1_INSTANCES,published_changed=False,N2_diagnostic_changed=False,native_visual_movement_performance_approval=False)
  self.v2=self.review/'terrain-v2-adoption/RECEIPT.json';self.doc(self.v2,d)
 def doc(self,path,data):self.write(path,(json.dumps(data)+'\n').encode())
 def mutate(self,path,change):d=json.loads(path.read_text());change(d);self.doc(path,d)
 def prepare(self):return m.prepare(self.n,self.setup_sha,self.publisher_sha)
class RefreshTests(unittest.TestCase):
 def setUp(self):
  self.constants={k:getattr(m,k) for k in ('V1_WORLD','V2_WORLD','V1_INSTANCES','V1_RECEIPT_SHA','V2_MANIFEST_SHA','V2_INDEPENDENT_SHA')};self.temp=tempfile.TemporaryDirectory();self.f=Fixture(Path(self.temp.name))
 def tearDown(self):
  for k,v in self.constants.items():setattr(m,k,v)
  self.temp.cleanup()
 def reject(self):
  with self.assertRaises(m.Refusal):self.f.prepare()
  self.assertEqual(self.f.plan.read_bytes(),self.f.initial_plan)
 def test_readonly_default_retains_all23preimages(self):
  before_manifest=(self.f.review/'BASELINE_MANIFEST.json').read_bytes();out,reads,path=self.f.prepare();self.assertEqual(len(out['files']),23)
  original=json.loads(self.f.initial_plan);self.assertEqual([r['baseline'] for r in out['files']],[r['baseline'] for r in original['files']]);self.assertEqual(self.f.plan.read_bytes(),self.f.initial_plan)
  self.assertEqual((self.f.review/'BASELINE_MANIFEST.json').read_bytes(),before_manifest);self.assertFalse(out['evidence']['polish_art_refresh']['native_acceptance'])
  changed=[r['relative_path'] for r in out['files'] if r['candidate']['sha256']!=r['baseline']['sha256']];self.assertEqual(set(changed),m.ART)
 def test_atomic_fixture_only_apply_and_idempotent_baseline(self):
  os.chmod(self.f.plan,0o640);out,reads,path=self.f.prepare();m.apply(out,reads,path);self.assertEqual(stat.S_IMODE(path.stat().st_mode),0o640);second,_,_=self.f.prepare();self.assertEqual([r['baseline'] for r in out['files']],[r['baseline'] for r in second['files']]);self.assertFalse(list(path.parent.glob('.polish-plan-*.tmp')))
 def test_setup_mutation_refused(self):self.f.write(self.f.review/'SETUP_RECEIPT.json',b'{}');self.reject()
 def test_baseline_manifest_mutation_refused(self):self.f.write(self.f.review/'BASELINE_MANIFEST.json',b'{}');self.reject()
 def test_live_manifest_drift_refused(self):self.f.write(self.f.root/'CURRENT_VALWOOD.json',b'{}');self.reject()
 def test_live_scene_drift_refused(self):self.f.write(self.f.live/'opening/road-traffic.settings',b'user edit');self.reject()
 def test_other21_candidate_mutation_refused(self):self.f.write(self.f.project/'models/battlefield/corpse_back.glb',b'new');self.reject()
 def test_world_v1_not_finalv2_refused(self):self.f.write(self.f.project/m.WORLD,self.f.v1world);self.reject()
 def test_unreceipted_instances_refused(self):self.f.write(self.f.project/m.INSTANCES,b'new placements');self.reject()
 def test_v2_receipt_missing_refused(self):self.f.v2.unlink();self.reject()
 def test_v1_hash_chain_refused(self):self.f.mutate(self.f.v1,lambda d:d['files'][0].update(before='0'*64));self.reject()
 def test_v2_hash_chain_refused(self):self.f.mutate(self.f.v2,lambda d:d.update(before_sha256='0'*64));self.reject()
 def test_v2_source_manifest_refused(self):self.f.write(self.f.proposal2/'V2_MANIFEST.json',b'{}');self.reject()
 def test_v2_independent_receipt_refused(self):self.f.mutate(self.f.v2,lambda d:d.update(independent_review_sha256='0'*64));self.reject()
 def test_source_provenance_mismatch_refused(self):self.f.mutate(self.f.v2,lambda d:d.update(source_proposal=str(self.f.proposal1)));self.reject()
 def test_extra_receipt_path_refused(self):self.f.mutate(self.f.v1,lambda d:d['files'].append(dict(relative_path='world-project/extra')));self.reject()
 def test_extra_plan_path_refused(self):self.f.mutate(self.f.plan,lambda d:d['files'].append(copy.deepcopy(d['files'][0])));self.f.initial_plan=self.f.plan.read_bytes();self.reject()
 def test_duplicate_plan_path_refused(self):self.f.mutate(self.f.plan,lambda d:d['files'].__setitem__(1,copy.deepcopy(d['files'][0])));self.f.initial_plan=self.f.plan.read_bytes();self.reject()
 def test_original_cas_hash_cannot_rebase(self):self.f.mutate(self.f.plan,lambda d:d['files'][0]['baseline'].update(sha256='0'*64));self.f.initial_plan=self.f.plan.read_bytes();self.reject()
 def test_extra_publisher_scene_refused(self):self.f.write(self.f.publisher,('SCENE=frozenset('+repr(set(m.SCENE)|{'extra.glb'})+')\n').encode());self.reject()
 def test_publisher_body_hash_guard(self):self.f.write(self.f.publisher,self.f.publisher.read_bytes()+b'# implementation changed\n');self.reject()
 def test_publisher_never_executed(self):self.f.write(self.f.publisher,self.f.publisher.read_bytes()+b'raise RuntimeError("must not execute publisher")\n');self.f.publisher_sha=sha(self.f.publisher.read_bytes());self.f.prepare()
 def test_candidate_symlink_refused(self):p=self.f.project/m.WORLD;data=p.read_bytes();p.unlink();target=self.f.root/'other';target.write_bytes(data);p.symlink_to(target);self.reject()
 def test_live_symlink_refused(self):p=self.f.live/m.WORLD;data=p.read_bytes();p.unlink();target=self.f.root/'other';target.write_bytes(data);p.symlink_to(target);self.reject()
 def test_receipt_symlink_refused(self):data=self.f.v2.read_bytes();self.f.v2.unlink();target=self.f.root/'receipt';target.write_bytes(data);self.f.v2.symlink_to(target);self.reject()
 def test_changed_before_commit_refused(self):
  out,reads,path=self.f.prepare();self.f.write(self.f.live/m.INSTANCES,b'new user edit')
  with self.assertRaises(m.Refusal):m.apply(out,reads,path)
  self.assertEqual(path.read_bytes(),self.f.initial_plan)
 def test_only_validated_review_plan_can_be_written(self):
  out,reads,_=self.f.prepare()
  with self.assertRaises(m.Refusal):m.apply(out,reads,self.f.project/m.WORLD)
  self.assertEqual((self.f.project/m.WORLD).read_bytes(),self.f.v2world)
 def test_missing_plan_row_refused(self):self.f.mutate(self.f.plan,lambda d:d['files'].pop());self.f.initial_plan=self.f.plan.read_bytes();self.reject()
 def test_parent_symlink_refused(self):
  p=self.f.project/'models/battlefield';target=p.with_name('retained-assets');p.rename(target);p.symlink_to(target,target_is_directory=True);self.reject()
 def test_exact_scene_expression_even_when_fixture_hash_bound(self):
  self.f.write(self.f.publisher,('SCENE=frozenset('+repr(set(m.SCENE)|{'extra.glb'})+')\n').encode());self.f.publisher_sha=sha(self.f.publisher.read_bytes());self.reject()
 def test_receipt_cannot_assert_live_mutation(self):self.f.mutate(self.f.v2,lambda d:d.update(published_changed=True));self.reject()
 def test_atomic_replace_failure_preserves_original(self):
  out,reads,path=self.f.prepare()
  with mock.patch.object(m.os,'replace',side_effect=OSError('fixture commit failure')):
   with self.assertRaises(OSError):m.apply(out,reads,path)
  self.assertEqual(path.read_bytes(),self.f.initial_plan);self.assertFalse(list(path.parent.glob('.polish-plan-*.tmp')))
if __name__=='__main__':unittest.main()
