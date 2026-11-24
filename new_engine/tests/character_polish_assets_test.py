import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from character_polish_assets import character_asset_record


def fixture(bank):
    bank.mkdir(parents=True, exist_ok=True)
    files = {}
    for kind in ('body', 'arms'):
        for i in range(172): files[f'combat_{kind}_{i:03d}.glb'] = b'fixture-model'
    for name in ('combat_motion_manifest.json','VALIDATION.json','TEXTURE_PROVENANCE.json',
                 'CHARACTER_POLISH_GEOMETRY.json','world_combat_weapon_sockets.inc'):
        files[name] = b'{}'
    files['combat_weapon_sockets.bin'] = struct.pack('<8sI1032f', b'VWMOT172',172,*([0.]*1032))
    for name,raw in files.items(): (bank/name).write_bytes(raw)
    data = dict(schema='valwood.character-polish-assets',version=1,ready=True,pose_count=172,
                asset_count=344,texture_size=2048,hit_seconds=.32,death_seconds=1.8,
                files=[dict(file=n,sha256=hashlib.sha256(v).hexdigest()) for n,v in files.items()])
    (bank/'CHARACTER_POLISH.json').write_text(json.dumps(data))
    return data


class CharacterBank(unittest.TestCase):
    def test_complete_fingerprinted_bank(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);fixture(p);record=character_asset_record(p)
            self.assertEqual(record['model_assets'],344)
            self.assertEqual(len(record['files_sha256']),351)

    def test_missing_changed_or_symlinked_runtime_inputs(self):
        for mode in ('missing','changed','symlink'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);fixture(p);target=p/'combat_body_171.glb'
                if mode=='missing': target.unlink()
                if mode=='changed': target.write_bytes(b'changed')
                if mode=='symlink': target.unlink();target.symlink_to(p/'combat_body_170.glb')
                with self.assertRaises(ValueError):character_asset_record(p)

    def test_contract_duplicate_or_traversal_cannot_pass(self):
        for mode in ('traversal','duplicate','bool-count','missing-source-proof'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);d=fixture(p)
                if mode=='traversal': d['files'][0]['file']='../combat_body_000.glb'
                if mode=='duplicate': d['files'][1]=d['files'][0]
                if mode=='bool-count': d['pose_count']=True
                if mode=='missing-source-proof': d['files']=[r for r in d['files'] if r['file']!='VALIDATION.json']
                (p/'CHARACTER_POLISH.json').write_text(json.dumps(d))
                with self.assertRaises(ValueError):character_asset_record(p)

    def test_matching_hash_does_not_bypass_socket_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);d=fixture(p);f=p/'combat_weapon_sockets.bin';raw=bytearray(f.read_bytes());raw[:8]=b'BADMAGIC';f.write_bytes(raw)
            for row in d['files']:
                if row['file']==f.name:row['sha256']=hashlib.sha256(raw).hexdigest()
            (p/'CHARACTER_POLISH.json').write_text(json.dumps(d))
            with self.assertRaises(ValueError):character_asset_record(p)


if __name__=='__main__':unittest.main()
