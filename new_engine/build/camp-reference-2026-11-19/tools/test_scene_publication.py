"""Owned /tmp publication fault/guard tests; never publish a real generation."""
from pathlib import Path
import hashlib,importlib.util,json,os,shutil,sys,tempfile,types,unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import check_unified as c


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m);return m


p=module('scene_art_promote_test',ROOT/'tools/promote.py')
f=module('scene_art_freeze_test',ROOT/'tools/freeze_review.py')
v=module('scene_art_baseline_adapter_test',ROOT/'tools/verify_baseline.py')
h=module('scene_art_static_harness_test',ROOT/'tools/run_forest_review.py')


def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value)+'\n' if isinstance(value,(dict,list)) else value)


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
        write(self.n/'BASELINE.json',dict(current_pointer_sha256=c.sha(self.root/'CURRENT_VALWOOD.json')))
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


class Reuse(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-scene-reuse-test-');self.root=Path(self.tmp.name)
        self.n=self.root/'candidate';self.b=self.root/'baseline';self.n.mkdir();self.b.mkdir();self.patches=[]
        for relative in c.IMMUTABLE:
            q=self.b/relative
            if relative=='runtime/rpg': write(q,'exact binary')
            else:q.mkdir(parents=True,exist_ok=True)
            pth=self.n/relative;pth.parent.mkdir(parents=True,exist_ok=True);pth.symlink_to(q,target_is_directory=q.is_dir())
        for i in range(203):write(self.b/'shared/shaders'/f'{i:03}.glsl','shader'+str(i))
        write(self.b/'shared/shaders/world_surface_library.fs.glsl','original surface shader')
        write(self.b/'UNIFIED_READY.json',{'ready':True})
        self.base=dict(baseline=str(self.b),runtime_reused=True,source_reused=True,shader_bank_reused=True,
                       Windows_work_performed=False,ready_sha256=c.sha(self.b/'UNIFIED_READY.json'),
                       binary_sha256=c.sha(self.b/'runtime/rpg'),shader_sha256=c.inventory(self.b/'shared/shaders'))
        write(self.n/'BASELINE.json',self.base)
        for name,value in [('N',self.n),('B',self.b)]:
            z=patch.object(c,name,value);z.start();self.patches.append(z)

    def tearDown(self):
        for z in reversed(self.patches):z.stop()
        self.tmp.cleanup()

    def test_real_alias_contract(self): self.assertEqual(len(c.verify_reuse(inherited=False)[1]),204)

    def test_wrong_alias_same_bytes_rejected(self):
        q=self.root/'other';write(q,'exact binary');(self.n/'runtime/rpg').unlink();(self.n/'runtime/rpg').symlink_to(q)
        with self.assertRaisesRegex(RuntimeError,'immutable alias'): c.verify_reuse(inherited=False)

    def test_owned_source_instead_of_alias_rejected(self):
        (self.n/'source-v1').unlink();(self.n/'source-v1').mkdir()
        with self.assertRaisesRegex(RuntimeError,'immutable alias'): c.verify_reuse(inherited=False)

    def test_added_shader_rejected(self):
        write(self.b/'shared/shaders/extra.glsl','unreviewed')
        with self.assertRaisesRegex(RuntimeError,'204-shader'): c.verify_reuse(inherited=False)

    def test_changed_shader_rejected(self):
        write(self.b/'shared/shaders/000.glsl','unreviewed')
        with self.assertRaisesRegex(RuntimeError,'204-shader'): c.verify_reuse(inherited=False)

    def install_override(self):
        bank=self.n/'shared/shaders';bank.unlink();shutil.copytree(self.b/'shared/shaders',bank)
        write(bank/'world_surface_library.fs.glsl','meter-based terrain projection correction')
        self.override_path=self.n/'review/SHADER_OVERRIDE.json'
        self.override=dict(schema='valwood.reviewed-shader-override.v1',reviewed=True,root_freeze_authorized=True,
            runtime_reused=True,CXX_source_reused=True,shader_bank_reused=False,shader_count=204,
            baseline_shader_bank=str(self.b/'shared/shaders'),candidate_shader_bank=str(bank),
            changes=[dict(path='world_surface_library.fs.glsl',before_sha256=c.sha(self.b/'shared/shaders/world_surface_library.fs.glsl'),after_sha256=c.sha(bank/'world_surface_library.fs.glsl'))])
        write(self.override_path,self.override)

    def test_explicit_one_shader_override_preserves_binary_source(self):
        self.install_override();binary,bank=c.verify_reuse(inherited=False)
        self.assertEqual(binary,self.base['binary_sha256']);self.assertEqual(len(bank),204)
        self.assertFalse(c.shader_delivery()['shader_bank_reused']);self.assertEqual(c.shader_delivery()['shader_changes'],self.override['changes'])

    def test_second_shader_change_rejected(self):
        self.install_override();write(self.n/'shared/shaders/000.glsl','unapproved second shader')
        with self.assertRaisesRegex(RuntimeError,'Only the reviewed'):c.verify_reuse(inherited=False)

    def test_wrong_override_before_hash_rejected(self):
        self.install_override();self.override['changes'][0]['before_sha256']='wrong';write(self.override_path,self.override)
        with self.assertRaisesRegex(RuntimeError,'Only the reviewed'):c.verify_reuse(inherited=False)

    def test_override_missing_root_authorization_rejected(self):
        self.install_override();self.override['root_freeze_authorized']=False;write(self.override_path,self.override)
        with self.assertRaisesRegex(RuntimeError,'root authorization'):c.verify_reuse(inherited=False)

    def test_override_extra_path_rejected(self):
        self.install_override();write(self.n/'shared/shaders/extra.glsl','extra')
        with self.assertRaisesRegex(RuntimeError,'204 shader paths'):c.verify_reuse(inherited=False)

    def test_override_shared_parent_alias_rejected(self):
        self.install_override();other=self.root/'other-shared';(self.n/'shared').rename(other)
        (self.n/'shared').symlink_to(other,target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError,'must be owned'):c.verify_reuse(inherited=False)

    def test_override_owned_file_alias_rejected(self):
        self.install_override();path=self.n/'shared/shaders/000.glsl';path.unlink();path.symlink_to(self.b/'shared/shaders/000.glsl')
        with self.assertRaisesRegex(RuntimeError,'unsafe alias'):c.verify_reuse(inherited=False)

    def test_baseline_ready_changed_rejected(self):
        write(self.b/'UNIFIED_READY.json',{'ready':True,'changed':True})
        with self.assertRaisesRegex(RuntimeError,'receipt changed'): c.verify_reuse(inherited=False)


class NativePins(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-scene-native-pins-test-')
        self.root=Path(self.tmp.name);self.n=self.root/'candidate';self.b=self.root/'baseline'
        self.out=self.n/'review/native';self.private=self.root/'private'
        write(self.b/'runtime/rpg','exact binary');(self.n/'runtime').mkdir(parents=True)
        (self.n/'runtime/rpg').symlink_to(self.b/'runtime/rpg')
        for i in range(204):write(self.b/'shared/shaders'/f'{i:03}.glsl','shader'+str(i))
        (self.n/'shared').mkdir();(self.n/'shared/shaders').symlink_to(self.b/'shared/shaders',target_is_directory=True)
        self.inputs={str(x):c.sha(x) for x in (self.b/'shared/shaders').glob('*')}
        for name in ('world.studio','collision/structures/catalog'):
            path=self.n/'world-project'/name;write(path,'current '+name);self.inputs[str(path)]=c.sha(path)
        write(self.private/'world.studio',(self.n/'world-project/world.studio').read_text())
        self.prep=dict(schema='valwood.forest-matched-views.preparation.v1',stage='candidate',view='camp-eye',
            generation=str(self.n),binary_sha256=c.sha(self.n/'runtime/rpg'),quality_frozen=True,
            command=[str(self.n/'runtime/rpg')],private_project=str(self.private),input_sha256=self.inputs,
            private_input_sha256={str(self.private/'world.studio'):c.sha(self.private/'world.studio')})
        write(self.out/'PREPARATION.json',self.prep)
        write(self.out/'FRAME_PACING_REVIEW.json',dict(inputs={}))
        self.doc=dict(schema='valwood.forest-matched-views.execution.v1',passed=True,stage='candidate',
            view='camp-eye',binary_sha256=self.prep['binary_sha256'],quality_frozen=True,
            preparation_sha256=c.sha(self.out/'PREPARATION.json'),
            frame_pacing_report_sha256=c.sha(self.out/'FRAME_PACING_REVIEW.json'))
        write(self.out/'EXECUTION.json',self.doc);self.patches=[]
        for name,value in [('N',self.n),('B',self.b)]:
            z=patch.object(f,name,value);z.start();self.patches.append(z)
        for owner in (f,c):
            z=patch.object(owner,'shader_delivery',return_value=dict(shader_bank_reused=True,shader_changes=[],shader_override=None));z.start();self.patches.append(z)

    def tearDown(self):
        for z in reversed(self.patches):z.stop()
        self.tmp.cleanup()

    def refresh(self):
        write(self.out/'PREPARATION.json',self.prep)
        self.doc['preparation_sha256']=c.sha(self.out/'PREPARATION.json');write(self.out/'EXECUTION.json',self.doc)

    def test_native_final_candidate_inputs_pinned(self):
        pins={};self.assertTrue(f.pin_receipt(self.out/'EXECUTION.json',pins))
        self.assertIn(str(self.out/'PREPARATION.json'),pins)
        # /tmp input fixtures are verified but deliberately not retained as
        # installed runtime dependencies; their preparation digest is retained.
        self.assertIn(str(self.out/'EXECUTION.json'),pins)

    def test_candidate_native_must_pin_its_owned_override_bank(self):
        bank=self.n/'shared/shaders';bank.unlink();shutil.copytree(self.b/'shared/shaders',bank);write(bank/'000.glsl','candidate shader')
        self.inputs={name:h for name,h in self.inputs.items() if not Path(name).is_relative_to(self.b/'shared/shaders')}
        self.inputs.update({str(p):c.sha(p) for p in bank.glob('*')});self.prep['input_sha256']=self.inputs;self.refresh()
        self.assertTrue(f.pin_receipt(self.out/'EXECUTION.json',{}))

    def test_old_baseline_bank_cannot_satisfy_candidate_override(self):
        bank=self.n/'shared/shaders';bank.unlink();shutil.copytree(self.b/'shared/shaders',bank);write(bank/'000.glsl','candidate shader')
        with self.assertRaisesRegex(RuntimeError,'complete reused shader bank'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_stale_preparation_rejected(self):
        write(self.out/'PREPARATION.json',dict(self.prep,changed=True))
        with self.assertRaisesRegex(RuntimeError,'preparation pin'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_missing_shader_pin_rejected(self):
        self.inputs.pop(next(iter(self.inputs)));self.refresh()
        with self.assertRaisesRegex(RuntimeError,'complete reused shader'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_stale_scene_rejected(self):
        write(self.n/'world-project/world.studio','after capture edit')
        with self.assertRaisesRegex(RuntimeError,'final scene/catalog'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_missing_catalog_pin_rejected(self):
        self.inputs.pop(str(self.n/'world-project/collision/structures/catalog'));self.refresh()
        with self.assertRaisesRegex(RuntimeError,'final scene/catalog'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_private_scene_differs_rejected(self):
        write(self.private/'world.studio','unrelated private scene')
        self.prep['private_input_sha256'][str(self.private/'world.studio')]=c.sha(self.private/'world.studio');self.refresh()
        with self.assertRaisesRegex(RuntimeError,'private world differs'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def test_stale_pacing_rejected(self):
        write(self.out/'FRAME_PACING_REVIEW.json',dict(inputs={},edited=True))
        with self.assertRaisesRegex(RuntimeError,'pacing report differs'):f.pin_receipt(self.out/'EXECUTION.json',{})

    def freeze_fixture(self):
        for name in ('BASELINE.json','review/LIVE_BEFORE.json','review/ACTIVE_SETTINGS.json'):
            write(self.n/name,{})
        for name in ('launch.py','check_unified.py','GUIDE.txt'):write(self.n/name,'fixture')
        for name in ('UNIFIED_READY.json','PUBLICATION.json'):write(self.b/name,{'passed':True})
        write(self.b/'check_unified.py','fixture baseline checker')
        write(self.n/'review/FINAL_REVIEW.json',dict(passed=True,visual_review_performed=True,AAA_quality_certified=False))
        self.binary=c.sha(self.n/'runtime/rpg');self.bank=c.inventory(self.b/'shared/shaders')
        self.delta={'world.studio':dict(before=dict(kind='file',sha256='old'),after=dict(kind='file',sha256=c.sha(self.n/'world-project/world.studio')))}
        for owner,name,value in [(f,'verify_reuse',lambda:(self.binary,self.bank)),(f,'scene_scope',lambda:(self.delta,[])),
                                 (f,'catalog_transition',lambda:None),(f,'matched_native_matrix',lambda paths:dict(unit_fixture=True))]:
            z=patch.object(owner,name,value);z.start();self.patches.append(z)

    def test_freeze_manifest_satisfies_scene_only_checker(self):
        self.freeze_fixture();manifest=f.freeze(['review/native/EXECUTION.json'])
        with patch.object(c,'verify_reuse',return_value=(self.binary,self.bank)),patch.object(c,'scene_scope',return_value=(self.delta,[])),patch.object(c,'catalog_transition',return_value=None):
            self.assertEqual(c.check_inputs(manifest),self.binary)
        self.assertEqual(manifest['source_changes'],[]);self.assertEqual(manifest['shader_changes'],[])

    def test_freeze_requires_final_candidate_native(self):
        self.freeze_fixture();write(self.n/'review/cpu.json',{'passed':True})
        with self.assertRaisesRegex(RuntimeError,'candidate native'):f.freeze(['review/cpu.json'])

    def test_freeze_rejects_escape_receipt(self):
        self.freeze_fixture()
        with self.assertRaisesRegex(RuntimeError,'owned review receipt'):f.freeze(['../../escape.json'])


class CatalogTransition(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-catalog-transition-test-');self.root=Path(self.tmp.name)
        self.n=self.root/'candidate';self.b=self.root/'baseline';self.l=self.root/'live';self.n.mkdir();self.b.mkdir();self.l.mkdir()
        self.relative='collision/structures/catalog';self.before='VALWOOD_STRUCTURE_COLLISION 1\n'
        write(self.b/'world-project'/self.relative,self.before);write(self.n/'review/tree-variants/catalog.before',self.before)
        self.names=['oak02','oak04','beech01','beech02'];self.models=[];self.collisions=[];self.rows=[]
        for i,name in enumerate(self.names):
            for lod in ('near','mid'):
                rel=f'models/camp_variants/camp_variant_{name}_{lod}.glb';write(self.n/'world-project'/rel,'GLB '+name+lod)
                self.models.append(dict(path=rel,sha256=c.sha(self.n/'world-project'/rel)))
            cache=f'collision/structures/camp_variant_{name}_trunk.vwc';write(self.n/'world-project'/cache,'VWC '+name)
            self.collisions.append(dict(path=cache,sha256=c.sha(self.n/'world-project'/cache)))
            near=f'models/camp_variants/camp_variant_{name}_near.glb';alias='nm_camp_variant_'+name
            self.rows.extend([f'mesh "{alias}" "{near}" "{cache}"',f'model "{alias}" {(self.n/"world-project"/near).stat().st_size} 1000000000000001'])
        write(self.n/'world-project'/self.relative,self.before+'\n'.join(self.rows)+'\n')
        self.report=dict(schema='valwood.camp-tree-variants.v1',passed=True,production_catalog_and_cache_original_pass=True,
            preimage_path=str(self.n/'review/tree-variants/catalog.before'),before_sha256=c.sha(self.b/'world-project'/self.relative),
            after_sha256=c.sha(self.n/'world-project'/self.relative),catalog_relative_path=self.relative,
            added_catalog_rows=self.rows,changed_aliases=['nm_camp_variant_'+x for x in self.names],input_sha256={},
            model_additions=[dict(row) for row in self.models],collision_additions=[dict(row) for row in self.collisions])
        self.review=self.n/'review/tree-variants/TREE_VARIANT_REVIEW.json';self.scope=self.n/'review/MODEL_ADDITIONS.json'
        self.patches=[]
        for owner,name,value in [(c,'N',self.n),(c,'B',self.b),(c,'L',self.l),(v,'N',self.n)]:
            z=patch.object(owner,name,value);z.start();self.patches.append(z)
        self.refresh()

    def tearDown(self):
        for z in reversed(self.patches):z.stop()
        self.tmp.cleanup()

    def refresh(self):
        write(self.review,self.report)
        write(self.scope,dict(schema='valwood.scene-art.model-additions.v1',reviewed=True,root_freeze_authorized=True,
            collision_catalog_unchanged=False,additions=self.models,collision_additions=self.collisions,
            tree_review=dict(path='review/tree-variants/TREE_VARIANT_REVIEW.json',sha256=c.sha(self.review))))

    def test_exact_append_transition(self):
        result=c.catalog_transition();self.assertEqual(result['after_sha256'],self.report['after_sha256'])
        delta,added=c.scene_scope();self.assertEqual(len(added),12);self.assertEqual(len(delta),13)

    def test_existing_catalog_edit_rejected(self):
        path=self.n/'world-project'/self.relative;write(path,path.read_text().replace('COLLISION 1','COLLISION 2'))
        self.report['after_sha256']=c.sha(path);self.refresh()
        with self.assertRaisesRegex(RuntimeError,'Existing catalog rows changed'):c.catalog_transition()

    def test_wrong_preimage_rejected(self):
        path=Path(self.report['preimage_path']);write(path,self.before+'unrelated\n');self.report['before_sha256']=c.sha(path);self.refresh()
        with self.assertRaisesRegex(RuntimeError,'exact before/after'):c.catalog_transition()

    def test_unapproved_catalog_rejected(self):
        self.scope.unlink()
        with self.assertRaises(RuntimeError):c.scene_scope()

    def test_alias_count_rejected(self):
        self.report['changed_aliases'].pop();self.refresh()
        with self.assertRaisesRegex(RuntimeError,'four reviewed'):c.catalog_transition()

    def test_changed_vwc_rejected(self):
        write(self.n/'world-project'/self.collisions[0]['path'],'different cache')
        with self.assertRaisesRegex(RuntimeError,'new owned reviewed'):c.catalog_transition()

    def test_unreviewed_mesh_reference_rejected(self):
        self.rows[0]=self.rows[0].replace(self.models[0]['path'],'models/old-tree.glb')
        write(self.n/'world-project'/self.relative,self.before+'\n'.join(self.rows)+'\n')
        self.report['after_sha256']=c.sha(self.n/'world-project'/self.relative);self.refresh()
        with self.assertRaisesRegex(RuntimeError,'Invalid appended'):c.catalog_transition()

    def adapter_fixture(self,live_text):
        write(self.l/self.relative,live_text);write(self.b/'UNIFIED_READY.json',{'ready':True})
        write(self.n/'BASELINE.json',dict(ready_sha256=c.sha(self.b/'UNIFIED_READY.json')))
        # This isolated verifier stands in for the immutable baseline; the real
        # installed baseline checker is exercised separately on actual aliases.
        write(self.b/'check_unified.py',f'''from pathlib import Path
import hashlib
def owned_project_leaf(root,relative): return Path(root)/relative
def check():
 p=owned_project_leaf(Path({str(self.l)!r}), 'collision/structures/catalog')
 assert hashlib.sha256(p.read_bytes()).hexdigest()=={self.report['before_sha256']!r}
 assert owned_project_leaf(Path({str(self.l)!r}), 'unrelated')==Path({str(self.l/'unrelated')!r})
''')

    def test_adapter_accepts_reviewed_before(self):
        self.adapter_fixture(self.before);v.verify()

    def test_adapter_accepts_reviewed_after(self):
        self.adapter_fixture((self.n/'world-project'/self.relative).read_text());v.verify()

    def test_adapter_rejects_third_live_state(self):
        self.adapter_fixture('artist changed catalog')
        with self.assertRaisesRegex(RuntimeError,'both reviewed states'):v.verify()

    def test_new_HDR_and_surface_resources_allowed_only_when_explicit(self):
        resources=[]
        for rel in ('environments/camp.hdr','surfaces/camp/ground_color.png'):
            write(self.n/'world-project'/rel,'owned image '+rel)
            resources.append(dict(path=rel,sha256=c.sha(self.n/'world-project'/rel)))
        scope=json.loads(self.scope.read_text());scope['resource_additions']=resources;write(self.scope,scope)
        self.assertEqual(c.resource_additions(),resources)
        delta,assets=c.scene_scope();self.assertEqual(len(assets),14)
        scope.pop('resource_additions');write(self.scope,scope)
        with self.assertRaisesRegex(RuntimeError,'Unreviewed scene/model'):c.scene_scope()

    def test_extra_reviewed_moss_models_do_not_weaken_tree_subset(self):
        extra=[]
        for i in range(12):
            rel=f'models/camp_floor/moss{i}.glb';write(self.n/'world-project'/rel,'owned moss '+str(i))
            extra.append(dict(path=rel,sha256=c.sha(self.n/'world-project'/rel)))
        scope=json.loads(self.scope.read_text());scope['additions']+=extra;write(self.scope,scope)
        self.assertIsNotNone(c.catalog_transition());delta,assets=c.scene_scope();self.assertEqual(len(assets),24)

    def test_owned_existing_resource_never_overwritten(self):
        rel='surfaces/existing.png';write(self.n/'world-project'/rel,'replacement');write(self.b/'world-project'/rel,'original')
        scope=json.loads(self.scope.read_text());scope['resource_additions']=[dict(path=rel,sha256=c.sha(self.n/'world-project'/rel))];write(self.scope,scope)
        with self.assertRaisesRegex(RuntimeError,'must be a new owned'):c.resource_additions()

    def test_resource_path_allowlist_rejects_executable(self):
        rel='surfaces/unreviewed.exe';write(self.n/'world-project'/rel,'not image')
        scope=json.loads(self.scope.read_text());scope['resource_additions']=[dict(path=rel,sha256=c.sha(self.n/'world-project'/rel))];write(self.scope,scope)
        with self.assertRaisesRegex(RuntimeError,'Invalid/duplicate'):c.resource_additions()


class ResourceOverlays(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-resource-overlay-test-');self.root=Path(self.tmp.name)
        self.n=self.root/'candidate';self.b=self.root/'baseline';self.l=self.root/'live';self.patches=[]
        for name,value in [('N',self.n),('B',self.b),('L',self.l)]:
            item=patch.object(c,name,value);item.start();self.patches.append(item)
        for name in ('surfaces','environments'):
            parent=self.l/name;parent.mkdir(parents=True)
            old=self.b/'world-project'/name;old.parent.mkdir(parents=True,exist_ok=True);old.symlink_to(parent,target_is_directory=True)
        write(self.l/'surfaces/original.png','original pixels')
        write(self.l/'environments/original.hdr','original sky')
        write(self.n/'world-project/surfaces/original.png','original pixels')
        (self.n/'world-project/environments').mkdir(parents=True)
        (self.n/'world-project/environments/original.hdr').symlink_to(self.l/'environments/original.hdr')
        self.resources=[]
        for relative in ('surfaces/camp/new.png','environments/new.hdr'):
            path=self.n/'world-project'/relative;write(path,'new '+relative)
            self.resources.append(dict(path=relative,sha256=c.sha(path)))
        for owner in (self.b,self.n):write(owner/'world-project/collision/structures/catalog','same catalog')
        write(self.n/'review/MODEL_ADDITIONS.json',dict(schema='valwood.scene-art.model-additions.v1',reviewed=True,
            root_freeze_authorized=True,collision_catalog_unchanged=True,resource_additions=self.resources))

    def tearDown(self):
        for item in reversed(self.patches):item.stop()
        self.tmp.cleanup()

    def test_only_new_resources_are_logical_delta(self):
        delta,added=c.scene_scope();self.assertEqual(set(delta),{r['path'] for r in self.resources});self.assertEqual(added,self.resources)
        overlays=c.resource_overlays();self.assertEqual(set(overlays),{'surfaces','environments'})
        self.assertEqual(overlays['surfaces']['original_leaves']['surfaces/original.png']['sha256'],c.sha(self.l/'surfaces/original.png'))
        self.assertEqual((self.l/'surfaces/original.png').read_text(),'original pixels')

    def test_overlay_contract_is_bound_in_changeset(self):
        delta,added=c.scene_scope();overlays=c.resource_overlays()
        manifest=dict(schema='valwood.scene-art.changeset.v1',reviewed=True,runtime_reused=True,
            source_reused=True,shader_bank_reused=True,Windows_work_performed=False,source_changes=[],shader_changes=[],
            binary_sha256='same binary',shader_sha256={},scene_delta=delta,model_additions=added,
            catalog_transition=None,resource_overlays=overlays,pins={},aliases={})
        with patch.object(c,'verify_reuse',return_value=('same binary',{})):
            self.assertEqual(c.check_inputs(manifest),'same binary')
            manifest['resource_overlays']={}
            with self.assertRaisesRegex(RuntimeError,'preserved resource overlays'):c.check_inputs(manifest)

    def test_original_owned_copy_change_rejected(self):
        write(self.n/'world-project/surfaces/original.png','different pixels')
        with self.assertRaisesRegex(RuntimeError,'Existing resource overlay leaf changed/missing'):c.scene_scope()

    def test_original_deleted_rejected(self):
        (self.n/'world-project/surfaces/original.png').unlink()
        with self.assertRaisesRegex(RuntimeError,'Existing resource overlay leaf changed/missing'):c.scene_scope()

    def test_original_live_change_after_staging_rejected(self):
        write(self.l/'surfaces/original.png','artist changed original')
        with self.assertRaisesRegex(RuntimeError,'Existing resource overlay leaf changed/missing'):c.scene_scope()

    def test_unapproved_owned_addition_rejected(self):
        write(self.n/'world-project/surfaces/extra.png','unreviewed')
        with self.assertRaisesRegex(RuntimeError,'Unreviewed resource overlay addition'):c.scene_scope()

    def test_original_dangling_alias_rejected(self):
        (self.n/'world-project/environments/original.hdr').unlink()
        (self.n/'world-project/environments/original.hdr').symlink_to(self.root/'missing')
        with self.assertRaisesRegex(RuntimeError,'Dangling resource overlay leaf'):c.scene_scope()

    def test_baseline_dangling_alias_rejected(self):
        (self.l/'environments/original.hdr').unlink();(self.l/'environments/original.hdr').symlink_to(self.root/'missing')
        with self.assertRaisesRegex(RuntimeError,'Dangling resource overlay leaf'):c.scene_scope()

    def test_nested_directory_alias_rejected(self):
        other=self.root/'other';write(other/'injected.png','new')
        (self.n/'world-project/surfaces/external').symlink_to(other,target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError,'Nested resource directory alias'):c.scene_scope()

    def test_existing_resource_cannot_be_approved_as_new(self):
        doc=c.read(self.n/'review/MODEL_ADDITIONS.json');doc['resource_additions'].append(dict(path='surfaces/original.png',sha256=c.sha(self.l/'surfaces/original.png')))
        write(self.n/'review/MODEL_ADDITIONS.json',doc)
        with self.assertRaisesRegex(RuntimeError,'new owned reviewed'):c.scene_scope()

    def test_arbitrary_directory_expansion_still_rejected(self):
        old=self.b/'world-project/road-traffic';old.symlink_to(self.l/'surfaces',target_is_directory=True)
        write(self.n/'world-project/road-traffic/original.png','original pixels')
        with self.assertRaisesRegex(RuntimeError,'Unreviewed scene/model scope'):c.scene_scope()


class CaptureResourcePins(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='valwood-scene-resource-pins-test-');self.origin=Path(self.tmp.name)
        write(self.origin/'world.studio','environment environments/camp.hdr 0 .33 .50 1\nsurfaces "image \\"Shore color\\" \\"surfaces/shore.png\\" 0"\n')
        write(self.origin/'environments/camp.hdr','HDR');write(self.origin/'surfaces/shore.png','PNG')
        write(self.origin/'models/tree.glb','GLB');write(self.origin/'collision/structures/tree.vwc','VWC')
        write(self.origin/'collision/structures/catalog','VALWOOD_STRUCTURE_COLLISION 1\nmesh "tree" "models/tree.glb" "collision/structures/tree.vwc"\n')

    def tearDown(self):self.tmp.cleanup()

    def test_HDR_surface_mesh_and_VWC_are_actual_inputs(self):
        inputs=h.scene_inputs(self.origin)
        for relative in ('environments/camp.hdr','surfaces/shore.png','models/tree.glb','collision/structures/tree.vwc'):
            self.assertEqual(inputs[str(self.origin/relative)],c.sha(self.origin/relative))
        self.assertTrue(h.verify_pins(inputs))
        write(self.origin/'collision/structures/tree.vwc','changed cache')
        self.assertFalse(h.verify_pins(inputs))

    def test_missing_HDR_rejected(self):
        (self.origin/'environments/camp.hdr').unlink()
        with self.assertRaisesRegex(ValueError,'Missing declared'):h.scene_inputs(self.origin)

    def test_escaping_HDR_rejected(self):
        write(self.origin/'world.studio','environment ../outside.hdr 0 .33 .50 1\n')
        with self.assertRaisesRegex(ValueError,'Escaping scene resource'):h.scene_inputs(self.origin)

    def test_same_water_tier_allowed_for_both_matched_stages(self):
        calls=[]
        with patch.object(sys,'argv',['review','--name','test','--view','camp-eye','--stage','both','--water-quality','High','--prepare-only']),patch.object(h,'execute',side_effect=lambda args,stage,view:(calls.append((stage,args.water_quality)) or True)):
            with self.assertRaises(SystemExit) as result:h.main()
        self.assertEqual(result.exception.code,0);self.assertEqual(calls,[('baseline','High'),('candidate','High')])


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
                    preparation=prep,water_quality=execution['water_quality'],effective_graphics=execution['graphics'])
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


if __name__=='__main__': unittest.main(verbosity=2)
