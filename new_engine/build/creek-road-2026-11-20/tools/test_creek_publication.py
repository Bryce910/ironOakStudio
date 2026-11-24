"""Bounded CPU-only water/scene derivative guard and publication fault tests."""
from pathlib import Path
import importlib.util,json,os,subprocess,sys,tempfile,unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import publication_fixtures as old
c,p,f=old.c,old.p,old.f
GENERIC=('preflight_read_only','stale_current_pointer','changed_live_authoring','changed_candidate_scene','candidate_leaf_alias_rejected','out_of_scope_scene','model_new_only','dangling_new_target_occupied','aliased_model_parent_rejected','owned_state_exact_including_deletions','state_alias_rejected_before_deletion','atomic_unexpected_temp_untouched','atomic_unexpected_dangling_temp_untouched','copy_failure_cleans_only_owned_temp','replace_failure_cleans_owned_temp','publish_preserves_accepted_state_and_installs_new_alias','post_switch_failure_restores_scene_pointer_aliases_ready','rollback_attempts_all_operations_including_current')
class WaterScope(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='creek-scope-test-');self.root=Path(self.tmp.name);self.n=self.root/'candidate';self.b=self.root/'baseline';self.stack=[]
  for name,value in [('N',self.n),('B',self.b)]:
   item=patch.object(c,name,value);item.start();self.stack.append(item)
  for name in ('engine/world_viewer.cpp','engine/world_river_reflection_scissor.h','shaders/world_water.fs.glsl'):
   old.write(self.b/'source-v1'/name,'old '+name);old.write(self.n/'source-v1'/name,'new '+name)
  for i in range(204):
   name='world_water.fs.glsl' if i==0 else str(i)+'.glsl'
   for generation in (self.b,self.n):old.write(generation/'shared/shaders'/name,'same '+name)
  self.scope=dict(schema='valwood.source-change-scope.v1',reviewed=True,root_freeze_authorized=True,baseline=str(self.b),changes={})
  self.scope['changes']={k:dict(before=v,after=c.inventory(self.n/'source-v1')[k]) for k,v in c.inventory(self.b/'source-v1').items()}
  self.flush()
 def flush(self):old.write(self.n/'review/SOURCE_CHANGE_SCOPE.json',self.scope)
 def tearDown(self):
  for item in reversed(self.stack):item.stop()
  self.tmp.cleanup()
 def approve_shader(self):
  name='world_water.fs.glsl';old.write(self.n/'shared/shaders'/name,'new '+name)
  old.write(self.n/'source-v1/shaders'/name,'new '+name)
  doc=dict(schema='valwood.reviewed-shader-override.v1',reviewed=True,root_freeze_authorized=True,baseline=str(self.b),shader_count=204,candidate_sha256=c.inventory(self.n/'shared/shaders'),changes=[dict(path=name,before_sha256=c.sha(self.b/'shared/shaders'/name),after_sha256=c.sha(self.n/'shared/shaders'/name))])
  old.write(self.n/'review/SHADER_OVERRIDE.json',doc);return doc
 def test_exact_approved_source_map(self):self.assertEqual(c.source_changes(),self.scope['changes'])
 def test_unrelated_source_path_rejected(self):
  old.write(self.n/'source-v1/engine/other.cpp','new unrelated')
  with self.assertRaisesRegex(RuntimeError,'exact reviewed water'):c.source_changes()
 def test_source_tamper_rejected(self):
  old.write(self.n/'source-v1/engine/world_viewer.cpp','later edit')
  with self.assertRaisesRegex(RuntimeError,'exact reviewed water'):c.source_changes()
 def test_source_root_authorization_required(self):
  self.scope['root_freeze_authorized']=False;self.flush()
  with self.assertRaisesRegex(RuntimeError,'Missing reviewed'):c.source_changes()
 def test_unchanged_owned_shader_bank(self):self.assertIsNone(c.shader_override())
 def test_exact_water_shader_override(self):self.approve_shader();self.assertEqual(c.shader_delivery()['shader_changes'][0]['path'],'world_water.fs.glsl')
 def test_other_shader_change_rejected(self):
  self.approve_shader();old.write(self.n/'shared/shaders/1.glsl','unrelated')
  with self.assertRaisesRegex(RuntimeError,'exact authorized water'):c.shader_override()
 def test_shader_source_copy_mismatch_rejected(self):
  self.approve_shader();old.write(self.n/'source-v1/shaders/world_water.fs.glsl','different')
  with self.assertRaisesRegex(RuntimeError,'source/runtime'):c.shader_override()
 def test_shader_path_added_rejected(self):
  self.approve_shader();old.write(self.n/'shared/shaders/new.glsl','extra')
  with self.assertRaisesRegex(RuntimeError,'204 shader path'):c.shader_override()
 def test_owned_shader_cannot_hide_alias(self):
  q=self.n/'shared/shaders/1.glsl';q.unlink();q.symlink_to(self.b/'shared/shaders/1.glsl')
  with self.assertRaisesRegex(RuntimeError,'without aliases'):c.shader_override()
 def test_binary_must_be_owned_candidate_build(self):
  old.write(self.n/'runtime/BUILD_REVIEW.json',{});old.write(self.n/'runtime-native-v1/READY.json',{});old.write(self.b/'runtime/rpg','old executable');(self.n/'runtime/rpg').symlink_to(self.b/'runtime/rpg')
  with self.assertRaisesRegex(RuntimeError,'inside its owned build'):c.runtime_delivery()

