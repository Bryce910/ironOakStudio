#!/usr/bin/env python3
"""Keep the feedback arena tied to its reviewed runtime and inherited roster."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys

STAGE = Path(__file__).resolve().parent
ROOT = STAGE.parents[2]
BASE = STAGE.parent / 'arena-orc-roster-2026-11-14'
READY = STAGE / 'COMBAT_FEEDBACK_READY.json'
REVIEWS = [
    'feedback-review/COMBAT_OUTCOME_CPU.json',
    'feedback-review/HERO_STEEL_REBOUND_CPU.json',
    'feedback-review/COMBAT_FEEDBACK_POLICY_CPU.json',
    'feedback-review/COMBAT_FEEDBACK_NATIVE.json',
    'vfx-review/VFX_REVIEW.json',
    'feedback-review/SOURCE_REVIEW.json',
    'feedback-review/VISUAL_REVIEW.json',
]


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    if args.write:
        inputs = {}

        def pin(path, expected=None):
            path = Path(path).resolve()
            key = str(path)
            digest = inputs.get(key)
            if digest is None:
                digest = sha(path)
                inputs[key] = digest
            require(expected is None or expected == digest, 'Changed evidence: ' + key)

        def evidence(value):
            if isinstance(value, list):
                for child in value:
                    evidence(child)
            if not isinstance(value, dict):
                return
            if value and all(isinstance(path, str) and Path(path).is_absolute() and
                             isinstance(digest, str) and re.fullmatch('[0-9a-f]{64}', digest)
                             for path, digest in value.items()):
                for path, digest in value.items():
                    pin(path, digest)
            if isinstance(value.get('path'), str) and isinstance(value.get('sha256'), str):
                pin(value['path'], value['sha256'])
            for key, child in value.items():
                if key.endswith('_sha256') and isinstance(child, str):
                    path = value.get(key[:-7])
                    if isinstance(path, str) and Path(path).is_absolute():
                        pin(path, child)
                evidence(child)

        baseline = read(BASE / 'ORC_ROSTER_READY.json')
        require(baseline.get('ready') and baseline.get('character_count') == 14,
                'The inherited fourteen-character roster is not ready')
        pin(BASE / 'ORC_ROSTER_READY.json')
        for path, digest in baseline['inputs'].items():
            pin(path, digest)
        build = read(STAGE / 'runtime-native-v1/READY.json')
        require(build.get('ready') and build.get('native_only') and
                not build.get('Windows_work_performed') and build['application_units'] == 103,
                'The complete native feedback runtime has not built')
        binary = (STAGE / 'runtime-native-v1/native-bin/rpg').resolve()
        pin(STAGE / 'runtime-native-v1/READY.json')
        pin(binary, build['sha256'])
        for path, digest in build['source_sha256'].items():
            pin(STAGE / 'source-v1' / path, digest)
        for name in REVIEWS:
            path = STAGE / name
            report = read(path)
            require(report.get('passed') is True, 'Review has not passed: ' + name)
            if name == 'feedback-review/COMBAT_FEEDBACK_NATIVE.json':
                require(report.get('inputs', {}).get(str(binary)) == build['sha256'],
                        'The native feedback review must pin this executable')
            pin(path)
            evidence(report)
        require((STAGE / 'arena-project/arena_orcs_roster').resolve() ==
                (BASE / 'arena_orcs_roster').resolve(), 'The inherited Orc roster link changed')
        require((STAGE / 'arena-project/models').resolve() ==
                (BASE / 'arena-project/models').resolve(), 'The inherited Human model bank changed')
        for path in [Path(__file__), STAGE / 'launch_feedback_candidate.sh',
                     ROOT / 'Launch_Valwood_Combat_Feedback_Test.sh',
                     STAGE / 'SOURCE_ORIGIN.json', STAGE / 'STAGE_ORIGIN.json',
                     STAGE / 'HERO_ASSET.json', STAGE / 'arena-project/arena.config']:
            pin(path)
        for path in (STAGE / 'shaders').rglob('*'):
            if path.is_file():
                pin(path)
        for name in ('slash', 'impact', 'parry', 'dash'):
            pin(STAGE / 'arena-project/audio/presentation' / (name + '.wav'))
        result = dict(schema='valwood.combat-feedback.ready.v1', ready=True,
                      native_only=True, orc_characters=14, inputs=inputs,
                      physical_controller_feel_verified=False,
                      sustained_hardware_fps_verified=False)
        # Validate the complete unique closure again after collecting all
        # receipts, before publishing a usable launcher gate.
        for path, digest in inputs.items():
            require(sha(path) == digest, 'Changed during readiness review: ' + path)
        temporary = READY.with_suffix('.tmp')
        temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
        temporary.replace(READY)
    require(READY.is_file(), 'The combat feedback arena is still being verified')
    result = read(READY)
    require(result.get('ready') and result.get('orc_characters') == 14,
            'The combat feedback arena is incomplete')
    for path, digest in result['inputs'].items():
        require(sha(path) == digest, 'Changed since feedback verification: ' + path)
    print('Combat feedback arena ready: distinct misses, steel blocks, parries and body hits.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
