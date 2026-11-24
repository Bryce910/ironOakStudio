#!/usr/bin/env python3
"""Check the private guard derivative preserves all unrelated bank references."""
from pathlib import Path
import hashlib
import json
import struct

BASE = Path(__file__).resolve().parent
OLD = BASE.parent / 'modular-knights-2026-11-13/enemy-guards-v5'
NEW = BASE / 'enemy-guards-v6'
BEFORE = OLD / 'bank/models'
AFTER = NEW / 'bank/models'
SLOTS = (77, 78, 79, 89, 90, 91)
ALLOWED = {'combat_anatomy/anatomy.bin', 'longsword_mocap/combat_weapon_sockets.bin', 'hero_grips/arm_grips.bin'}
ALLOWED.update(f'longsword_mocap/combat_body_{slot:03d}.glb' for slot in SLOTS)
ALLOWED.update(f'combat_anatomy/caps/cap_{slot:03d}.glb' for slot in SLOTS)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    old_owned = json.loads((OLD / 'OWNED_DEPENDENCIES.json').read_text())['sha256']
    for name, expected in old_owned.items():
        assert sha(BEFORE / name) == expected, 'Accepted baseline was changed: ' + name
    new_owned = json.loads((NEW / 'OWNED_DEPENDENCIES.json').read_text())['sha256']
    # V2 bank0 source frames precede 217 fixed-size pair records. Only disable
    # the three stale BODY certificates; preserve FP bits and every other byte.
    old_grips = (BEFORE / 'hero_grips/arm_grips.bin').read_bytes()
    new_grips = (AFTER / 'hero_grips/arm_grips.bin').read_bytes()
    assert old_grips[:8] == b'VWAG0002' and struct.unpack_from('<III', old_grips, 16) == (0, 172, 217)
    expected_grips = bytearray(old_grips)
    pair_start = 16 + 12 + 172 * 8 * 3 * 4 * 8
    for slot in (76, 77, 78):
        offset = pair_start + slot * 52
        assert struct.unpack_from('<I', old_grips, offset)[0] == 3
        struct.pack_into('<I', expected_grips, offset, 2)
    assert new_grips == expected_grips, 'Unexpected grip metadata or first-person change'
    checked_links = checked_files = 0
    changed = []
    for old in BEFORE.rglob('*'):
        name = old.relative_to(BEFORE).as_posix()
        new = AFTER / name
        assert new.exists(), 'Derivative is missing: ' + name
        if name in ALLOWED:
            assert name in new_owned and sha(new) == new_owned[name], 'Unpinned guard output: ' + name
            if sha(old) != sha(new):
                changed.append(name)
        elif old.is_symlink():
            assert new.is_symlink() and old.readlink() == new.readlink(), 'Unrelated reference changed: ' + name
            checked_links += 1
        elif old.is_file():
            assert not new.is_symlink() and sha(old) == sha(new), 'Unrelated bytes changed: ' + name
            checked_files += 1
    pins = {str(p): sha(p) for p in (Path(__file__), OLD / 'READY.json', OLD / 'OWNED_DEPENDENCIES.json', NEW / 'BASELINE_CLONE.json', NEW / 'OWNED_DEPENDENCIES.json')}
    pins.update({str(BEFORE / name): expected for name, expected in old_owned.items()})
    for name, expected in new_owned.items():
        assert sha(AFTER / name) == expected
        pins[str(AFTER / name)] = expected
    result = {'schema': 'valwood.guard-bank-isolation.v1', 'accepted': True,
              'unchanged_symlink_references': checked_links, 'unchanged_regular_files': checked_files,
              'changed_bank_files': changed, 'first_person_views_and_sockets_unchanged': True,
              'grip_metadata_only_body_certificates_76_77_78_disabled': True,
              'accepted_v5_owned_files_unchanged': True, 'dependencies_sha256': pins}
    (NEW / 'ISOLATION_REVIEW.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'dependencies_sha256'}))

if __name__ == '__main__':
    main()
