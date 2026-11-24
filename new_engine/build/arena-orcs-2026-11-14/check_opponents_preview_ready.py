#!/usr/bin/env python3
"""Pin the native, independently checked arena test. This is not art acceptance."""
from pathlib import Path
import argparse
import hashlib
import json

STAGE = Path(__file__).resolve().parent
ROOT = STAGE.parents[1].parent
READY = STAGE / 'ARENA_OPPONENTS_PREVIEW_READY.json'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    if args.write:
        inputs = {}

        def pin(path):
            path = Path(path).resolve()
            inputs[str(path)] = sha(path)

        def artifacts(value):
            if isinstance(value, list):
                for row in value:
                    artifacts(row)
            elif isinstance(value, dict):
                for key, expected in value.items():
                    if key.endswith('_sha256') and isinstance(expected, str):
                        name = value.get(key.removesuffix('_sha256'))
                        if isinstance(name, str):
                            require(sha(name) == expected, 'Changed review artifact: ' + name)
                            pin(name)
                    if key == 'images' and isinstance(expected, dict):
                        for name, digest in expected.items():
                            require(sha(name) == digest, 'Changed review image: ' + name)
                            pin(name)
                    artifacts(expected)

        def review(name):
            path = STAGE / name
            value = json.loads(path.read_text())
            require(value.get('passed', value.get('accepted')) is True, 'Review did not pass: ' + name)
            pin(path)
            for field in ('inputs', 'dependencies_sha256', 'before_sha256', 'after_sha256', 'outputs_sha256', 'outputs', 'all_source_and_bank_sha256'):
                for name, expected in value.get(field, {}).items():
                    require(isinstance(expected, str) and sha(name) == expected, 'Stale review input: ' + name)
                    inputs[str(Path(name).resolve())] = expected
            for row in value.get('reports', []):
                if isinstance(row, dict) and 'path' in row and 'sha256' in row:
                    require(sha(row['path']) == row['sha256'], 'Changed contact report: ' + row['path'])
                    pin(row['path'])
            artifacts(value)
            return value

        native_path = STAGE / 'runtime-native-v1/READY.json'
        native = json.loads(native_path.read_text())
        binary = STAGE / 'runtime-native-v1/native-bin/rpg'
        require(native.get('ready') and native['sha256'] == sha(binary), 'Native binary is stale')
        pin(native_path)
        pin(binary)
        for name, expected in native['source_sha256'].items():
            path = STAGE / 'source-v1' / name
            require(sha(path) == expected, 'Native source changed: ' + name)
            pin(path)

        geometry = review('opponent-review/ORC_GEOMETRY_ONLY_REVIEW.json')
        require(geometry['geometry_only'] and not geometry.get('original_textures_present'), 'Unexpected material status; review the full textured release separately')
        review('opponent-bank-review/OPPONENT_BANK_CPU_REVIEW.json')
        review('opponent-bank-review/SOURCE_PROFILE_SEVER_REVIEW.json')
        review('opponent-review/OPPONENT_CONFIG_REVIEW.json')
        review('opponent-review/SPECIES_CYCLE_CPU_REVIEW.json')
        review('orc-contact-review/ORC_CONTACT_REVIEW.json')
        animation = review('opponent-render-review/ORC_ANIMATION_RENDER_REVIEW.json')
        require({row['scenario'] for row in animation['scenarios']} == {'idle', 'walk', 'attack', 'guard', 'head', 'arm', 'leg', 'hands', 'crowd'}, 'Missing actual Orc render scenario')
        human = review('opponent-review/HUMAN_WORLD_SMOKE_ACCEPTED.json')
        orc = review('opponent-review/ORC_NATIVE_GEOMETRY_RENDER_REVIEW.json')
        require(human['binary_sha256'] == native['sha256'] == orc['binary_sha256'], 'Game captures used a different binary')
        require({row['view'] for row in orc['views']} == {'first', 'third'}, 'Missing actual Orc camera check')

        asset = json.loads((STAGE / 'arena_orcs/ORC_ASSETS.json').read_text())
        require(asset['materials_provisional'] and asset['original_textures_present'] is False, 'Preview warning disagrees with materials')
        require((STAGE / 'arena-project/arena_orcs').resolve() == STAGE / 'arena_orcs', 'Arena points to the wrong Orc bank')
        for path in [Path(__file__), STAGE / 'launch_arena_opponents_candidate.sh', ROOT / 'Launch_Valwood_Arena_Opponents_Test.sh', STAGE / 'arena-project/arena.config', STAGE / 'HERO_ASSET.json']:
            pin(path)
        for root in [STAGE / 'shaders', STAGE / 'arena_orcs']:
            for path in root.rglob('*'):
                if path.is_file():
                    pin(path)

        # Keep the accepted rig-fit build available independently.
        baseline = STAGE.parent / 'arena-rig-fit-2026-11-14'
        require(sha(baseline / 'runtime-native-v1/native-bin/rpg') == '95c0492808a87f5813f7311a38e6fdc90544c4542fad156ca77bac33e60bc284', 'Accepted arena binary changed')
        pin(baseline / 'runtime-native-v1/native-bin/rpg')
        result = dict(schema='valwood.arena-opponents.playable-preview-ready.v1', ready=True,
                      native_only=True, original_textures_present=False, materials_provisional=True,
                      visual_quality_accepted=False, hardware_performance_measured=False,
                      controller_feel_accepted=False, inputs=inputs,
                      limitations=['Purchased source geometry and animations; original texture maps still need downloading.',
                                   'CPU contact and scripted rendering plus bounded native game captures; final controller feel needs player testing.'])
        READY.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    require(READY.is_file(), 'Arena opponent preview has not passed its checks')
    ready = json.loads(READY.read_text())
    require(ready.get('ready') and ready.get('materials_provisional'), 'Wrong readiness record')
    for name, expected in ready['inputs'].items():
        require(sha(name) == expected, 'Arena opponent preview changed since testing: ' + name)
    print('Arena test ready: choose Humans or Orcs in Opponents. Orc original textures are pending; preview materials are active.')


if __name__ == '__main__':
    main()
