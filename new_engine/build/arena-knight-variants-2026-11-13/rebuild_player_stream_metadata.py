#!/usr/bin/env python3
"""Keep legacy FPS data while replacing only the baked NPC union body layout."""
from pathlib import Path
import argparse
import hashlib
import json
import struct

BASE = Path(__file__).resolve().parent
SOURCE_MODELS = BASE.parent / 'modular-knights-combat-2026-11-13/enemy-guards-v6/bank/models'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def words(data, offset, count):
    return struct.unpack_from('<' + 'I' * count, data, offset)

def view_end(data, start):
    meshes, total, vertices, faces = words(data, start, 4)
    assert 0 < meshes <= 128 and total <= 100000 and vertices <= 12000 and faces <= 50000
    at = start + 16 + meshes * 4
    for _ in range(vertices):
        entries = words(data, at + 24, 1)[0]
        assert entries <= 4
        at += 28 + entries * 20
    return at + faces * 12 + 172 * vertices * 36

def rebuild_grips(raw, counts):
    assert raw[:8] == b'VWAG0002' and words(raw, 8, 2) == (2, 3)
    at = 16
    result = bytearray(raw[:at])
    preserved = []
    for bank in range(3):
        start = at
        assert words(raw, at, 3) == (bank, 172, 217)
        pair_start = at + 12 + 172 * 8 * 3 * 4 * 8
        body_start = pair_start + 217 * 52
        body_end = view_end(raw, body_start)
        fp_end = view_end(raw, body_end)
        if bank == 0:
            prefix = bytearray(raw[start:body_start])
            for slot in range(217):
                offset = pair_start - start + slot * 52
                enabled = words(prefix, offset, 1)[0]
                struct.pack_into('<I', prefix, offset, enabled & 2)
            result.extend(prefix)
            result.extend(struct.pack('<4I', len(counts), sum(counts), 0, 0))
            result.extend(struct.pack('<' + 'I' * len(counts), *counts))
            result.extend(raw[body_end:fp_end])
            preserved.append({'bank': 0, 'view': 'first_person', 'sha256': sha(raw[body_end:fp_end])})
        else:
            result.extend(raw[start:fp_end])
            preserved.append({'bank': bank, 'view': 'both', 'sha256': sha(raw[start:fp_end])})
        at = fp_end
    assert at == len(raw)
    # Independently walk the result to verify the preserved view ranges.
    at = 16
    for bank in range(3):
        start = at
        pair_start = start + 12 + 172 * 8 * 3 * 4 * 8
        body_start = pair_start + 217 * 52
        body_end = view_end(result, body_start)
        fp_end = view_end(result, body_end)
        if bank == 0:
            assert all(words(result, pair_start + slot * 52, 1)[0] & 1 == 0 for slot in range(217))
            assert sha(result[body_end:fp_end]) == preserved[0]['sha256']
        else:
            assert sha(result[start:fp_end]) == preserved[bank]['sha256']
        at = fp_end
    assert at == len(result)
    return bytes(result), preserved

def rebuild_locomotion(raw, counts):
    assert raw[:8] == b'VWLOCO01' and words(raw, 8, 4) == (1, 5, 16, 4)
    at = 40  # Header and four source stride lengths.
    first_end = None
    spans = []
    for bank in range(5):
        meshes, total, sparse = words(raw, at, 3)
        end = at + 12 + meshes * 4 + sparse * (12 + 64 * 36)
        assert end <= len(raw)
        spans.append((at, end))
        if bank == 0:
            first_end = end
        at = end
    assert at == len(raw)
    result = bytearray(raw[:40])
    result[:8] = b'VWLOCO02'
    struct.pack_into('<I', result, 8, 2)
    result.extend(struct.pack('<3I', len(counts), sum(counts), 0))
    result.extend(struct.pack('<' + 'I' * len(counts), *counts))
    result.extend(raw[first_end:])
    assert result[-(len(raw)-first_end):] == raw[first_end:]
    return bytes(result), [{'bank': bank, 'sha256': sha(raw[a:z])} for bank, (a, z) in enumerate(spans) if bank]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--models', type=Path, required=True)
    args = parser.parse_args()
    body = args.models / 'longsword_mocap/combat_body_000.glb'
    original_body = body.read_bytes()
    length = words(original_body, 12, 1)[0]
    document = json.loads(original_body[20:20+length])
    primitives = document['meshes'][0]['primitives']
    counts = [document['accessors'][p['attributes']['POSITION']]['count'] for p in primitives]
    assert 1 <= len(counts) <= 32 and sum(counts) <= 100000
    pins = {str(body.absolute()): sha(original_body), str(Path(__file__)): sha(Path(__file__).read_bytes())}
    records = {}
    for relative, rebuild in [('hero_grips/arm_grips.bin', rebuild_grips), ('hero_locomotion/directional_locomotion.bin', rebuild_locomotion)]:
        source = SOURCE_MODELS / relative
        source_bytes = source.read_bytes()
        converted, preserved = rebuild(source_bytes, counts)
        target = args.models / relative
        assert not target.parent.is_symlink(), 'Prepare a private metadata directory before rebuilding'
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink():
            target.unlink()
        target.write_bytes(converted)
        pins[str(source)] = sha(source_bytes)
        pins[str(target.absolute())] = sha(converted)
        records[relative] = {'before_sha256': sha(source_bytes), 'after_sha256': sha(converted), 'preserved_ranges': preserved}
    assert body.read_bytes() == original_body
    result = {'schema': 'valwood.npc-variant.player-stream-correspondence.v1', 'accepted': True,
              'body_counts': counts, 'new_body_vertices': sum(counts), 'first_person_payloads_preserved': True,
              'other_loadout_payloads_preserved': True, 'bank0_baked_body_overrides_disabled': True,
              'records': records, 'dependencies_sha256': pins}
    (BASE / 'PLAYER_STREAM_CORRESPONDENCE.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'accepted': True, 'body_primitives': len(counts), 'body_vertices': sum(counts)}))

if __name__ == '__main__':
    main()
