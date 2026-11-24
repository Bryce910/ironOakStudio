"""Exercise real isolated copies, return integrity and concurrent-owner conflicts."""
import hashlib
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from world_artist_handoff import create_workspace, export_return, review_return
from world_asset_database import ValidationError, canonical_json
from build_world_package import verify_package


class ArtistHandoffTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='valwood-artist-test-')
        self.root = Path(self.temp.name)
        self.seed = self.root / 'seed with spaces'
        (self.seed / 'project/models').mkdir(parents=True)
        (self.seed / 'bin').mkdir()
        (self.seed / 'tools').mkdir()
        for name in ('world_artist_handoff.py', 'build_world_package.py', 'world_asset_database.py'):
            (self.seed / 'tools' / name).write_bytes((Path(__file__).resolve().parents[1] / 'tools' / name).read_bytes())
            (self.seed / 'tools' / name).chmod(0o644)
        for name, data in {'bin/rpg': b'fixture binary', 'launch.sh': b'fixture launcher',
                           'project/world.studio': b'original scene',
                           'project/models/tree.glb': b'original mesh',
                           'project/graphics.settings': b'owner settings'}.items():
            (self.seed / name).write_bytes(data)
            (self.seed / name).chmod(0o644)
        entries = [dict(path=p.relative_to(self.seed).as_posix(), bytes=p.stat().st_size,
                        sha256=hashlib.sha256(p.read_bytes()).hexdigest(), mode='0644')
                   for p in sorted(self.seed.rglob('*')) if p.is_file()]
        doc = dict(schema='valwood.linux-package', schema_version=1, files=entries,
                   content_id=hashlib.sha256(canonical_json(entries)).hexdigest())
        (self.seed / 'package-manifest.json').write_bytes(canonical_json(doc))
        self.work = self.root / 'artist work'
        create_workspace(self.seed, self.work)

    def tearDown(self):
        self.temp.cleanup()

    def packet(self):
        (self.work / 'project/world.studio').write_text('new street scene')
        imported = self.work / 'project/models/imports/myprop/abc'
        imported.mkdir(parents=True)
        (imported / 'asset.glb').write_bytes(b'new runtime mesh')
        packet = self.root / 'return 01'
        export_return(self.work, packet)
        return packet

    def test_create_export_stage_and_owner_preserved(self):
        packet = self.packet()
        before = {p.relative_to(self.seed): p.read_bytes() for p in self.seed.rglob('*') if p.is_file()}
        review = self.root / 'review copy'
        self.assertTrue(review_return(packet, self.seed, review)['ok'])
        self.assertEqual((review / 'project/world.studio').read_text(), 'new street scene')
        self.assertTrue((review / 'project/models/imports/myprop/abc/asset.glb').exists())
        self.assertEqual(before, {p.relative_to(self.seed): p.read_bytes() for p in self.seed.rglob('*') if p.is_file()})
        self.assertTrue(verify_package(self.seed)['ok'])
        with self.assertRaises(ValidationError):
            export_return(review, self.root / 'empty return')

    def test_same_file_conflict_refuses_stage(self):
        packet = self.packet()
        (self.seed / 'project/world.studio').write_text('owner parallel change')
        output = self.root / 'conflicted copy'
        result = review_return(packet, self.seed, output)
        self.assertFalse(result['ok'])
        self.assertEqual(result['conflicts'], ['world.studio'])
        self.assertFalse(output.exists())

    def test_unrelated_owner_changes_survive_stage(self):
        packet = self.packet()
        (self.seed / 'project/models/tree.glb').write_bytes(b'owner tree change')
        review = self.root / 'review'
        self.assertTrue(review_return(packet, self.seed, review)['ok'])
        self.assertEqual((review / 'project/models/tree.glb').read_bytes(), b'owner tree change')

    def test_personal_preferences_and_unsaved_files_excluded(self):
        project = self.work / 'project'
        for name in ('world.studio.recovery', 'world.vwproject', 'world.studio.workspace', 'studio-layout.ini', 'graphics.settings'):
            (project / name).write_text('private/session data')
        (project / 'saves').mkdir()
        (project / 'saves/private.json').write_text('player save')
        packet = self.packet()
        doc = json.loads((packet / 'changes.json').read_text())
        self.assertEqual({r['path'] for r in doc['changes']}, {'world.studio', 'models/imports/myprop/abc/asset.glb'})

    def test_deletion_is_recorded_and_staged_only_in_copy(self):
        (self.work / 'project/models/tree.glb').unlink()
        packet = self.root / 'deleted return'
        self.assertEqual(export_return(self.work, packet)['deletions'], 1)
        output = self.root / 'deletion review'
        self.assertTrue(review_return(packet, self.seed, output)['ok'])
        self.assertFalse((output / 'project/models/tree.glb').exists())
        self.assertTrue((self.seed / 'project/models/tree.glb').exists())

    def test_city_and_stronghold_saved_files_survive_return_and_review(self):
        names=['city_showcase.json','publisher_showcase.json','stronghold.json','city.scene','city.sky','city.physics',
               'showcase.views','showcase.flames','showcase.flames.json','showcase.residents',
               'ground/showcase.height','ground/city.coverage','ground/countryside.height','ground/countryside.coverage','ground/showcase.roofs']
        for name in names:
            path=self.work/'project'/name;path.parent.mkdir(exist_ok=True)
            path.write_bytes(('saved '+name).encode())
        packet=self.root/'new-environment-return';export_return(self.work,packet)
        changes=json.loads((packet/'changes.json').read_text())
        self.assertEqual({r['path']for r in changes['changes']},set(names))
        review=self.root/'new-environment-review'
        self.assertTrue(review_return(packet,self.seed,review)['ok'])
        for name in names:self.assertEqual((review/'project'/name).read_bytes(),('saved '+name).encode())

    def test_tampered_return_refuses_stage(self):
        packet = self.packet()
        (packet / 'files/world.studio').write_text('unexpected bytes')
        with self.assertRaises(ValidationError):
            review_return(packet, self.seed, self.root / 'bad copy')

    def test_traversal_duplicate_and_extra_files_rejected(self):
        packet = self.packet()
        manifest = packet / 'changes.json'
        original = json.loads(manifest.read_text())
        doc = json.loads(manifest.read_text());doc['changes'][0]['path'] = '../outside'
        manifest.write_bytes(canonical_json(doc))
        with self.assertRaises(ValidationError):review_return(packet, self.seed)
        doc = json.loads(json.dumps(original));doc['changes'].append(doc['changes'][0])
        manifest.write_bytes(canonical_json(doc))
        with self.assertRaises(ValidationError):review_return(packet, self.seed)
        manifest.write_bytes(canonical_json(original));(packet / 'extra.txt').write_text('extra')
        with self.assertRaises(ValidationError):review_return(packet, self.seed)

    def test_symlink_model_and_binary_revision_rejected(self):
        link = self.work / 'project/models/link.glb'
        link.symlink_to(self.root / 'outside')
        with self.assertRaises(ValidationError):export_return(self.work, self.root / 'bad return')
        link.unlink();(self.work / 'bin/rpg').write_bytes(b'different binary')
        with self.assertRaises(ValidationError):export_return(self.work, self.root / 'bad return')

    def test_different_seed_and_existing_destination_rejected(self):
        with self.assertRaises(ValidationError):create_workspace(self.seed, self.work)
        packet = self.packet();doc = json.loads((packet / 'changes.json').read_text())
        doc['seed_content_id'] = 'different seed';(packet / 'changes.json').write_bytes(canonical_json(doc))
        with self.assertRaises(ValidationError):review_return(packet, self.seed)

    def test_direct_packaged_cli_keeps_seed_hash_verified(self):
        output = self.root / 'CLI work'
        result = subprocess.run([sys.executable, str(self.seed/'tools/world_artist_handoff.py'),
            'create', '--package', str(self.seed), '--output', str(output)],
            cwd='/tmp', text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertTrue(verify_package(self.seed)['ok'])
        self.assertFalse((self.seed/'tools/__pycache__').exists())


if __name__ == '__main__':
    unittest.main()
