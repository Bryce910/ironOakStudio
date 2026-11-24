#!/usr/bin/env python3
"""Launch only the owned Orc roster whose assets, combat and rendering passed."""
from pathlib import Path
import argparse
import hashlib
import json
import re

STAGE = Path(__file__).resolve().parent
ROOT = STAGE.parents[2]
BANKS = STAGE / 'arena_orcs_roster'
READY = STAGE / 'ORC_ROSTER_READY.json'
EXPECTED = {'orc01', 'orc02', 'orc03', 'orc04', 'orc05', 'orc06',
            'azrak', 'costalmed', 'dardan', 'girra', 'gnur', 'kanit', 'magur', 'ursag'}
REVIEWS = [
    'roster-review/ROSTER_CONTRACT_TESTS.json',
    'roster-review/ROSTER_ASSET_REVIEW.json',
    'roster-review/ROSTER_BATTLE_REVIEW.json',
    'roster-review/ROSTER_RENDER_REVIEW.json',
    'roster-review/ROSTER_NATIVE_REVIEW.json',
    'roster-review/ROSTER_SOURCE_REVIEW.json',
    'roster-review/ROSTER_VISUAL_REVIEW.json',
]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def read(path):
    return json.loads(Path(path).read_text())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    if args.write:
        pinned = {}

        def pin(path, expected=None):
            path = Path(path).resolve()
            if str(path) in pinned:
                require(expected is None or pinned[str(path)] == expected,
                        'Conflicting evidence: ' + str(path))
                return
            digest = sha(path)
            require(expected is None or digest == expected, 'Changed evidence: ' + str(path))
            pinned[str(path)] = digest

        # Evidence writers flatten their actual dependencies. Accept both
        # common path/SHA shapes and keep statistics dictionaries separate.
        def evidence(value):
            if isinstance(value, list):
                for child in value:
                    evidence(child)
            if not isinstance(value, dict):
                return
            if isinstance(value.get('path'), str) and isinstance(value.get('sha256'), str):
                pin(value['path'], value['sha256'])
            for key, child in value.items():
                if key in {'inputs', 'inputs_sha256', 'outputs', 'outputs_sha256', 'before_sha256',
                           'after_sha256', 'dependencies_sha256', 'all_source_and_bank_sha256', 'images'}:
                    if isinstance(child, dict) and child and all(
                            isinstance(path, str) and Path(path).is_absolute() and
                            isinstance(digest, str) and re.fullmatch('[0-9a-f]{64}', digest)
                            for path, digest in child.items()):
                        for path, digest in child.items():
                            pin(path, digest)
                if key.endswith('_sha256') and isinstance(child, str):
                    target = value.get(key[:-7])
                    if isinstance(target, str) and Path(target).is_absolute():
                        pin(target, child)
                evidence(child)

        native_path = STAGE / 'runtime-native-v1/READY.json'
        native = read(native_path)
        require(native.get('ready'), 'Native roster build is incomplete')
        pin(native_path)
        pin(STAGE / 'runtime-native-v1/native-bin/rpg', native['sha256'])
        for name, digest in native['source_sha256'].items():
            pin(STAGE / 'source-v1' / name, digest)
        source = read(STAGE / 'intake_roster/ORC_SOURCE_ROSTER.json')
        require(source.get('passed') and {row['id'] for row in source['characters']} == EXPECTED,
                'Owned source roster does not contain the expected fourteen characters')
        pin(STAGE / 'intake_roster/ORC_SOURCE_ROSTER.json')
        evidence(source)
        roster = read(BANKS / 'ORC_ROSTER.json')
        require(roster['schema'] == 'valwood.owned-orc.roster.v1' and roster['default_id'] == 'gnur',
                'Wrong Orc roster contract')
        require(len(roster['entries']) == 14 and {row['id'] for row in roster['entries']} == EXPECTED,
                'All fourteen genuine characters must be present')
        for row in roster['entries']:
            path = (BANKS / row['asset_manifest']).resolve()
            require(path.is_relative_to(BANKS.resolve()), 'Character manifest escapes roster directory')
            pin(path, row['asset_manifest_sha256'])
            manifest = read(path)
            require(manifest.get('producer_complete') and manifest.get('original_textures_present') and
                    not manifest.get('materials_provisional') and manifest['body_pose_count'] == 172 and
                    manifest['cap_pose_count'] == 172, 'Incomplete character: ' + row['id'])
            original = next(char for char in source['characters'] if char['id'] == row['id'])
            require(row['source_prefab_guid'] == original['source_prefab_guid'],
                    'Character is not bound to its actual owned source: ' + row['id'])
        relocation = read(STAGE / 'GNUR_ROSTER_RELOCATION.json')
        require(relocation['passed'], 'Gnur relocation failed')
        pin(STAGE / 'GNUR_ROSTER_RELOCATION.json')
        for path, row in relocation['files'].items():
            pin(path, row['sha256'])
            pin(row['source'], row['sha256'])
        for name in REVIEWS:
            path = STAGE / name
            report = read(path)
            require(report.get('passed') is True, 'Review has not passed: ' + name)
            if name != 'roster-review/ROSTER_CONTRACT_TESTS.json':
                require(set(report.get('reviewed_ids', [])) == EXPECTED, 'Missing character review: ' + name)
            if name == 'roster-review/ROSTER_NATIVE_REVIEW.json':
                binary = str((STAGE / 'runtime-native-v1/native-bin/rpg').resolve())
                require(report.get('inputs', {}).get(binary) == native['sha256'],
                        'Native roster review does not pin this executable')
            pin(path)
            evidence(report)
        for directory in [BANKS, STAGE / 'source-v1', STAGE / 'shaders']:
            for path in sorted(directory.rglob('*')):
                if path.is_file():
                    pin(path)
        for path in [Path(__file__), STAGE / 'launch_arena_opponents_candidate.sh',
                     ROOT / 'Launch_Valwood_Orc_Roster_Test.sh', STAGE / 'HERO_ASSET.json',
                     STAGE / 'arena-project/arena.config', STAGE / 'ROSTER_STAGE_ORIGIN.json']:
            pin(path)
        require((STAGE / 'arena-project/arena_orcs_roster').resolve() == BANKS,
                'Private project points to the wrong roster')
        base = STAGE.parent / 'arena-orc-motion-polish-2026-11-14'
        pin(base / 'runtime-native-v1/native-bin/rpg',
            '452ca527567ba90ea1fea003ee73e949ba20a2cc4173f4d99b7db902f60faab4')
        result = dict(schema='valwood.orc-roster.reviewed.v1', ready=True,
                      character_count=14, character_ids=sorted(EXPECTED), inputs=pinned,
                      original_textures_present=True, materials_provisional=False,
                      native_only=True, simultaneous_distinct_types=False,
                      physical_controller_test=False, hardware_performance_measured=False,
                      limitations=[
                          'Choose one Orc character per match; concurrent mixed character types are not implemented.',
                          'Scripted combat geometry, native load/render and resource lifecycle checks do not certify AAA animation or physical-controller feel.'])
        READY.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    require(READY.is_file(), 'The full Orc roster is still being verified')
    ready = read(READY)
    require(ready.get('ready') and ready.get('character_count') == 14 and
            set(ready['character_ids']) == EXPECTED, 'Incomplete Orc roster readiness record')
    for path, digest in ready['inputs'].items():
        require(sha(path) == digest, 'Orc roster changed since verification: ' + path)
    print('Fourteen Orc characters ready. Choose Orcs, then an Orc character before starting.')


if __name__ == '__main__':
    main()
