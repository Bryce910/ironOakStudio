"""Validate the immutable, offline-cooked fighter bank before a local launch."""
import hashlib
import json
import math
from pathlib import Path
import struct


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def character_asset_record(bank):
    bank = Path(bank).resolve(strict=True)
    marker = bank/'CHARACTER_POLISH.json'
    if marker.is_symlink() or not marker.is_file() or not 8 <= marker.stat().st_size <= 1024*1024:
        raise ValueError('Missing bounded character bank marker')
    data = json.loads(marker.read_text())
    expected = dict(schema='valwood.character-polish-assets', version=1, ready=True,
                    pose_count=172, asset_count=344, texture_size=2048,
                    hit_seconds=.32, death_seconds=1.8)
    if any(data.get(key) != value or type(data.get(key)) is not type(value) for key,value in expected.items()):
        raise ValueError('Unsupported character bank contract')
    rows = data.get('files')
    if not isinstance(rows, list) or not 350 <= len(rows) <= 380:
        raise ValueError('Incomplete character bank file list')
    files = {}; total = 0
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError('Invalid character asset entry')
        name = row.get('file'); digest = row.get('sha256')
        if not isinstance(name, str) or Path(name).name != name or name in files or name == marker.name:
            raise ValueError('Unsafe or duplicate character asset name')
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Invalid character asset fingerprint')
        path = bank/name
        if path.is_symlink() or not path.is_file() or not 1 <= path.stat().st_size <= 64*1024*1024 or sha(path) != digest:
            raise ValueError('Character asset changed: '+name)
        files[name] = digest; total += path.stat().st_size
    models = {f'combat_{kind}_{i:03d}.glb' for kind in ('body','arms') for i in range(172)}
    if {name for name in files if name.endswith('.glb')} != models or total > 8*1024**3:
        raise ValueError('Character model bank exceeds or misses its contract')
    required = {'combat_weapon_sockets.bin', 'combat_motion_manifest.json', 'VALIDATION.json',
                'TEXTURE_PROVENANCE.json', 'CHARACTER_POLISH_GEOMETRY.json', 'world_combat_weapon_sockets.inc'}
    if not required.issubset(files):
        raise ValueError('Character bank validation or sockets missing')
    sockets = (bank/'combat_weapon_sockets.bin').read_bytes()
    if len(sockets) != 12+172*24 or struct.unpack_from('<8sI', sockets) != (b'VWMOT172',172):
        raise ValueError('Invalid character socket contract')
    if any(not math.isfinite(v) or abs(v) >= 5 for v in struct.unpack_from('<1032f', sockets, 12)):
        raise ValueError('Character socket positions exceed the model bounds')
    files[marker.name] = sha(marker)
    return dict(bank=str(bank), files_sha256=files, bytes=total+marker.stat().st_size,
                poses=172, model_assets=344, texture_size=2048)
