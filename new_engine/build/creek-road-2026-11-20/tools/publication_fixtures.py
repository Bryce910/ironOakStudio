"""CPU-only /tmp publication fixtures inherited from the published predecessor."""
from pathlib import Path
import importlib.util,json,os,shutil,sys,tempfile,types,unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import check_unified as c

def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
p=module('creek_promote_fixture',ROOT/'tools/promote.py')
f=module('creek_freeze_fixture',ROOT/'tools/freeze_review.py')
def write(path,value):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value)+'\n' if isinstance(value,(dict,list)) else value)

class Publication(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-scene-publication-test-')
        self.root=Path(self.tmp.name);self.e=self.root/'engine';self.n=self.e/'build/candidate';self.b=self.e/'build/baseline';self.l=self.e/'projects/live';self.area=self.e/'projects/area'
        for project in (self.n,self.b,self.l,self.area): project.mkdir(parents=True)
        self.stack=[]
        for name,value in [('N',self.n),('B',self.b),('E',self.e),('R',self.root),('L',self.l)]:
            z=patch.object(p,name,value);z.start();self.stack.append(z)
        for name,value in [('verify_reuse',lambda:('binary',{})),('evidence',lambda manifest:'binary')]:
            z=patch.object(p,name,value);z.start();self.stack.append(z)
        write(self.b/'shared/shaders/test.glsl','shader')
        for project in (self.l,self.area): (project/'shaders').symlink_to(self.b/'shared/shaders',target_is_directory=True)
        self.current=dict(runtime_generation=str(self.b),area_projects={'area':str(self.area)},working_world=str(self.l))
        write(self.root/'CURRENT_VALWOOD.json',self.current)
        write(self.n/'BASELINE.json',dict(current_pointer_sha256=c.sha(self.root/'CURRENT_VALWOOD.json'),live_shader_bindings={str(project/'shaders'):str(self.b/'shared/shaders') for project in (self.l,self.area)}))
        write(self.l/'world.studio','old scene');write(self.n/'world-project/world.studio','new scene')
        self.row=dict(before=dict(kind='file',sha256=c.sha(self.l/'world.studio')),after=dict(kind='file',sha256=c.sha(self.n/'world-project/world.studio')))
        self.manifest=dict(scene_delta={'world.studio':self.row},model_additions=[],pins={},aliases={})
        write(self.n/'review/CHANGESET.json',self.manifest)
        write(self.n/'review/FINAL_REVIEW.json',{'passed':True})
        write(self.n/'check_unified.py','fixture checker')
        write(self.n/'launch.py','fixture launcher')
        write(self.n/'review/LIVE_BEFORE.json',{'world.studio':c.sha(self.l/'world.studio')})
        for project in (self.l,self.area,self.b/'arena-project',self.b/'joust-project',self.b/'shared'):
            project.mkdir(parents=True,exist_ok=True);write(project/'graphics.settings','accepted settings')
        for mode in ('arena','joust'):
            write(self.b/(mode+'-project')/'saves/player.progress','accepted '+mode)
            write(self.b/(mode+'-project')/'settings/controls.config','controls '+mode)
            write(self.b/(mode+'-project')/'arena_roster.cfg','roster '+mode)
            write(self.n/(mode+'-project')/'saves/qa.progress','QA only')
            write(self.n/(mode+'-project')/'graphics.settings','stale settings')
        (self.n/'shared').mkdir()
        self.original_pointer=(self.root/'CURRENT_VALWOOD.json').read_bytes()
        self.before=p.state_files([self.l,self.area,self.b/'arena-project',self.b/'joust-project',self.b/'shared'])

    def tearDown(self):
        for z in reversed(self.stack): z.stop()
        self.tmp.cleanup()

    def flush(self): write(self.n/'review/CHANGESET.json',self.manifest)

    def fake_check(self):
        self.assertEqual(json.loads((self.root/'CURRENT_VALWOOD.json').read_text())['runtime_generation'],str(self.n))
        self.assertEqual((self.l/'world.studio').read_text(),'new scene')

    def test_preflight_read_only(self):
        values=p.preflight();self.assertEqual(len(values[4]),1)
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)
        self.assertEqual((self.l/'world.studio').read_text(),'old scene')

    def test_stale_current_pointer(self):
        write(self.root/'CURRENT_VALWOOD.json',dict(self.current,other='changed'))
        with self.assertRaisesRegex(RuntimeError,'Current generation'): p.preflight()

    def test_changed_live_authoring(self):
        write(self.l/'world.studio','artist new scene')
        with self.assertRaisesRegex(RuntimeError,'authoring changed'): p.preflight()

    def test_changed_candidate_scene(self):
        write(self.n/'world-project/world.studio','post-review change')
        with self.assertRaisesRegex(RuntimeError,'Scene conflict'): p.preflight()

    def test_candidate_leaf_alias_rejected(self):
        target=self.n/'world-project/world.studio';target.unlink();target.symlink_to(self.l/'world.studio')
        with self.assertRaisesRegex(RuntimeError,'must be owned'): p.preflight()

    def test_out_of_scope_scene(self):
        self.manifest['scene_delta']['city.sky']=self.row;self.flush()
        with self.assertRaisesRegex(RuntimeError,'Unreviewed scene'): p.preflight()

    def add_model(self):
        relative='models/new-art/tree.glb';source=self.n/'world-project'/relative;write(source,'new GLB')
        self.manifest['model_additions']=[dict(path=relative,sha256=c.sha(source))];self.flush()
        return self.l/relative,source

    def test_model_new_only(self):
        target,source=self.add_model();values=p.preflight();self.assertEqual(values[5],[(target,source)])
        write(target,'artist model')
        with self.assertRaisesRegex(RuntimeError,'already exists'): p.preflight()
        self.assertEqual(target.read_text(),'artist model')

    def test_dangling_new_target_occupied(self):
        target,_=self.add_model();target.parent.mkdir(parents=True);target.symlink_to(self.root/'missing')
        with self.assertRaisesRegex(RuntimeError,'already exists'): p.preflight()
        self.assertTrue(target.is_symlink())

    def test_aliased_model_parent_rejected(self):
        target,_=self.add_model();other=self.root/'other';other.mkdir();(self.l/'models').symlink_to(other)
        with self.assertRaisesRegex(RuntimeError,'parent unexpectedly aliased'): p.preflight()

    def test_owned_state_exact_including_deletions(self):
        p.copy_state_exact(self.b/'arena-project',self.n/'arena-project')
        self.assertEqual(p.relative_state(self.b/'arena-project'),p.relative_state(self.n/'arena-project'))
        self.assertFalse((self.n/'arena-project/saves/qa.progress').exists())

    def test_state_alias_rejected_before_deletion(self):
        source=self.b/'arena-project';(source/'saves/player.progress').unlink();(source/'saves/player.progress').symlink_to(self.root/'missing')
        with self.assertRaises(RuntimeError): p.copy_state_exact(source,self.n/'arena-project')
        self.assertTrue((self.n/'arena-project/saves/qa.progress').exists())

    def test_atomic_unexpected_temp_untouched(self):
        target=self.root/'target';write(target,'original');temporary=p._temporary(target);write(temporary,'someone else')
        for operation in (lambda:p.atomic_copy(self.l/'world.studio',target),lambda:p.atomic_link(target,self.l/'world.studio'),lambda:p.write(target,{})):
            with self.assertRaises(RuntimeError): operation()
            self.assertEqual(temporary.read_text(),'someone else');self.assertEqual(target.read_text(),'original')

    def test_atomic_unexpected_dangling_temp_untouched(self):
        target=self.root/'target';temporary=p._temporary(target);temporary.symlink_to(self.root/'missing')
        with self.assertRaises(RuntimeError): p.write(target,{})
        self.assertTrue(temporary.is_symlink())

    def test_copy_failure_cleans_only_owned_temp(self):
        target=self.root/'target';write(target,'original')
        with patch.object(p.shutil,'copy2',side_effect=OSError('copy failed')):
            with self.assertRaises(OSError): p.atomic_copy(self.l/'world.studio',target)
        self.assertFalse(p._temporary(target).exists());self.assertEqual(target.read_text(),'original')

    def test_replace_failure_cleans_owned_temp(self):
        target=self.root/'target';write(target,'original')
        with patch.object(p.os,'replace',side_effect=OSError('replace failed')):
            for operation in (lambda:p.atomic_copy(self.l/'world.studio',target),lambda:p.atomic_link(target,self.l/'world.studio'),lambda:p.write(target,{})):
                with self.assertRaises(OSError): operation()
                self.assertFalse(p._temporary(target).exists());self.assertFalse(p._temporary(target).is_symlink())
                self.assertEqual(target.read_text(),'original')

    def test_publish_preserves_accepted_state_and_installs_new_alias(self):
        target,source=self.add_model();values=p.preflight()
        with patch.dict(sys.modules,{'check_unified':types.SimpleNamespace(check=self.fake_check)}): p.publish(*values)
        self.assertTrue(target.is_symlink());self.assertEqual(target.resolve(),source)
        self.assertEqual(self.before,p.state_files([self.l,self.area,self.b/'arena-project',self.b/'joust-project',self.b/'shared']))
        for mode in ('arena','joust'):
            self.assertEqual(p.relative_state(self.b/(mode+'-project'),graphics=False),p.relative_state(self.n/(mode+'-project'),graphics=False))
            self.assertFalse((self.n/(mode+'-project')/'saves/qa.progress').exists())
        self.assertTrue((self.n/'PUBLICATION.json').exists())

    def test_post_switch_failure_restores_scene_pointer_aliases_ready(self):
        target,_=self.add_model();values=p.preflight()
        with patch.dict(sys.modules,{'check_unified':types.SimpleNamespace(check=lambda:(_ for _ in ()).throw(RuntimeError('post-switch failure')))}):
            with self.assertRaisesRegex(RuntimeError,'post-switch failure'): p.publish(*values)
        self.assertEqual((self.l/'world.studio').read_text(),'old scene')
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)
        self.assertFalse(target.exists());self.assertFalse(target.is_symlink())
        self.assertFalse((self.l/'models').exists());self.assertFalse((self.n/'UNIFIED_READY.json').exists())
        self.assertEqual(self.before,p.state_files([self.l,self.area,self.b/'arena-project',self.b/'joust-project',self.b/'shared']))

    def test_catalog_and_new_VWC_roll_back_together(self):
        model,_=self.add_model()
        cache_relative='collision/structures/new_trunk.vwc';cache_source=self.n/'world-project'/cache_relative
        write(cache_source,'new collision cache')
        self.manifest['model_additions'].append(dict(path=cache_relative,sha256=c.sha(cache_source)))
        catalog_relative='collision/structures/catalog'
        write(self.l/catalog_relative,'accepted catalog\n');write(self.n/'world-project'/catalog_relative,'accepted catalog\nnew binding\n')
        self.manifest['scene_delta'][catalog_relative]=dict(before=dict(kind='file',sha256=c.sha(self.l/catalog_relative)),
            after=dict(kind='file',sha256=c.sha(self.n/'world-project'/catalog_relative)))
        write(self.n/'review/LIVE_BEFORE.json',{'world.studio':c.sha(self.l/'world.studio'),catalog_relative:c.sha(self.l/catalog_relative)})
        self.flush();values=p.preflight()
        with patch.dict(sys.modules,{'check_unified':types.SimpleNamespace(check=lambda:(_ for _ in ()).throw(RuntimeError('check fails')))}):
            with self.assertRaisesRegex(RuntimeError,'check fails'):p.publish(*values)
        self.assertEqual((self.l/catalog_relative).read_text(),'accepted catalog\n')
        self.assertFalse((self.l/cache_relative).exists());self.assertFalse((self.l/cache_relative).is_symlink())
        self.assertFalse(model.exists());self.assertFalse(model.is_symlink())
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)

    def test_new_resource_rollback_preserves_existing_owned_parent(self):
        source=self.n/'world-project/surfaces/camp/new.png';write(source,'new PNG')
        write(self.l/'surfaces/original.png','original PNG')
        self.manifest['model_additions']=[dict(path='surfaces/camp/new.png',sha256=c.sha(source))];self.flush()
        values=p.preflight()
        with patch.object(c,'check',side_effect=RuntimeError('resource rollback fixture')):
            with self.assertRaisesRegex(RuntimeError,'resource rollback fixture'):p.publish(*values)
        self.assertEqual((self.l/'surfaces/original.png').read_text(),'original PNG')
        self.assertFalse((self.l/'surfaces/camp').exists())
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)

    def add_resource_overlay(self):
        source=self.n/'world-project/surfaces/camp/new.png';write(source,'new PNG')
        original=self.l/'surfaces/original.png';write(original,'original PNG')
        (self.b/'world-project').mkdir(parents=True,exist_ok=True)
        (self.b/'world-project/surfaces').symlink_to(self.l/'surfaces',target_is_directory=True)
        self.manifest['model_additions']=[dict(path='surfaces/camp/new.png',sha256=c.sha(source))]
        self.manifest['resource_overlays']={'surfaces':dict(original_alias=str((self.l/'surfaces').resolve()),
            original_leaves={'surfaces/original.png':dict(kind='file',sha256=c.sha(original))},additions={'surfaces/camp/new.png':c.sha(source)})}
        self.flush();return original

    def test_live_overlay_original_hash_conflict_rejected(self):
        original=self.add_resource_overlay();write(original,'artist changed PNG')
        with self.assertRaisesRegex(RuntimeError,'LIVE original resource changed'):p.preflight()
        self.assertEqual(original.read_text(),'artist changed PNG')

    def test_live_overlay_parent_alias_rejected(self):
        self.add_resource_overlay();other=self.root/'other-surfaces';(self.l/'surfaces').rename(other)
        (self.l/'surfaces').symlink_to(other,target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError,'parent must remain owned'):p.preflight()

    def test_overlay_publication_preserves_originals_and_new_only(self):
        original=self.add_resource_overlay();values=p.preflight()
        with patch.object(c,'check',side_effect=self.fake_check):p.publish(*values)
        self.assertFalse(original.is_symlink());self.assertEqual(original.read_text(),'original PNG')
        self.assertTrue((self.l/'surfaces/camp/new.png').is_symlink())
        self.assertEqual(c.read(self.n/'UNIFIED_READY.json')['resource_overlays'],self.manifest['resource_overlays'])

    def add_shader_override(self):
        write(self.n/'shared/shaders/test.glsl','reviewed replacement shader')
        self.manifest.update(shader_bank_reused=False,shader_changes=[{'path':'world_surface_library.fs.glsl'}],shader_override={'reviewed':True})
        self.flush()

    def test_shader_publication_retargets_only_live_modes(self):
        self.add_shader_override();values=p.preflight();self.assertEqual(len(values[6]),2)
        with patch.object(c,'check',side_effect=self.fake_check):p.publish(*values)
        for project in (self.l,self.area):self.assertEqual((project/'shaders').resolve(),self.n/'shared/shaders')
        self.assertEqual((self.b/'shared/shaders/test.glsl').read_text(),'shader')
        ready=c.read(self.n/'UNIFIED_READY.json');self.assertFalse(ready['shader_bank_reused']);self.assertEqual(len(ready['live_shader_bindings']),2)

    def test_shader_post_switch_failure_restores_original_aliases(self):
        self.add_shader_override();values=p.preflight();before={project:os.readlink(project/'shaders') for project in (self.l,self.area)}
        with patch.object(c,'check',side_effect=RuntimeError('shader after-switch failure')):
            with self.assertRaisesRegex(RuntimeError,'shader after-switch failure'):p.publish(*values)
        for project,target in before.items():self.assertEqual(os.readlink(project/'shaders'),target)
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)

    def test_partial_shader_install_failure_restores_first_alias(self):
        self.add_shader_override();values=p.preflight();real=p.atomic_link
        def fail_second(path,target):
            if Path(path)==self.area/'shaders':raise OSError('second shader install failed')
            return real(path,target)
        with patch.object(p,'atomic_link',side_effect=fail_second):
            with self.assertRaisesRegex(OSError,'second shader install failed'):p.publish(*values)
        for project in (self.l,self.area):self.assertEqual((project/'shaders').resolve(),self.b/'shared/shaders')
        self.assertEqual((self.root/'CURRENT_VALWOOD.json').read_bytes(),self.original_pointer)

    def test_shader_preflight_rejects_third_bank(self):
        self.add_shader_override();other=self.root/'unreviewed-shaders';other.mkdir();(self.l/'shaders').unlink();(self.l/'shaders').symlink_to(other)
        with self.assertRaisesRegex(RuntimeError,'Existing shared shader binding changed'):p.preflight()

    def test_rollback_attempts_all_operations_including_current(self):
        rollback=self.n/'rollback';write(rollback/'CURRENT_VALWOOD.json',self.current)
        write(rollback/'world-project/world.studio','old scene');write(self.n/'UNIFIED_READY.json',{})
        real=p.atomic_copy;visited=[]
        def failing(source,target):
            visited.append(str(target))
            if Path(target)==self.l/'world.studio': raise OSError('blocked scene restore')
            return real(source,target)
        with patch.object(p,'atomic_copy',side_effect=failing):
            errors=p.restore_publication([(self.n/'world-project/world.studio',self.l/'world.studio')],[],rollback,True,[])
        self.assertEqual(len(errors),1);self.assertIn(str(self.root/'CURRENT_VALWOOD.json'),visited)
        self.assertFalse((self.n/'UNIFIED_READY.json').exists())

