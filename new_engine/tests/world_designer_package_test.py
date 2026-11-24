#!/usr/bin/env python3
"""World-designer opening/blood closure and explicit bootstrap CPU contracts."""
import hashlib,json
from pathlib import Path
import sys,tempfile,unittest,zipfile
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tests')]
from world_asset_database_test import fixture,glb
from world_asset_database import ProjectInventory,ValidationError
from world_artist_handoff import authoring_path
import windows_artist_bundle_test as bundle_tests
import build_windows_artist_bundle as bundle

class OpeningClosureTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.engine,self.project=fixture(Path(self.tmp.name))
        p=self.project;(p/'opening').mkdir();(p/'blood_assets').mkdir();(p/'models/longsword_mocap').mkdir()
        (p/'opening.config').write_text('VALWOOD_OPENING_SCENE 1\n')
        (p/'opening.scene').write_text(json.dumps({'source':'/home/private/Downloads/source.blend','runtime_dependencies':['opening/home.physics']}))
        (p/'opening/home.settings').write_text('numeric engine settings\n')
        (p/'opening/home.physics').write_text('physics\n')
        (p/'blood_assets/blood.bank').write_bytes(b'VWBLOOD1fixture')
        (p/'blood_assets/provenance.json').write_text('{}')
        (p/'blood_assets/BLOOD_ASSETS.json').write_text(json.dumps({'assets':[{'file':'blood.bank'},{'file':'provenance.json'}]}))
        (p/'models/longsword_mocap/pose.glb').write_bytes(glb())
        (p/'CHARACTER_POLISH.json').write_text(json.dumps({'files':[{'file':'pose.glb'}]}))
    def tearDown(self):self.tmp.cleanup()
    def db(self):return ProjectInventory(self.project,self.engine,active_imports_only=True).build()
    def test_new_authoring_files_and_explicit_closure(self):
        db=self.db();rows={r['path']:r for r in db['files']}
        for name in ('opening.config','opening.scene','opening/home.physics','blood_assets/blood.bank','CHARACTER_POLISH.json'):
            self.assertIn(name,rows);self.assertTrue(authoring_path(name))
        self.assertEqual(rows['blood_assets/BLOOD_ASSETS.json']['dependency_paths'],['blood_assets/blood.bank','blood_assets/provenance.json'])
        self.assertEqual(rows['CHARACTER_POLISH.json']['dependency_paths'],['models/longsword_mocap/pose.glb'])
        self.assertNotIn('/home/private',json.dumps(db))
    def test_missing_bank_pose_or_home_rejected(self):
        for name in ('blood_assets/blood.bank','models/longsword_mocap/pose.glb','opening/home.physics'):
            p=self.project/name;raw=p.read_bytes();p.unlink()
            with self.subTest(name=name),self.assertRaisesRegex(ValidationError,'Missing runtime dependency'):self.db()
            p.write_bytes(raw)
    def test_bank_traversal_and_private_files_cannot_enter_payload(self):
        p=self.project;(p/'blood_assets/saves').mkdir();(p/'blood_assets/saves/private.bank').write_bytes(b'private')
        (p/'opening/secret.blend').write_bytes(b'raw-source')
        rows={r['path'] for r in self.db()['files']}
        self.assertNotIn('blood_assets/saves/private.bank',rows);self.assertNotIn('opening/secret.blend',rows)
        for reference in ('../saves/private.bank','/tmp/private.bank','saves/private.bank'):
            (p/'blood_assets/BLOOD_ASSETS.json').write_text(json.dumps({'assets':[{'file':reference}]}))
            with self.subTest(reference=reference),self.assertRaises(ValidationError):self.db()
    def test_unknown_opening_scenery_alias_rejected(self):
        (self.project/'scenery/fixtures_instances.txt').write_text('absent_alias 0 0 0 1 0 0\n')
        with self.assertRaisesRegex(ValidationError,'Unknown scenery asset alias'):self.db()

