#!/usr/bin/env python3
"""Tiny real-inventory/PE-metadata fixtures; no Windows executable or GPU runs."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'tests'))
from world_asset_database_test import fixture
from world_asset_database import canonical_json, ValidationError
from build_world_package import verify_package
import build_windows_package as windows
import build_windows_artist_bundle as bundle
import install_windows_artist_bundle as installer


def pe():
    data = bytearray(512)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\0\0'
    struct.pack_into('<HH', data, 132, 0x8664, 1)
    struct.pack_into('<H', data, 148, 240)
    struct.pack_into('<H', data, 152, 0x20b)
    return bytes(data)


class WindowsArtistBundleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='valwood-artist-bundle-test-')
        self.root = Path(self.temp.name)
        self.engine, self.project = fixture(self.root)
        runtime = self.engine/'build/windows/editor/bin'
        (runtime/'python').mkdir(parents=True)
        for name in ['rpg.exe', 'valwood_project_validate.exe', 'python/python.exe']:
            (runtime/name).write_bytes(pe())
        (runtime/'python/python311.zip').write_bytes(b'tiny embedded standard-library fixture')
        tools = self.engine/'tools'
        tools.mkdir()
        for name in ['build_windows_package.py', 'build_world_package.py', 'world_asset_database.py',
                     'world_artist_handoff.py', 'windows_file_io.py', 'windows_artist_actions.py',
                     'import_world_asset.py']:
            shutil.copyfile(ROOT/'tools'/name, tools/name)
        validator = tools/'valwood_project_validate'
        validator.write_text('#!'+sys.executable+'\nimport json\nprint(json.dumps({"ok":True,"components_validated":0,"capabilities":[]}))\n')
        validator.chmod(0o755)
        self.second = self.engine/'projects/second'
        shutil.copytree(self.project, self.second, symlinks=True)
        (self.second/'project.valwood').write_text('name=Second Fixture\nterrain=valwood_atlas\n')

    def tearDown(self):
        self.temp.cleanup()

    def validated(self, project=None, active=False):
        import argparse
        return windows.validate(argparse.Namespace(project=project or self.project, engine_root=self.engine,
            runtime=None, executable=None, previous_database=None, project_id=None, active_imports_only=active))

    def build(self, projects=None):
        path = self.root/'bundle.zip'
        with contextlib.redirect_stderr(io.StringIO()):
            result = bundle.build_bundle(projects or [('city', self.validated()), ('jungle', self.validated(self.second))], path)
        return path, result

    def unpack(self, path):
        root = self.root/'extracted'
        with zipfile.ZipFile(path) as archive:
            archive.extractall(root)
        return root

    def test_dedup_real_inventory_standalone_install_and_independent_files(self):
        before = {str(p.relative_to(self.engine)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in self.engine.rglob('*') if p.is_file()}
        plans = [('city', self.validated(active=True)), ('jungle', self.validated(self.second, active=True))]
        # Any full staging-copy call here would fail. Payloads must stream.
        with mock.patch('shutil.copyfile', side_effect=AssertionError('Unexpected staging copy')):
            path, result = self.build(plans)
        with zipfile.ZipFile(path) as archive:
            index = json.loads(archive.read('bundle-index.json'))
            self.assertEqual(len(archive.namelist()), len(set(archive.namelist())))
            self.assertIn('bin/python/python.exe', archive.namelist())
            self.assertIn('Install_city.bat', archive.namelist())
            self.assertIn('Install_jungle.bat', archive.namelist())
            for info in archive.infolist():
                self.assertEqual((info.external_attr >> 16) & 0o170000, 0o100000)
            hashes = [row['sha256'] for row in index['contents']]
            self.assertEqual(len(hashes), len(set(hashes)))
            self.assertLess(result['unique_payload_bytes'], sum(p['materialized_bytes'] for p in result['projects']))
            grass = hashlib.sha256((self.project/'ground/dirt.png').read_bytes()).hexdigest()
            self.assertEqual(sum(row['sha256'] == grass for row in index['contents']), 1)
        extracted = self.unpack(path)
        # Move the entire source tree away: installation must be standalone.
        self.engine.rename(self.root/'source-away')
        first = self.root/'new city seed'
        cmd = [sys.executable, '-B', str(extracted/'Install_Artist_Bundle.py'), '--project', 'city', '--output', str(first)]
        run = subprocess.run(cmd, cwd='/', text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout+run.stderr)
        self.assertTrue(verify_package(first)['verified'])
        second = self.root/'new jungle seed'
        installer.install(extracted, 'jungle', second)
        self.assertTrue(verify_package(second)['verified'])
        for seed in [first, second]:
            for p in seed.rglob('*'):
                self.assertFalse(p.is_symlink())
                if p.is_file(): self.assertEqual(p.stat().st_nlink, 1)
            for name in ['Launch_Editor.bat', 'Create_Artist_Workspace.bat', 'Return_Changes.bat',
                         'asset-database.json', 'native-dependencies.json', 'asset-provenance.json', 'build-recipe.json']:
                self.assertTrue((seed/name).is_file(), name)
        (first/'project/ground/dirt.png').write_bytes(b'artist edit')
        self.assertEqual((second/'project/ground/dirt.png').read_bytes(), b'texturefixture')
        self.assertEqual(before, {str(p.relative_to(self.root/'source-away')): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in (self.root/'source-away').rglob('*') if p.is_file()})
        recipe = json.loads((second/'build-recipe.json').read_text())
        self.assertIsInstance(recipe['pruning'], dict)

    def test_existing_destination_output_and_invalid_labels_are_preserved(self):
        path, _ = self.build()
        extracted = self.unpack(path)
        target = self.root/'existing'
        target.mkdir();(target/'keep').write_bytes(b'original')
        with self.assertRaisesRegex(ValueError, 'already exists'):
            installer.install(extracted, 'city', target)
        self.assertEqual((target/'keep').read_bytes(), b'original')
        with self.assertRaisesRegex(ValueError, 'NEW .zip'):
            self.build()
        for labels in [[('CON', self.validated())], [('city', self.validated()), ('City', self.validated())]]:
            with self.assertRaises(ValueError):
                bundle.build_bundle(labels, self.root/'invalid.zip')
        self.assertFalse((self.root/'invalid.zip').exists())

    def test_corrupt_payload_rejects_and_cleans_partial_install(self):
        path, _ = self.build()
        extracted = self.unpack(path)
        index = json.loads((extracted/'bundle-index.json').read_text())
        wanted = hashlib.sha256((self.project/'ground/dirt.png').read_bytes()).hexdigest()
        row = next(r for r in index['contents'] if r['sha256'] == wanted)
        (extracted/row['member']).write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError, 'hash/length'):
            installer.install(extracted, 'city', self.root/'bad-install')
        self.assertFalse((self.root/'bad-install').exists())
        self.assertEqual(list(self.root.glob('.valwood-artist-install-*')), [])

    def test_symlink_payload_rejected_and_no_output(self):
        path, _ = self.build()
        extracted = self.unpack(path)
        index = json.loads((extracted/'bundle-index.json').read_text())
        wanted = hashlib.sha256((self.project/'ground/dirt.png').read_bytes()).hexdigest()
        row = next(r for r in index['contents'] if r['sha256'] == wanted)
        blob = extracted/row['member']; value = blob.read_bytes();blob.unlink()
        external = self.root/'external';external.write_bytes(value);blob.symlink_to(external)
        # Pick a project that uses the selected blob (all first project's
        # content is shared with city or its own package manifest).
        with self.assertRaises(ValueError):
            installer.install(extracted, 'city', self.root/'symlink-install')
        self.assertFalse((self.root/'symlink-install').exists())

    def test_changed_source_cleans_incomplete_zip(self):
        validated = self.validated()
        original = bundle.copy_verified
        changed = False
        def mutate(entry, stream=None):
            nonlocal changed
            if not changed and entry.name == 'project/ground/dirt.png':
                changed = True
                (self.project/'ground/dirt.png').write_bytes(b'changed input')
            return original(entry, stream)
        with mock.patch.object(bundle, 'copy_verified', mutate):
            with self.assertRaisesRegex(ValueError, 'changed'):
                self.build([('city', validated)])
        self.assertFalse((self.root/'bundle.zip').exists())
        self.assertEqual(list(self.root.glob('.valwood-artist-bundle-*')), [])

    def test_unsafe_names_and_disk_preflight(self):
        for name in ['../escape', '/absolute', 'a/../b', 'C:/file', 'project/NUL.txt',
                     'project/trailing.', 'project/a:b', 'a\\b', 'project/COM1',
                     'project/CONIN$', 'project/CONOUT$.txt', 'project/COM¹.txt']:
            with self.assertRaises(ValueError): installer.safe_names([name])
        for names in [['A', 'a'], ['project/file', 'PROJECT/file/child']]:
            with self.assertRaises(ValueError): installer.safe_names(names)
        path, _ = self.build()
        extracted = self.unpack(path)
        usage = shutil.disk_usage(self.root)
        with mock.patch.object(installer.shutil, 'disk_usage', return_value=usage._replace(free=0)):
            with self.assertRaisesRegex(ValueError, 'free disk space'):
                installer.install(extracted, 'city', self.root/'no-space')
        self.assertFalse((self.root/'no-space').exists())

    def test_tampered_manifest_paths_fail_before_materialization(self):
        path, _ = self.build()
        extracted = self.unpack(path)
        original = json.loads((extracted/'bundle-index.json').read_text())
        selected = original['projects'][0]
        manifest_row = next(r for r in original['contents'] if r['sha256'] == selected['manifest_sha256'])
        original_manifest = json.loads((extracted/manifest_row['member']).read_bytes())
        for unsafe in ['../outside', 'project/CON.txt', 'project/a/../outside']:
            index = json.loads(json.dumps(original))
            manifest = json.loads(json.dumps(original_manifest))
            manifest['files'][0]['path'] = unsafe
            manifest['content_id'] = hashlib.sha256(canonical_json(manifest['files'])).hexdigest()
            value = canonical_json(manifest)
            digest = hashlib.sha256(value).hexdigest()
            replacement = dict(sha256=digest, bytes=len(value), member='blobs/'+digest[:2]+'/'+digest)
            blob = extracted/replacement['member'];blob.parent.mkdir(parents=True, exist_ok=True);blob.write_bytes(value)
            index['contents'] = [r for r in index['contents'] if r['sha256'] != manifest_row['sha256']]+[replacement]
            index['projects'][0].update(manifest_sha256=digest, package_content_id=manifest['content_id'])
            index.pop('content_id')
            index['content_id'] = hashlib.sha256(canonical_json(index)).hexdigest()
            (extracted/'bundle-index.json').write_bytes(canonical_json(index))
            with self.assertRaisesRegex(ValueError, 'filename'):
                installer.install(extracted, selected['label'], self.root/'traversal-install')
            self.assertFalse((self.root/'traversal-install').exists())
            self.assertFalse((self.root/'outside').exists())

    def test_label_required_and_project_list_exposes_materialized_sizes(self):
        path, result = self.build()
        extracted = self.unpack(path)
        script = extracted/'Install_Artist_Bundle.py'
        run = subprocess.run([sys.executable, '-B', str(script), '--list'], text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stdout)
        listed = json.loads(run.stdout)
        self.assertEqual([p['label'] for p in listed['projects']], ['city', 'jungle'])
        self.assertEqual([p['materialized_bytes'] for p in listed['projects']], [p['materialized_bytes'] for p in result['projects']])
        run = subprocess.run([sys.executable, '-B', str(script)], text=True, capture_output=True)
        self.assertEqual(run.returncode, 1)
        self.assertIn('--project is required', run.stdout)


if __name__ == '__main__':
    unittest.main()
