#!/usr/bin/env python3
"""Small CPU fixtures exercise the production payload writer and reuse guards."""
from pathlib import Path
from types import SimpleNamespace
import copy,json,shutil,sys,tempfile,unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build_opening_demo_payload as payload

class ReuseTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='valwood-payload-reuse-')
        self.root=Path(self.temp.name);self.old=self.root/'old-release';self.source=self.root/'candidate/project'
        self.data={'models/a.glb':b'geometry-payload','ground/a.png':b'texture-payload','audio/a.wav':b'audio-payload',
                   'models/bank.json':b'{"poses":344}','world.studio':b'world scene','shaders/main.fs.glsl':b'void main(){}',
                   'opening/controls.settings':b'switches=4'}
        for folder in [self.old/'project',self.source]:
            folder.mkdir(parents=True)
            for name,data in self.data.items():
                p=folder/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
            (folder/'saves').mkdir()
        (self.old/'project/graphics.settings').write_bytes(payload.GRAPHICS)
        (self.old/'bin').mkdir();(self.old/'bin/rpg.exe').write_bytes(b'old PE runtime stays untouched')
        rows=[dict(path=n,bytes=len(v),sha256=payload.sha(self.old/'project'/n))for n,v in self.data.items()]
        rows.append(dict(path='graphics.settings',bytes=len(payload.GRAPHICS),sha256=payload.sha(self.old/'project/graphics.settings')))
        (self.old/'project-payload-manifest.json').write_text(json.dumps(dict(schema='valwood.opening-demo-project-payload',version=3,project_directory='project',fresh_saves=True,files=rows)))
        self.origin=self.root/'candidate/local-origin.json';self.origin.write_text('{}')
        self.ready=self.root/'READY.json';self.ready.write_text('{}')
        self.exe=self.root/'new.exe';self.exe.write_bytes(b'new PE runtime')
        self.closure=self.root/'CLOSURE.json';self.closure.write_text('{}')
        self.inputs={n:dict(source=str(self.source/n),lexical=str(self.source/n),fingerprint=payload.fingerprint(self.source/n),expected_sha256=payload.sha(self.source/n))for n in self.data}
        self.review=dict(source_reference=dict(candidate_origin_sha256=payload.sha(self.origin),runtime_ready_sha256=payload.sha(self.ready),windows_binary_sha256=payload.sha(self.exe)),files=len(rows),bytes=sum(r['bytes']for r in rows),studio_aliases=1,studio_lod_paths=1,runtime_alias_proof=dict(path=str(self.closure),sha256=payload.sha(self.closure),runtime_paths=['models/a.glb']),origin_bound_bank_fallbacks=[],excluded=[])
        self.before={str(p.relative_to(self.old)):(payload.sha(p),p.stat().st_mtime_ns,p.stat().st_size,p.stat().st_mode)for p in self.old.rglob('*')if p.is_file()}
    def tearDown(self):self.temp.cleanup()
    def config(self,reuse):
        return SimpleNamespace(validate=lambda:None,output=self.root/'new-release',project=self.source,origin=self.origin,runtime_ready=self.ready,windows_executable=self.exe,runtime_closure=self.closure,previous_manifest=self.old/'project-payload-manifest.json',reuse_immutable_from=self.old if reuse else None,audit_dir=self.root/'audit')
    def build(self,reuse):
        with patch.object(payload,'plan',return_value=(self.inputs,copy.deepcopy(self.review))):return payload.build(self.config(reuse))
    def assert_old_content(self):
        after={str(p.relative_to(self.old)):(payload.sha(p),p.stat().st_mtime_ns,p.stat().st_size,p.stat().st_mode)for p in self.old.rglob('*')if p.is_file()}
        self.assertEqual(self.before,after)
    def test_default_writer_keeps_ordinary_copy_behavior(self):
        result=self.build(False);manifest=json.loads(Path(result['manifest']).read_text())
        self.assertNotIn('immutable_reuse',manifest)
        self.assertNotEqual((self.root/'new-release/project/models/a.glb').stat().st_ino,(self.old/'project/models/a.glb').stat().st_ino)
        self.assert_old_content()
    def test_reuse_links_only_assets_and_never_chmods_shared_inode(self):
        old_inodes={(p.stat().st_dev,p.stat().st_ino)for p in self.old.rglob('*')if p.is_file()};real_chmod=payload.os.chmod
        def checked_chmod(path,*args,**kwargs):
            s=Path(path).stat();self.assertNotIn((s.st_dev,s.st_ino),old_inodes);return real_chmod(path,*args,**kwargs)
        with patch.object(payload.os,'chmod',side_effect=checked_chmod):result=self.build(True)
        out=self.root/'new-release/project';manifest=json.loads(Path(result['manifest']).read_text())
        self.assertEqual(manifest['immutable_reuse']['hardlinked_files'],3)
        for name,data in self.data.items():
            self.assertEqual((out/name).read_bytes(),data)
            self.assertEqual((out/name).stat().st_ino==(self.old/'project'/name).stat().st_ino,payload.immutable_reuse_name(name))
        self.assertFalse((self.root/'new-release/bin').exists());self.assertFalse(any((out/'saves').iterdir()))
        self.assert_old_content()
    def test_corrupt_old_asset_rejected_before_link(self):
        (self.old/'project/models/a.glb').write_bytes(b'corrupt payload')
        with self.assertRaises(AssertionError):payload.ImmutableReuse(self.old,self.inputs)
    def test_changed_new_source_with_stale_expected_sha_rejected(self):
        p=self.source/'models/a.glb';p.write_bytes(b'changed source');self.inputs['models/a.glb']['fingerprint']=payload.fingerprint(p)
        with self.assertRaises(AssertionError):payload.ImmutableReuse(self.old,self.inputs)
    def test_old_symlink_rejected(self):
        p=self.old/'project/models/a.glb';p.unlink();p.symlink_to(self.source/'models/a.glb')
        with self.assertRaises(AssertionError):payload.ImmutableReuse(self.old,self.inputs)
    def test_unexpected_old_private_file_rejected(self):
        (self.old/'project/surprise.txt').write_text('unexpected')
        with self.assertRaises(AssertionError):payload.ImmutableReuse(self.old,self.inputs)
    def test_changed_planned_pathset_rejected(self):
        inputs=dict(self.inputs);inputs.pop('audio/a.wav')
        with self.assertRaises(AssertionError):payload.ImmutableReuse(self.old,inputs)
    def test_private_link_attempt_rejected(self):
        reuse=payload.ImmutableReuse(self.old,self.inputs)
        with self.assertRaises(AssertionError):reuse.link('world.studio',self.root/'bad-link')
        self.assertFalse((self.root/'bad-link').exists());self.assert_old_content()
    def test_old_metadata_or_content_change_detected(self):
        reuse=payload.ImmutableReuse(self.old,self.inputs);(self.old/'project/world.studio').write_bytes(b'changed scene')
        with self.assertRaises(AssertionError):reuse.verify_preserved({})
    def test_whitelist_excludes_code_metadata_runtime_and_save_banks(self):
        for name in ['bin/rpg.exe','bin/SDL2.dll','models/bank.json','shaders/main.fs.glsl','blood_assets/blood.bank','city.physics','ground/a.height','particles/fire.vfx','fonts/a.ttf','saves/a.png','models/saves/a.png']:
            self.assertFalse(payload.immutable_reuse_name(name),name)
        for name in ['models/a.glb','models/a.bin','models/nested/a.png','audio/a.ogg','ground/a.png','texture_blocks/a.vbc7','environments/a.hdr']:
            self.assertTrue(payload.immutable_reuse_name(name),name)
        for name in ['/tmp/a.glb','models/../a.glb']:
            with self.assertRaises(AssertionError):payload.immutable_reuse_name(name)

if __name__=='__main__':unittest.main()