class MatchedMatrix(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-matched-matrix-test-');self.n=Path(self.tmp.name)
        self.patch=patch.object(f,'N',self.n);self.patch.start();self.paths=[];self.comparisons={};self.inputs={}
        self.delivery=dict(shader_bank_reused=True,shader_changes=[],shader_override=None)
        self.delivery_patch=patch.object(f,'shader_delivery',return_value=self.delivery);self.delivery_patch.start()
        for tool in ('compare_forest_performance.py','freeze_review.py','review_frame_pacing.py'):
            path=self.n/'tools'/tool;write(path,'owned helper '+tool);self.inputs[str(path)]=c.sha(path)
        for label,view in f.VIEWS.items():
            row=dict(passed=True,matched_conditions=True,comparison_errors=[])
            for stage in ('baseline','candidate'):
                directory=self.n/'review/native'/stage/view
                prep=dict(stage=stage,view=view,generation=str(self.n/stage),binary_sha256='same binary',camera=dict(x=1,y=2,z=3,yaw=0,pitch=-.1,fov=1),
                    resolution=[1600,900],frames=120,warmup=60,quality_frozen=True,actual_settings_sha256='same settings')
                execution=dict(schema='valwood.forest-matched-views.execution.v1',passed=True,stage=stage,view=view,
                    water_quality=dict(level='High',ssr_steps=24),graphics=dict(preset='Custom',render_width=1600))
                write(directory/'PREPARATION.json',prep);write(directory/'EXECUTION.json',execution)
                for raw in ('FRAME_PACING_REVIEW.json','performance.json','performance.json.csv','performance.json.world.json'):
                    write(directory/raw,dict(fixture=True) if not raw.endswith('.csv') else 'frame,total_ms\n1,16\n')
                for path in directory.iterdir():self.inputs[str(path)]=c.sha(path)
                self.paths.append(directory/'EXECUTION.json')
                row[stage]=dict(directory=str(directory),passed=True,csv_frame_ids_match_declared_range=True,all_csv_rows_retained=120,
                    evidence_role=stage,preparation=prep,water_quality=execution['water_quality'],effective_graphics=execution['graphics'])
            self.comparisons[label]=row
        self.report_path=self.n/'review/MATCHED_NATIVE_PERFORMANCE.json'
        self.report=dict(schema='valwood.forest-matched-native-performance.v1',passed=True,sustained_fps_certified=False,
            AAA_quality_certified=False,capture_or_state_based_exclusions_applied=False,comparisons=self.comparisons,inputs_sha256=self.inputs,
            shader_delivery=dict(self.delivery))
        self.paths.append(self.report_path);self.flush()

    def tearDown(self):self.delivery_patch.stop();self.patch.stop();self.tmp.cleanup()

    def flush(self):write(self.report_path,self.report)

    def test_all_four_pairs_bind_the_actual_comparison(self):
        result=f.matched_native_matrix(self.paths);self.assertEqual(len(result['executions']),8)
        self.assertEqual(result['performance_sha256'],c.sha(self.report_path))

    def test_one_missing_view_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'Four final'):f.matched_native_matrix(self.paths[1:])

    def test_duplicate_view_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'Duplicate native'):f.matched_native_matrix([*self.paths,self.paths[0]])

    def test_missing_comparator_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'One final matched'):f.matched_native_matrix(self.paths[:-1])

    def test_report_for_different_native_directory_rejected(self):
        self.comparisons['camp']['candidate']['directory']=str(self.n/'other');self.flush()
        with self.assertRaisesRegex(RuntimeError,'selected native directory'):f.matched_native_matrix(self.paths)

    def test_stale_raw_timing_rejected(self):
        path=self.paths[0].parent/'performance.json.csv';write(path,'changed timing bytes')
        with self.assertRaisesRegex(RuntimeError,'input missing/stale'):f.matched_native_matrix(self.paths)

    def test_comparator_using_previous_freeze_helper_rejected(self):
        write(self.n/'tools/freeze_review.py','later helper change')
        with self.assertRaisesRegex(RuntimeError,'another helper'):f.matched_native_matrix(self.paths)

    def test_comparison_missing_shader_delivery_rejected(self):
        self.report.pop('shader_delivery');self.flush()
        with self.assertRaisesRegex(RuntimeError,'shader delivery differs'):f.matched_native_matrix(self.paths)

    def test_unmatched_preparation_rejected_even_if_report_claims_pass(self):
        directory=self.paths[1].parent;prep=json.loads((directory/'PREPARATION.json').read_text());prep['actual_settings_sha256']='different settings'
        write(directory/'PREPARATION.json',prep);self.inputs[str(directory/'PREPARATION.json')]=c.sha(directory/'PREPARATION.json')
        self.comparisons['camp']['candidate']['preparation']=prep;self.flush()
        with self.assertRaisesRegex(RuntimeError,'unmatched conditions'):f.matched_native_matrix(self.paths)

    def test_dropped_timing_rows_rejected(self):
        self.comparisons['camp']['candidate']['all_csv_rows_retained']=119;self.flush()
        with self.assertRaisesRegex(RuntimeError,'selected native directory'):f.matched_native_matrix(self.paths)

    def test_comparator_must_disclose_current_shader_delivery(self):
        self.report['shader_delivery']={'shader_bank_reused':False,'shader_changes':[]};self.flush()
        with self.assertRaisesRegex(RuntimeError,'shader delivery differs'):f.matched_native_matrix(self.paths)

    def test_sustained_fps_claim_rejected(self):
        self.report['sustained_fps_certified']=True;self.flush()
        with self.assertRaisesRegex(RuntimeError,'scope differs'):f.matched_native_matrix(self.paths)
