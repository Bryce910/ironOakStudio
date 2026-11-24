#!/usr/bin/env python3
"""Small real ZIP fixtures for streaming final artifact verification; no extraction."""
import hashlib
import json
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import warnings
import zipfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from install_windows_artist_bundle import canonical
import verify_windows_artist_bundle as verifier


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


class VerifyArtistBundleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='valwood-zip-verifier-')
        self.root = Path(self.temp.name); self.path = self.root/'artist.zip'
        self.payloads = {}; self.contents = {}; self.bootstrap = []
        self.files = {}
        self.project_id = '12345678-1234-1234-1234-123456789abc'
        self.add_file('project/project.valwood', b'name=Tiny Fixture\n')
        self.add_file('project/ground/soil.png', b'unique-source-ground-image')
        project_files = []
        for i, path in enumerate(('project.valwood','ground/soil.png')):
            row = self.files['project/'+path]
            project_files.append(dict(path=path, sha256=row['sha256'], bytes=row['bytes'], asset_id='asset'+str(i),
                artifact_id='artifact'+str(i), dependency_paths=['ground/soil.png'] if i == 0 else [],
                dependencies=['asset1'] if i == 0 else []))
        self.database = dict(schema='valwood.asset-database', schema_version=1, project_id=self.project_id,
            files=project_files, assets=[dict(id='asset0', artifacts=['artifact0'], dependencies=['asset1']),
                                      dict(id='asset1', artifacts=['artifact1'], dependencies=[])],
            summary=dict(files=2, logical_assets=2, packaged_bytes=sum(row['bytes'] for row in project_files)))
        self.add_file('asset-database.json', canonical(self.database))
        for path in ('bin/rpg.exe','bin/valwood_project_validate.exe','bin/python/python.exe',
                     'bin/python/python312.dll','bin/python/python312.zip','bin/python/python312._pth'):
            self.add_file(path, (b'MZfixture-'+path.encode()))
        native = dict(format='PE32+', architecture='AMD64',
            binaries=[dict(path='bin/rpg.exe',imports=['KERNEL32.dll']),
                      dict(path='bin/python/python.exe',imports=['python312.dll'])])
        self.add_file('native-dependencies.json', canonical(native))
        for path in ('Launch_Editor.bat','Create_Artist_Workspace.bat','Return_Changes.bat'):
            self.add_file(path, b'@echo off\r\n')
        for path, value in list(self.files.items()):
            if path.startswith('bin/python/'):
                self.payloads[path] = self.payloads[self.contents[value['sha256']]['member']]
                self.bootstrap.append(dict(member=path, sha256=value['sha256'], bytes=value['bytes']))
        for path, raw in [('Install_Artist_Bundle.py',b'# installer fixture'),
                          ('READ_ME_FIRST.txt',b'Orientation instructions'), ('Install_city.bat',b'@echo off')]:
            self.payloads[path] = raw
            self.bootstrap.append(dict(member=path,sha256=sha(raw),bytes=len(raw)))

    def tearDown(self):
        self.temp.cleanup()

    def content(self, raw):
        digest = sha(raw); member = 'blobs/'+digest[:2]+'/'+digest
        self.payloads[member] = raw
        self.contents[digest] = dict(member=member, sha256=digest, bytes=len(raw))
        return self.contents[digest]

    def add_file(self, path, raw):
        row = self.content(raw)
        self.files[path] = dict(path=path, sha256=row['sha256'], bytes=len(raw), mode='0644')

    def build(self, *, extra=None, duplicate=None, change_index=None):
        # Rebuild references after each intentional fixture mutation.
        reachable = {row['sha256'] for row in self.files.values()}
        self.contents = {digest:row for digest,row in self.contents.items() if digest in reachable}
        files = sorted(self.files.values(), key=lambda row:row['path'])
        manifest = dict(schema='valwood.windows-package',schema_version=1,project_id=self.project_id,
            files=files,content_id=sha(canonical(files)),total_bytes=sum(row['bytes'] for row in files))
        row = self.content(canonical(manifest))
        index = dict(schema='valwood.windows-artist-bundle', schema_version=1,contents=list(self.contents.values()),
            bootstrap_files=copy.deepcopy(self.bootstrap), projects=[dict(label='city',project_id=self.project_id,
            manifest_sha256=row['sha256'],package_content_id=manifest['content_id'],files=len(files),materialized_bytes=manifest['total_bytes'])])
        if change_index: change_index(index)
        index['content_id'] = sha(canonical(index))
        payloads = {row['member']:self.payloads[row['member']] for row in index['contents']+index['bootstrap_files']}
        payloads['bundle-index.json'] = canonical(index)
        if extra: payloads.update(extra)
        with zipfile.ZipFile(self.path, 'w', compression=zipfile.ZIP_STORED) as archive:
            for path, raw in payloads.items(): archive.writestr(path, raw)
            if duplicate:
                with warnings.catch_warnings():
                    warnings.simplefilter('ignore'); archive.writestr(duplicate, payloads[duplicate])
        self.sidecar()
        return payloads

    def sidecar(self):
        Path(str(self.path)+'.sha256').write_text(sha(self.path.read_bytes())+'  '+self.path.name+'\n')

    def test_valid_once_per_member_and_no_extraction(self):
        payloads = self.build(); calls = {}; original = zipfile.ZipFile.open
        def counted(archive, name, *args, **kwargs):
            key = name.filename if isinstance(name, zipfile.ZipInfo) else name
            calls[key] = calls.get(key,0)+1
            return original(archive,name,*args,**kwargs)
        before = self.path.read_bytes()
        with mock.patch.object(zipfile.ZipFile,'open', counted), \
             mock.patch.object(zipfile.ZipFile,'extract', side_effect=AssertionError('extraction')), \
             mock.patch.object(zipfile.ZipFile,'extractall', side_effect=AssertionError('extraction')):
            report = verifier.verify_bundle(self.path)
        self.assertTrue(report['verified']); self.assertEqual(report['extracted_files'],0)
        self.assertEqual(calls,{name:1 for name in payloads})
        self.assertEqual(report['projects'][0]['project']['dependencies'],1)
        self.assertEqual(before,self.path.read_bytes())

    def test_unexpected_and_duplicate_zip_members_reject(self):
        self.build(extra={'unexpected.bin':b'x'})
        with self.assertRaisesRegex(ValueError,'Unexpected'): verifier.verify_bundle(self.path)
        self.build(duplicate='Install_city.bat')
        with self.assertRaisesRegex(ValueError,'collision'): verifier.verify_bundle(self.path)

    def test_manifest_windows_filename_and_dependency_closure_reject(self):
        self.files['project/CON.txt'] = dict(self.files['project/ground/soil.png'], path='project/CON.txt')
        self.build()
        with self.assertRaisesRegex(ValueError,'filename'): verifier.verify_bundle(self.path)
        del self.files['project/CON.txt']
        self.database['files'][0]['dependency_paths'] = ['missing.png']
        self.add_file('asset-database.json',canonical(self.database)); self.build()
        with self.assertRaisesRegex(ValueError,'dependency'): verifier.verify_bundle(self.path)

    def test_payload_sha_and_zip_crc_detect_independently(self):
        payloads = self.build(); member = self.contents[self.files['project/ground/soil.png']['sha256']]['member']
        payloads[member] = b'X'*len(payloads[member])
        with zipfile.ZipFile(self.path,'w',compression=zipfile.ZIP_STORED) as archive:
            for name, raw in payloads.items(): archive.writestr(name,raw)
        self.sidecar()
        with self.assertRaisesRegex(ValueError,'SHA256'): verifier.verify_bundle(self.path)
        self.build(); raw = self.path.read_bytes(); target = b'unique-source-ground-image'
        offset = raw.index(target); raw = raw[:offset]+b'X'+raw[offset+1:]
        self.path.write_bytes(raw); self.sidecar()
        with self.assertRaises(zipfile.BadZipFile): verifier.verify_bundle(self.path)

    def test_bundle_index_id_and_sidecar_reject(self):
        payloads = self.build(); index = json.loads(payloads['bundle-index.json']); index['content_id'] = '0'*64
        payloads['bundle-index.json'] = canonical(index)
        with zipfile.ZipFile(self.path,'w') as archive:
            for name, raw in payloads.items(): archive.writestr(name,raw)
        self.sidecar()
        with self.assertRaisesRegex(ValueError,'content ID'): verifier.verify_bundle(self.path)
        self.build(); Path(str(self.path)+'.sha256').write_text('0'*64+'  artist.zip\n')
        with self.assertRaisesRegex(ValueError,'sidecar mismatch'): verifier.verify_bundle(self.path)

    def test_missing_python_and_package_byte_mismatch_reject(self):
        self.build(change_index=lambda index:index['bootstrap_files'].__setitem__(slice(None),
            [row for row in index['bootstrap_files'] if not row['member'].endswith('._pth')]))
        with self.assertRaisesRegex(ValueError,'Python runtime'): verifier.verify_bundle(self.path)
        self.build(change_index=lambda index:index['projects'][0].__setitem__('materialized_bytes',1))
        with self.assertRaisesRegex(ValueError,'byte/count'): verifier.verify_bundle(self.path)

    def test_reported_non_system_dll_must_be_present(self):
        self.add_file('native-dependencies.json',canonical(dict(format='PE32+',architecture='AMD64',
            binaries=[dict(path='bin/rpg.exe',imports=['missing-vendor.dll'])])))
        self.build()
        with self.assertRaisesRegex(ValueError,'Unbundled reported DLL'): verifier.verify_bundle(self.path)

    def test_shared_manifest_under_two_labels_is_streamed_once(self):
        self.payloads['Install_jungle.bat'] = b'@echo off jungle'
        self.bootstrap.append(dict(member='Install_jungle.bat',sha256=sha(self.payloads['Install_jungle.bat']),
                                   bytes=len(self.payloads['Install_jungle.bat'])))
        payloads = self.build(change_index=lambda index:index['projects'].append(dict(index['projects'][0],label='jungle')))
        report = verifier.verify_bundle(self.path)
        self.assertEqual(len(report['projects']),2)
        self.assertEqual(report['members'],len(payloads))

    def test_current_builder_real_inventory_two_projects(self):
        # Exercise the actual builder's manifest/database schema, not only hand
        # authored verifier fixtures. Fake PE files are inspected, never run.
        sys.path.insert(0,str(Path(__file__).resolve().parent))
        from windows_artist_bundle_test import WindowsArtistBundleTest, pe
        fixture = WindowsArtistBundleTest()
        fixture.setUp()
        try:
            runtime = fixture.engine/'build/windows/editor/bin/python'
            (runtime/'python311.dll').write_bytes(pe())
            (runtime/'python311._pth').write_bytes(b'python311.zip\n.\n')
            path, _ = fixture.build()
            Path(str(path)+'.sha256').write_text(sha(path.read_bytes())+'  '+path.name+'\n')
            report = verifier.verify_bundle(path)
            self.assertEqual([row['label'] for row in report['projects']],['city','jungle'])
            self.assertTrue(report['verified'])
        finally:
            fixture.tearDown()


if __name__ == '__main__':
    unittest.main()
