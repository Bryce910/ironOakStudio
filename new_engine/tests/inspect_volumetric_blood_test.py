"""Offline source intake must preserve data without importing Unity code."""
import hashlib
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import inspect_volumetric_blood as tool


class IntakeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package = self.root/'source.unitypackage'

    def tearDown(self):
        self.temp.cleanup()

    def archive(self, sources=(), extra=()):
        with tarfile.open(self.package, 'w:gz') as archive:
            for index, (path, value) in enumerate(sources, 1):
                guid = f'{index:032x}'
                for name, data in (('pathname', path.encode()), ('asset', value),
                                   ('asset.meta', f'guid: {guid}\n'.encode())):
                    member = tarfile.TarInfo(f'{guid}/{name}')
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
            for name, kind, data in extra:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.size = len(data) if kind == tarfile.REGTYPE else 0
                if kind in (tarfile.SYMTYPE, tarfile.LNKTYPE):
                    member.linkname = '/tmp/escape'
                archive.addfile(member, io.BytesIO(data) if member.isfile() else None)

    def test_preserves_mesh_binary_animation_shader_and_metadata_only(self):
        values = [('Assets/Blood/mesh.fbx', b'Kaydara FBX\x00\xff'),
                  ('Assets/Blood/positions.bytes', b'\x00\x80\xff\x01'),
                  ('Assets/Blood/clip.anim', b'AnimationClip: {}'),
                  ('Assets/Blood/mesh.asset', b'\x00binarymesh'),
                  ('Assets/Blood/render.shader', b'Shader "Blood" {}'),
                  ('Assets/Blood/run.cs', b'throw new Exception();'),
                  ('Assets/Blood/native.dll', b'MZ executable'),
                  ('Assets/Blood/Plugins/sneaky.bytes', b'not source'),
                  ('Assets/BloodExtra/other.png', b'not selected')]
        self.archive(values)
        report = tool.inspect(self.package)
        self.assertEqual(report['asset_count'], 9)
        self.assertEqual(report['categories']['excluded-script-or-plugin'], 3)
        bank = self.root/'fresh-bank'
        manifest = tool.extract_sources(self.package, report, bank, ['Assets/Blood/'])
        self.assertEqual(len(manifest['extracted']), 10)
        self.assertEqual(manifest['executed_files'], 0)
        for path, data in values[:5]:
            self.assertEqual((bank/path).read_bytes(), data)
            self.assertTrue((bank/(path+'.meta')).is_file())
            row = next(x for x in report['entries'] if x['path'] == path)
            self.assertEqual(row['source_sha256'], hashlib.sha256(data).hexdigest())
        for path, _ in values[5:]:
            self.assertFalse((bank/path).exists())
        self.assertEqual(json.loads((bank/'extraction-manifest.json').read_text()), manifest)

    def test_refuses_existing_bank_and_requires_prefix(self):
        self.archive([('Assets/Blood/mesh.fbx', b'data')])
        report = tool.inspect(self.package)
        bank = self.root/'bank'
        with self.assertRaisesRegex(ValueError, 'explicit'):
            tool.extract_sources(self.package, report, bank, [])
        bank.mkdir()
        sentinel = bank/'keep.txt'
        sentinel.write_text('preserve')
        with self.assertRaises(FileExistsError):
            tool.extract_sources(self.package, report, bank, ['Assets/Blood'])
        self.assertEqual(sentinel.read_text(), 'preserve')

    def test_source_path_traversal_and_windows_ambiguity(self):
        for path in ('../escape.fbx', '/Assets/escape.fbx', 'Assets/../escape.fbx',
                     'Assets/Blood//mesh.fbx', 'Assets/Blood/./mesh.fbx',
                     'Assets\\Blood\\mesh.fbx', 'Assets/Blood/mesh.fbx:stream'):
            with self.subTest(path=path):
                self.archive([(path, b'data')])
                with self.assertRaises(ValueError):
                    tool.inspect(self.package)

    def test_rejects_archive_traversal_links_and_duplicate_members(self):
        for extra in ([('../escape', tarfile.REGTYPE, b'data')],
                      [('1'*32+'/asset', tarfile.SYMTYPE, b'')],
                      [('1'*32+'/asset', tarfile.LNKTYPE, b'')],
                      [('0'*31+'1/asset', tarfile.REGTYPE, b'duplicate')]):
            with self.subTest(extra=extra):
                self.archive([('Assets/Blood/mesh.fbx', b'data')], extra)
                with self.assertRaises(ValueError):
                    tool.inspect(self.package)

    def test_store_icon_is_bounded_inventory_only(self):
        self.archive([('Assets/Blood/a.bytes', b'data')], [('.icon.png', tarfile.REGTYPE, b'PNG icon')])
        report = tool.inspect(self.package)
        manifest = tool.extract_sources(self.package, report, self.root/'bank', ['Assets/Blood'])
        self.assertEqual(len(manifest['extracted']), 2)
        self.assertFalse((self.root/'bank/.icon.png').exists())

    def test_rejects_duplicate_casefold_source_paths(self):
        self.archive([('Assets/Blood/a.fbx', b'one'), ('Assets/Blood/A.fbx', b'two')])
        with self.assertRaisesRegex(ValueError, 'Duplicate source'):
            tool.inspect(self.package)

    def test_enforces_per_file_and_total_limits(self):
        self.archive([('Assets/Blood/large.bytes', b'x'*1024)])
        with self.assertRaisesRegex(ValueError, 'per-file'):
            tool.inspect(self.package, max_file_bytes=512)
        with self.assertRaisesRegex(ValueError, 'total unpacked'):
            tool.inspect(self.package, max_file_bytes=2048, max_total_bytes=1030)

    def test_changed_archive_refused_without_creating_bank(self):
        self.archive([('Assets/Blood/a.bytes', b'old')])
        report = tool.inspect(self.package)
        self.archive([('Assets/Blood/a.bytes', b'new')])
        bank = self.root/'bank'
        with self.assertRaisesRegex(ValueError, 'does not match'):
            tool.extract_sources(self.package, report, bank, ['Assets/Blood'])
        self.assertFalse(bank.exists())

    def test_refuses_symlink_destination_ancestor(self):
        self.archive([('Assets/Blood/a.bytes', b'data')])
        report = tool.inspect(self.package)
        actual = self.root/'actual'
        actual.mkdir()
        link = self.root/'link'
        link.symlink_to(actual, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            tool.extract_sources(self.package, report, link/'bank', ['Assets/Blood'])
        self.assertFalse((actual/'bank').exists())


if __name__ == '__main__':
    unittest.main()
