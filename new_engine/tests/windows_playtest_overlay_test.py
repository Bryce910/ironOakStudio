#!/usr/bin/env python3
"""Tiny complete base + overlay installation fixtures; no game or Windows execution."""
import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools')); sys.path.insert(0,str(ROOT/'tests'))
from windows_artist_bundle_test import WindowsArtistBundleTest, pe
from build_world_package import verify_package
from install_windows_artist_bundle import canonical
import install_windows_artist_bundle as original_installer
import install_windows_playtest_bundle as playtest_installer
import build_windows_playtest_overlay as overlay


def sha(raw): return hashlib.sha256(raw).hexdigest()


class WindowsPlaytestOverlayTest(unittest.TestCase):
    def setUp(self):
        self.fixture = WindowsArtistBundleTest(); self.fixture.setUp()
        self.root = self.fixture.root
        self.base, _ = self.fixture.build()
        with zipfile.ZipFile(self.base) as archive:
            self.base_index = json.loads(archive.read('bundle-index.json'))
        self.verification = dict(ok=True,verified=True,bundle_content_id=self.base_index['content_id'],
                                zip_bytes=self.base.stat().st_size,zip_sha256=sha(self.base.read_bytes()))
        self.sources = self.root/'patches'; self.sources.mkdir()
        for name,raw in [('rpg-next.exe',pe()+b'playtest-runtime'),('route.txt',b'VALWOOD_PLAYTEST_ROUTE 1\n'),
                         ('guide.md',b'Walk around this bounded playtest.'),('launch.bat',b'@echo off\r\n'),
                         ('physics.txt',b'CPU collision fixture\n')]:
            (self.sources/name).write_bytes(raw)
        self.spec = dict(schema='valwood.windows-playtest-patches',schema_version=1,projects=[])
        for label in ('city','jungle'):
            self.spec['projects'].append(dict(label=label,base_label=label,patches={
                'bin/rpg.exe':str(self.sources/'rpg-next.exe'),
                'project/playtest.route':str(self.sources/'route.txt'),
                'project/playtest.physics':str(self.sources/'physics.txt'),
                'PLAYTEST_GUIDE.md':str(self.sources/'guide.md'),
                'Launch_Playtest.bat':str(self.sources/'launch.bat')},project_dependencies={
                'playtest.route':['ground/dirt.png'],'playtest.physics':[],
                'project.valwood':['playtest.route','playtest.physics']}))

    def tearDown(self): self.fixture.tearDown()

    def build(self, spec=None):
        path = self.root/'playtest.zip'
        result = overlay.build_overlay(self.base,spec or self.spec,path,self.verification)
        return path,result

    def extracted(self,path):
        folder = self.root/'extracted'
        with zipfile.ZipFile(self.base) as archive: archive.extractall(folder)
        before = {p.relative_to(folder).as_posix():sha(p.read_bytes()) for p in folder.rglob('*') if p.is_file()}
        with zipfile.ZipFile(path) as archive:
            self.assertFalse(set(archive.namelist()) & set(before))
            archive.extractall(folder)
        self.assertEqual(before,{name:sha((folder/name).read_bytes()) for name in before})
        return folder,before

    def test_metadata_only_build_and_standalone_independent_install(self):
        base_before = sha(self.base.read_bytes())
        original_read = zipfile.ZipFile.read; metadata_reads = []
        def read(archive,name,*args,**kwargs):
            metadata_reads.append(name.filename if isinstance(name,zipfile.ZipInfo) else name)
            return original_read(archive,name,*args,**kwargs)
        with mock.patch.object(zipfile.ZipFile,'read',read), \
             mock.patch.object(zipfile.ZipFile,'extractall',side_effect=AssertionError('staging extraction')), \
             mock.patch.object(zipfile.ZipFile,'extract',side_effect=AssertionError('staging extraction')):
            path,result = self.build()
        self.assertTrue(result['ok']); self.assertEqual(result['base_payloads_extracted'],0)
        self.assertGreater(result['reused_base_payloads'],result['new_payloads'])
        self.assertLess(result['new_payload_bytes'],sum(row['materialized_bytes'] for row in result['projects']))
        self.assertEqual(len(metadata_reads),len(set(metadata_reads)))
        self.assertEqual(base_before,sha(self.base.read_bytes()))
        folder,before = self.extracted(path)
        # Prove that the standalone installer can use only the extracted bundle.
        self.fixture.engine.rename(self.root/'source-away')
        target = self.root/'playtest-city'
        run = subprocess.run([sys.executable,'-B',str(folder/'Install_Playtest_Bundle.py'),
            '--project','city','--output',str(target)],cwd='/',capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        self.assertTrue(verify_package(target)['verified'])
        self.assertEqual((target/'bin/rpg.exe').read_bytes(),(self.sources/'rpg-next.exe').read_bytes())
        self.assertEqual((target/'project/playtest.route').read_bytes(),(self.sources/'route.txt').read_bytes())
        database = json.loads((target/'asset-database.json').read_bytes())
        rows = {row['path']:row for row in database['files']}
        self.assertEqual(rows['playtest.route']['dependency_paths'],['ground/dirt.png'])
        self.assertIn(rows['playtest.physics']['asset_id'],rows['project.valwood']['dependencies'])
        original = self.root/'original-city'
        original_installer.install(folder,'city',original)
        self.assertTrue(verify_package(original)['verified'])
        self.assertFalse((original/'project/playtest.route').exists())
        self.assertNotEqual((original/'bin/rpg.exe').read_bytes(),(target/'bin/rpg.exe').read_bytes())
        for seed in (target,original):
            for p in seed.rglob('*'):
                self.assertFalse(p.is_symlink())
                if p.is_file(): self.assertEqual(p.stat().st_nlink,1)
        self.assertEqual(before,{name:sha((folder/name).read_bytes()) for name in before})

    def test_new_default_seed_name_and_existing_destination_preserved(self):
        path,_ = self.build(); folder,_ = self.extracted(path)
        default = self.root/'Valwood_city_Playtest_Seed'
        run = subprocess.run([sys.executable,'-B',str(folder/'Install_Playtest_Bundle.py'),
            '--project','city'],capture_output=True,text=True)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        self.assertTrue((default/'package-manifest.json').is_file())
        (default/'artist-work.txt').write_text('keep')
        with self.assertRaisesRegex(ValueError,'already exists'):
            playtest_installer.install(folder,'city',default)
        self.assertEqual((default/'artist-work.txt').read_text(),'keep')
        with self.assertRaisesRegex(ValueError,'NEW'):
            self.build()

    def test_wrong_base_index_or_corrupt_patch_rejects_without_publication(self):
        path,_ = self.build(); folder,_ = self.extracted(path)
        base = folder/'bundle-index.json'; value = base.read_bytes(); base.write_bytes(value+b' ')
        with self.assertRaisesRegex(ValueError,'orientation bundle'):
            playtest_installer.install(folder,'city',self.root/'wrong-base-seed')
        self.assertFalse((self.root/'wrong-base-seed').exists()); base.write_bytes(value)
        index = json.loads((folder/'playtest-index.json').read_bytes())
        wanted = sha((self.sources/'rpg-next.exe').read_bytes())
        row = next(row for row in index['contents'] if row['sha256'] == wanted)
        (folder/row['member']).write_bytes(b'broken')
        with self.assertRaisesRegex(ValueError,'hash/length'):
            playtest_installer.install(folder,'city',self.root/'corrupt-seed')
        self.assertFalse((self.root/'corrupt-seed').exists())
        self.assertEqual(list(self.root.glob('.valwood-artist-install-*')),[])

    def test_unsafe_patch_and_dependency_paths_fail_before_zip(self):
        for name in ('../outside','project/CON.txt','bin/a:b.exe','project/route/../bad'):
            specification = copy.deepcopy(self.spec)
            specification['projects'][0]['patches'][name] = str(self.sources/'route.txt')
            with self.assertRaises(ValueError): self.build(specification)
            self.assertFalse((self.root/'playtest.zip').exists())
        specification = copy.deepcopy(self.spec)
        specification['projects'][0]['project_dependencies']['playtest.route'] = ['missing.png']
        with self.assertRaisesRegex(ValueError,'Missing project dependency'): self.build(specification)
        specification = copy.deepcopy(self.spec)
        del specification['projects'][0]['project_dependencies']['playtest.physics']
        with self.assertRaisesRegex(ValueError,'explicit dependency'): self.build(specification)

    def test_unverified_base_symlink_source_and_reserved_replacements_reject(self):
        report = dict(self.verification,verified=False)
        with self.assertRaisesRegex(ValueError,'verification'):
            overlay.build_overlay(self.base,self.spec,self.root/'playtest.zip',report)
        specification = copy.deepcopy(self.spec)
        specification['projects'][0]['patches']['asset-database.json'] = str(self.sources/'route.txt')
        with self.assertRaisesRegex(ValueError,'Reserved'): self.build(specification)
        linked = self.sources/'linked'; linked.symlink_to(self.sources/'route.txt')
        specification = copy.deepcopy(self.spec)
        specification['projects'][0]['patches']['project/playtest.route'] = str(linked)
        with self.assertRaisesRegex(ValueError,'symlink'): self.build(specification)

    def test_source_change_during_stream_cleans_overlay(self):
        original = overlay.copy_verified; changed = False
        def change(entry,destination=None):
            nonlocal changed
            if not changed and entry.name == 'bin/rpg.exe':
                changed = True; (self.sources/'rpg-next.exe').write_bytes(pe()+b'changed')
            return original(entry,destination)
        with mock.patch.object(overlay,'copy_verified',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'): self.build()
        self.assertFalse((self.root/'playtest.zip').exists())
        self.assertEqual(list(self.root.glob('.valwood-playtest-overlay-*')),[])

    def test_truncated_patched_windows_binary_fails_before_publication(self):
        (self.sources/'rpg-next.exe').write_bytes(b'MZshort')
        with self.assertRaisesRegex(ValueError,'Truncated patched Windows binary'): self.build()
        self.assertFalse((self.root/'playtest.zip').exists())

    def test_dotdot_output_cannot_bypass_original_bundle_containment(self):
        path,_ = self.build(); folder,_ = self.extracted(path)
        (self.root/'outside').mkdir()
        target = self.root/'outside'/'..'/folder.name/'nested-playtest'
        with self.assertRaisesRegex(ValueError,'outside the extracted bundle'):
            playtest_installer.install(folder,'city',target)
        self.assertFalse((folder/'nested-playtest').exists())
        self.assertEqual(list(folder.glob('.valwood-artist-install-*')),[])


if __name__ == '__main__':
    unittest.main()
