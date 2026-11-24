#!/usr/bin/env python3
"""Reject stale validation provenance before loading the private inventory cache."""
import hashlib,importlib.util,json,pickle,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
script=ROOT/'docs/world-designer/2026-11-05/build_designer_bundle.py'
spec=importlib.util.spec_from_file_location('designer_plan_guard',script)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
refresh_spec=importlib.util.spec_from_file_location('designer_refresh_guard',script.with_name('refresh_bundle_plan.py'))
refresh=importlib.util.module_from_spec(refresh_spec);refresh_spec.loader.exec_module(refresh)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
class PlanGuardTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.doc=self.root/'docs/proof';self.doc.mkdir(parents=True)
        self.runtime=self.root/'build/world-designer-2026-11-05/runtime';(self.runtime/'bin').mkdir(parents=True)
        (self.doc/'windows').mkdir();(self.root/'tools').mkdir()
        tools={}
        for n in ('world_asset_database.py','build_windows_package.py','build_windows_artist_bundle.py','world_artist_handoff.py','world_package_cache_selection.py'):
            p=self.root/'tools'/n;p.write_text(n);tools[n]=sha(p)
        self.exe=self.runtime/'bin/rpg.exe';self.exe.write_bytes(b'PE fixture')
        (self.runtime/'PROVENANCE.json').write_text(json.dumps({'files':[{'path':'rpg.exe','bytes':self.exe.stat().st_size,'sha256':sha(self.exe)}]}))
        (self.doc/'SOURCE_PREPARATION.json').write_text('{}')
        cache=self.doc/'validated-inputs.private.pickle';cache.write_bytes(pickle.dumps([]))
        self.data=dict(tool_sha256=tools,source_preparation_sha256=sha(self.doc/'SOURCE_PREPARATION.json'),
            runtime_provenance_sha256=sha(self.runtime/'PROVENANCE.json'),private_cache=str(cache),private_cache_sha256=sha(cache))
        (self.doc/'DATA_PLAN.json').write_text(json.dumps(self.data))
        self.rootpatch=mock.patch.multiple(module,ROOT=self.root,DOC=self.doc,FRONT=self.root/'frontend');self.rootpatch.start()
    def tearDown(self):self.rootpatch.stop();self.tmp.cleanup()
    def reject_before_cache(self,expected):
        with mock.patch.object(module.pickle,'load',side_effect=RuntimeError('Unpickle must not run')):
            with self.assertRaisesRegex(AssertionError,expected):module.inputs()
    def test_valid_local_cache_and_bound_runtime_accepted(self):
        data,projects,extras=module.inputs();self.assertEqual(projects,[]);self.assertEqual(len(extras),2)
    def test_changed_helper_or_provenance_rejected_before_unpickle(self):
        for p,expected in [(self.root/'tools/world_package_cache_selection.py','Validated helper changed'),
                (self.doc/'SOURCE_PREPARATION.json','Source preparation provenance changed'),
                (self.runtime/'PROVENANCE.json','Runtime provenance changed')]:
            before=p.read_bytes();p.write_bytes(before+b' ')
            with self.subTest(path=p):self.reject_before_cache(expected)
            p.write_bytes(before)
    def test_runtime_byte_change_and_extra_file_rejected_before_unpickle(self):
        before=self.exe.read_bytes();self.exe.write_bytes(b'PE changed')
        self.reject_before_cache('Validated runtime bytes changed');self.exe.write_bytes(before)
        (self.runtime/'bin/extra.dll').write_bytes(b'extra')
        self.reject_before_cache('Validated runtime inventory changed')
    def test_bad_go_rejected_before_loading_private_inventory(self):
        plan=self.doc/'FINAL.json';plan.write_text('{}')
        argv=[str(script),'--plan-path',str(plan),'--data-plan-path',str(self.doc/'DATA_PLAN.json'),'--build','--go-sha256','0'*64]
        with mock.patch.object(sys,'argv',argv),mock.patch.object(module,'plan',side_effect=RuntimeError('Must not load inventory')):
            with self.assertRaisesRegex(AssertionError,'Root GO must name exact reviewed plan SHA'):module.main()
    def test_only_reviewed_readme_packager_revision_bridges_prior_inventory(self):
        p=self.root/'tools/build_windows_artist_bundle.py';old=p.read_bytes()
        before=self.doc/'before/post-data-plan-build_windows_artist_bundle.py';before.parent.mkdir();before.write_bytes(old)
        p.write_bytes(old+b'\nREADME only revision')
        log=self.doc/'cpu-designer-package-readme-final.log';log.write_text('reviewed tests PASS')
        review=dict(tests_passed=True,before_sha256=hashlib.sha256(old).hexdigest(),after_sha256=sha(p),test_log_sha256=sha(log))
        (self.doc/'PACKAGER_README_REVISION.json').write_text(json.dumps(review))
        with mock.patch.multiple(refresh,ROOT=self.root,DOC=self.doc):
            data,projects,current=refresh.prior_inventory();self.assertEqual(projects,[]);self.assertEqual(current[p.name],sha(p))
            log.write_text('changed log')
            with mock.patch.object(refresh.pickle,'load',side_effect=RuntimeError('No stale unpickle')):
                with self.assertRaises(AssertionError):refresh.prior_inventory()
            log.write_text('reviewed tests PASS')
            (self.root/'tools/world_package_cache_selection.py').write_text('unreviewed selection change')
            with mock.patch.object(refresh.pickle,'load',side_effect=RuntimeError('No stale unpickle')):
                with self.assertRaisesRegex(AssertionError,'Only the reviewed README packager change'):refresh.prior_inventory()
if __name__=='__main__':unittest.main()