class NativeMatrix(old.MatchedMatrix):
 def setUp(self):
  super().setUp();self.runtime=dict(runtime_reused=False,source_reused=False,binary_sha256='candidate fixture')
  self.patches=[patch.object(f,'runtime_delivery',return_value=self.runtime),patch.object(f,'native_role',side_effect=lambda path,prep:prep['stage'])]
  for item in self.patches:item.start()
  self.report.update(runtime_reused=False,CXX_source_reused=False,runtime_delivery=self.runtime);self.flush()
 def tearDown(self):
  for item in reversed(self.patches):item.stop()
  super().tearDown()

class HistoricalBaseline(unittest.TestCase):
 def test_exact_published_four_are_reused_without_stage_rewrite(self):
  captures=f.historical_baselines();self.assertEqual(len(captures),4)
  for path in captures:
   prep=c.read(path.with_name('PREPARATION.json'));self.assertEqual(prep['stage'],'candidate');self.assertEqual(f.native_role(path,prep),'baseline')
 def test_unapproved_predecessor_candidate_is_not_baseline(self):
  with self.assertRaisesRegex(RuntimeError,'neither current candidate'):
   f.native_role(c.B/'review/unapproved/EXECUTION.json',dict(stage='candidate',generation=str(c.B)))
 def test_current_candidate_role_retains_real_stage(self):
  self.assertEqual(f.native_role(ROOT/'review/new/EXECUTION.json',dict(stage='candidate',generation=str(ROOT))),'candidate')

class ShaderView(unittest.TestCase):
 def exercise(self,state):
  with tempfile.TemporaryDirectory(prefix='creek-shader-view-test-') as tmp:
   root=Path(tmp);oldbank=root/'old';newbank=root/'new';trace=root/'trace';trace.mkdir()
   old.write(oldbank/'water.glsl','old shader');old.write(newbank/'water.glsl','new shader')
   old.write(root/'unrelated','actual unrelated bytes')
   aliases={}
   for i in range(5):
    q=root/str(i)/'shaders';q.parent.mkdir();q.symlink_to(oldbank if state=='before' else newbank,target_is_directory=True);aliases[str(q)]=str(oldbank)
   script=root/'checker.py';script.write_text('''from pathlib import Path
import json,os
c=json.loads(os.environ['VALWOOD_HISTORICAL_SHADER_VIEW'])
for p,target in c['alias_targets'].items():
 q=Path(p);assert q.resolve()==Path(target);assert(q/'water.glsl').read_text()=='old shader'
 try:(q/'water.glsl').write_text('forbidden')
 except RuntimeError:pass
 else:raise AssertionError('write allowed')
assert Path(c['unrelated']).read_text()=='actual unrelated bytes'
''')
   config=dict(alias_targets=aliases,read_bank=str(oldbank),shader_paths=['water.glsl'],allowed_scripts=[str(script)],trace=str(trace),unrelated=str(root/'unrelated'))
   env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',PYTHONPATH=str(ROOT/'tools/baseline_view'),VALWOOD_HISTORICAL_SHADER_VIEW=json.dumps(config))
   run=subprocess.run([sys.executable,str(script)],env=env,capture_output=True,text=True)
   self.assertEqual(run.returncode,0,run.stderr);self.assertEqual((newbank/'water.glsl').read_text(),'new shader');self.assertEqual((oldbank/'water.glsl').read_text(),'old shader')
   self.assertEqual(len(list(trace.glob('*.json'))),1)
   for q in aliases:self.assertEqual(Path(q).resolve(),oldbank if state=='before' else newbank)
 def test_before_alias_view_is_read_only(self):self.exercise('before')
 def test_after_new_shader_alias_view_preserves_actual_new_bytes(self):self.exercise('after')

def suite():
 result=unittest.TestSuite(old.Publication('test_'+name) for name in GENERIC)
 for cls in (WaterScope,NativeMatrix,HistoricalBaseline,ShaderView):result.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(cls))
 return result
if __name__=='__main__':
 result=unittest.TextTestRunner(verbosity=2).run(suite());raise SystemExit(0 if result.wasSuccessful() else 1)