class BootstrapTest(bundle_tests.WindowsArtistBundleTest):
    def test_explicit_bootstrap_and_refreshed_docs_are_manifest_bound(self):
        docs=self.root/'newdocs';docs.mkdir()
        for n in ('ARTIST_START_HERE.md','EDITOR_GUIDE.md','FIRST_ASSIGNMENT.md','TESTING_STATUS.md','ACCEPTANCE_CHECKLIST.csv','WINDOWS_BUILD.md','WORLD_DESIGNER_GUIDE.md'):
            (docs/n).write_text('fresh '+n)
        launcher=self.root/'launcher.exe';launcher.write_bytes(b'reviewed-launcher-fixture')
        output=self.root/'designer.zip'
        result=bundle.build_bundle([('city',self.validated())],output,docs_root=docs,extra_bootstrap=[('Valwood World Designer.exe',launcher),('ACCEPTANCE_CHECKLIST.csv',docs/'ACCEPTANCE_CHECKLIST.csv')])
        with zipfile.ZipFile(output) as z:
            index=json.loads(z.read('bundle-index.json'));row=next(r for r in index['bootstrap_files'] if r['member']=='Valwood World Designer.exe')
            self.assertEqual(row['sha256'],hashlib.sha256(launcher.read_bytes()).hexdigest())
            self.assertIn('Install_city.bat',z.namelist());self.assertTrue(result['ok'])
            self.assertIn('ACCEPTANCE_CHECKLIST.csv',z.namelist())
            frontdoor=z.read('READ_ME_FIRST.txt').decode()
            self.assertIn('Run Valwood World Designer.exe',frontdoor)
            self.assertIn('automatically verifies',frontdoor)
            self.assertIn('Guide opens WORLD_DESIGNER_GUIDE.md',frontdoor)
            self.assertNotIn('Double-click Install_LABEL.bat',frontdoor)
    def test_world_readme_is_opt_in_and_obsolete_private_assignment_briefs_excluded(self):
        docs=self.root/'guide';docs.mkdir()
        for n in ('ARTIST_START_HERE.md','EDITOR_GUIDE.md','FIRST_ASSIGNMENT.md','TESTING_STATUS.md','ACCEPTANCE_CHECKLIST.csv','WINDOWS_BUILD.md','WORLD_DESIGNER_GUIDE.md'):(docs/n).write_text('fresh '+n)
        old=self.root/'old.md';old.write_text('old character assignment')
        validated=list(self.validated());validated[3]+=[('CHARACTER_ASSETS.md',old),('HANDOFF_PLAN.md',old)]
        legacy,_,_=bundle.plan_package(validated)
        self.assertEqual(next(e for e in legacy if e.name=='README.txt').payload,bundle.README)
        legacy_zip=self.root/'legacy.zip';bundle.build_bundle([('city',validated)],legacy_zip)
        with zipfile.ZipFile(legacy_zip) as z:
            self.assertIn('Double-click Install_LABEL.bat',z.read('READ_ME_FIRST.txt').decode())
            self.assertNotIn('Valwood World Designer.exe',z.read('READ_ME_FIRST.txt').decode())
        fresh,_,_=bundle.plan_package(bundle.override_docs(validated,docs),world_designer=True)
        names={e.name for e in fresh}
        self.assertNotIn('CHARACTER_ASSETS.md',names);self.assertNotIn('HANDOFF_PLAN.md',names)
        self.assertFalse(any('/saves/' in n or '/Downloads/' in n for n in names))
        readme=next(e for e in fresh if e.name=='README.txt').payload.decode()
        self.assertIn('Valwood World Designer.exe',readme);self.assertIn('independent working copy',readme)
    def test_bootstrap_cannot_replace_index_or_runtime_and_rejects_collisions(self):
        path=self.root/'extra.py';path.write_text('pass\n')
        for name in ('../escape.py','bin/runtime.py','bundle-index.json','Install_Artist_Bundle.py'):
            with self.subTest(name=name),self.assertRaises(ValueError):
                bundle.build_bundle([('city',self.validated())],self.root/'bad.zip',extra_bootstrap=[(name,path)])

if __name__=='__main__':unittest.main()
