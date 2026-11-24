"""Small CPU-only successor gate using existing state/rollback fixtures."""
from pathlib import Path
import importlib.util,json,shutil,sys,tempfile,unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
spec=importlib.util.spec_from_file_location('inherited_publication_fixtures',ROOT/'tools/test_scene_publication.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old);c=old.c
GENERIC=('preflight_read_only','stale_current_pointer','changed_live_authoring','changed_candidate_scene','candidate_leaf_alias_rejected','out_of_scope_scene','model_new_only','dangling_new_target_occupied','aliased_model_parent_rejected','owned_state_exact_including_deletions','state_alias_rejected_before_deletion','atomic_unexpected_temp_untouched','atomic_unexpected_dangling_temp_untouched','copy_failure_cleans_only_owned_temp','replace_failure_cleans_owned_temp','publish_preserves_accepted_state_and_installs_new_alias','post_switch_failure_restores_scene_pointer_aliases_ready','rollback_attempts_all_operations_including_current')
class NativeScope(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='valwood-native-scope-test-');self.root=Path(self.tmp.name);self.n=self.root/'candidate';self.b=self.root/'baseline';self.stack=[]
  for owner,key,value in ((c,'N',self.n),(c,'B',self.b)):
   p=patch.object(owner,key,value);p.start();self.stack.append(p)
  self.delta={}
  for name in ('engine/world_forest_shelter.h','engine/world_viewer.cpp'):
   old.write(self.b/'source-v1'/name,'old '+name);old.write(self.n/'source-v1'/name,'new '+name)
   self.delta[name]={'before':c.sha(self.b/'source-v1'/name),'after':c.sha(self.n/'source-v1'/name)}
  old.write(self.n/'review/SOURCE_CHANGE_SCOPE.json',dict(schema='valwood.source-change-scope.v1',reviewed=True,baseline=str(self.b),changes=self.delta))
  for i in range(204):
   for generation in (self.b,self.n):old.write(generation/'shared/shaders'/f'{i}.glsl','same shader '+str(i))
 def tearDown(self):
  for p in reversed(self.stack):p.stop()
  self.tmp.cleanup()
 def test_exact_two_source_changes(self):self.assertEqual(c.source_changes(),self.delta)
 def test_unrelated_source_change_rejected(self):
  old.write(self.n/'source-v1/engine/other.cpp','unexpected')
  with self.assertRaisesRegex(RuntimeError,'exact two-file reviewed correction'):c.source_changes()
 def test_source_postreview_tamper_rejected(self):
  old.write(self.n/'source-v1/engine/world_viewer.cpp','later edit')
  with self.assertRaisesRegex(RuntimeError,'exact two-file reviewed correction'):c.source_changes()
 def test_no_shader_change_allowed(self):
  self.assertIsNone(c.shader_override());old.write(self.n/'shared/shaders/0.glsl','changed')
  with self.assertRaisesRegex(RuntimeError,'204 shader bytes'):c.shader_override()
 def test_owned_bank_cannot_hide_file_alias(self):
  p=self.n/'shared/shaders/0.glsl';p.unlink();p.symlink_to(self.b/'shared/shaders/0.glsl')
  with self.assertRaisesRegex(RuntimeError,'contains an alias'):c.shader_override()
 def test_binary_must_be_inside_owned_candidate_build(self):
  old.write(self.n/'runtime/BUILD_REVIEW.json',{});old.write(self.n/'runtime-native-v1/READY.json',{});old.write(self.b/'runtime/rpg','old executable')
  (self.n/'runtime/rpg').symlink_to(self.b/'runtime/rpg')
  with self.assertRaisesRegex(RuntimeError,'inside its owned build'):c.runtime_delivery()

class NativeMatrix(old.MatchedMatrix):
 def setUp(self):
  super().setUp();self.runtime=dict(runtime_reused=False,source_reused=False,binary_sha256='candidate fixture')
  self.native_patch=patch.object(old.f,'runtime_delivery',return_value=self.runtime);self.native_patch.start()
  self.report.update(runtime_reused=False,CXX_source_reused=False,runtime_delivery=self.runtime);self.flush()
 def tearDown(self):self.native_patch.stop();super().tearDown()

def suite():
 s=unittest.TestSuite(old.Publication('test_'+name) for name in GENERIC);s.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(NativeScope));s.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(NativeMatrix));return s
if __name__=='__main__':
 result=unittest.TextTestRunner(verbosity=2).run(suite());raise SystemExit(0 if result.wasSuccessful() else 1)
