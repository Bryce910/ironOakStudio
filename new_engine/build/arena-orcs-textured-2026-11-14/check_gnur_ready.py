#!/usr/bin/env python3
"""Require the same bank, native executable and evidence that were reviewed."""
from pathlib import Path
import argparse
import hashlib
import json

STAGE = Path(__file__).resolve().parent
ROOT = STAGE.parents[2]
READY = STAGE / 'GNUR_ARENA_READY.json'
REVIEWS = (
    'opponent-review/ORC_ASSET_CONTRACT_REVIEW.json',
    'opponent-bank-review/OPPONENT_BANK_CPU_REVIEW.json',
    'opponent-bank-review/SOURCE_PROFILE_SEVER_REVIEW.json',
    'opponent-review/INHERITED_ARENA_SOURCE_REVIEW.json',
    'orc-contact-review/ORC_CONTACT_REVIEW.json',
    'opponent-render-review/ORC_ANIMATION_RENDER_REVIEW.json',
    'opponent-render-review/GNUR_VISUAL_REVIEW.json',
    'opponent-review/ORC_NATIVE_LOAD_RENDER_REVIEW.json',
)


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
        pinned = {}

        def pin(path, expected=None):
            path = Path(path).resolve()
            actual = sha(path)
            require(expected is None or actual == expected, 'Changed review dependency: ' + str(path))
            pinned[str(path)] = actual

        def artifacts(value):
            if isinstance(value, list):
                for row in value:
                    artifacts(row)
            elif isinstance(value, dict):
                for key, digest in value.items():
                    if key.endswith('_sha256') and isinstance(digest, str):
                        path = value.get(key.removesuffix('_sha256'))
                        if isinstance(path, str):
                            pin(path, digest)
                    if key in ('inputs', 'dependencies_sha256', 'before_sha256', 'after_sha256',
                               'outputs_sha256', 'all_source_and_bank_sha256') and isinstance(digest, dict):
                        for path, expected in digest.items():
                            pin(path, expected)
                    # Texture inspection also uses "images" for pixel
                    # statistics keyed by content ID. Only path→SHA maps
                    # represent screenshot artifacts; texture paths are
                    # independently pinned by the strict asset inputs.
                    if key == 'images' and isinstance(digest, dict) and all(isinstance(v, str) for v in digest.values()):
                        for path, expected in digest.items():
                            pin(path, expected)
                    if key == 'reports' and isinstance(digest, list):
                        for row in digest:
                            if isinstance(row, dict) and 'path' in row and 'sha256' in row:
                                pin(row['path'], row['sha256'])
                    artifacts(digest)

        native_path = STAGE / 'runtime-native-v1/READY.json'
        native = json.loads(native_path.read_text())
        binary = STAGE / 'runtime-native-v1/native-bin/rpg'
        require(native.get('ready'), 'Native build is incomplete')
        pin(native_path)
        pin(binary, native['sha256'])
        for path, expected in native['source_sha256'].items():
            pin(STAGE / 'source-v1' / path, expected)
        reviews = {}
        for name in REVIEWS:
            path = STAGE / name
            review = json.loads(path.read_text())
            require(review.get('passed', review.get('accepted')) is True, 'Review failed: ' + name)
            pin(path)
            artifacts(review)
            reviews[name] = review
        render = reviews['opponent-render-review/ORC_ANIMATION_RENDER_REVIEW.json']
        require({row['scenario'] for row in render['scenarios']} ==
                {'idle', 'walk', 'attack', 'guard', 'head', 'arm', 'leg', 'hands', 'crowd'}, 'Missing motion scenario')
        smoke = reviews['opponent-review/ORC_NATIVE_LOAD_RENDER_REVIEW.json']
        require(smoke['binary_sha256'] == native['sha256'], 'Native capture used a different executable')
        require({row['view'] for row in smoke['views']} == {'first', 'third'}, 'Missing native camera check')
        asset_path = STAGE / 'arena_orcs/ORC_ASSETS.json'
        asset = json.loads(asset_path.read_text())
        require(asset['source_name'] == 'Gnur' and asset['original_textures_present'] and
                not asset['materials_provisional'] and asset['producer_complete'], 'Wrong or incomplete asset bank')
        require((STAGE / 'arena-project/arena_orcs').resolve() == STAGE / 'arena_orcs', 'Wrong project bank')
        pin(asset_path)
        for path in (Path(__file__), STAGE / 'launch_arena_opponents_candidate.sh',
                     ROOT / 'Launch_Valwood_Arena_Gnur_Test.sh', STAGE / 'HERO_ASSET.json',
                     STAGE / 'arena-project/arena.config'):
            pin(path)
        for directory in (STAGE / 'arena_orcs', STAGE / 'shaders'):
            for path in directory.rglob('*'):
                if path.is_file():
                    pin(path)
        # Preserve the previously accepted arena independently of this import.
        baseline = STAGE.parent / 'arena-orcs-2026-11-14/runtime-native-v1/native-bin/rpg'
        pin(baseline, '7bad545156dd57ddf0c35de939a7d3c7115577bb98ce7e41abb606e540060d5e')
        result = dict(schema='valwood.gnur.arena-reviewed.v1', ready=True, source_name='Gnur',
                      original_textures_present=True, materials_provisional=False, native_only=True,
                      physical_controller_test=False, hardware_performance_measured=False,
                      inputs=pinned, limitations=[
                          'This imports Gnur from the purchased Orc collection; other collection characters are not yet imported.',
                          'Scripted animation, collision checks and bounded native first/third-person captures; player feel still needs hands-on testing.'])
        READY.write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    require(READY.is_file(), 'The Gnur arena is still being verified')
    ready = json.loads(READY.read_text())
    require(ready.get('ready') and ready.get('original_textures_present') and
            not ready.get('materials_provisional'), 'Wrong arena readiness record')
    for path, expected in ready['inputs'].items():
        require(sha(path) == expected, 'Arena changed since verification: ' + path)
    print('Gnur arena ready. Choose Humans or Orcs in Opponents before starting.')


if __name__ == '__main__':
    main()
