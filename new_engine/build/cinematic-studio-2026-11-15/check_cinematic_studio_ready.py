#!/usr/bin/env python3
"""Verify the reviewed Linux cinematic editor without freezing designer content."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

N = Path(__file__).resolve().parent
ROOT = N.parents[2]
READY = N / 'CINEMATIC_STUDIO_READY.json'
REVIEWS = [
    'review/cutscenes-core/CUTSCENES_CORE_CPU_REVIEW.json',
    'review/cutscenes-host/CUTSCENES_HOST_CPU_REVIEW.json',
    'review/cinematic-editor-merged/CINEMATIC_EDITOR_CPU_REVIEW.json',
    'review/cinematic-editor-merged/CINEMATIC_EDITOR_RETENTION_REVIEW.json',
    'review/editor-forward-port/EDITOR17_FORWARD_PORT_CPU_REVIEW.json',
    'review/editor-forward-port/EDITOR17_FEATURE_RETENTION_REVIEW.json',
    'review/equipment-promotion/EQUIPMENT_PROMOTION_REVIEW.json',
    'equipment-prototype/review/EQUIPMENT_PROMOTION_INDEPENDENT_AUDIT.json',
    'review/VOICE_IMPORT_CPU_REVIEW.json',
    'review/incoming-timing/INCOMING_ATTACK_TIMING_CPU_REVIEW.json',
    'review/CLOSE_COMBAT_VISIBILITY_CPU_REVIEW.json',
    'review/native-game/CUTSCENE_NATIVE_GAME_REVIEW.json',
    'review/native-editor/CINEMATIC_EDITOR_NATIVE_REVIEW.json',
    'review/native-editor/AUTHORED_DEMO_NATIVE_STARTUP_REVIEW.json',
    'review/CINEMATIC_FINAL_CLOSURE_REVIEW.json',
]


def sha(p):
    with Path(p).open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def stat(p):
    s = Path(p).stat()
    return [s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns]


def read(p):
    return json.loads(Path(p).read_text())


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--arena', action='store_true')
    args = parser.parse_args()
    if args.write:
        inputs, frozen_stats = {}, {}

        def pin(path, expected=None):
            path = Path(path).resolve()
            before = stat(path)
            digest = sha(path)
            require(expected is None or digest == expected, 'Reviewed file changed: ' + str(path))
            require(stat(path) == before, 'File changed during publication: ' + str(path))
            inputs[str(path)] = digest
            frozen_stats[str(path)] = before

        native = read(N / 'runtime-editor-native-v1/READY.json')
        require(native.get('ready') and native.get('native_only') and not native.get('Windows_work_performed'), 'Native build is incomplete')
        require(native.get('application_units') == 103 and native.get('full_application_dependency_closure_verified'), 'Native dependency closure is incomplete')
        pin(native['binary'], native['sha256'])
        pin(N / 'runtime-editor-native-v1/READY.json')
        pin(N / 'runtime-editor-native-v1/BUILD_PLAN.json')
        for name, digest in native['source_sha256'].items():
            pin(N / 'source-editor-v1' / name, digest)
        for name in REVIEWS:
            report = read(N / name)
            require(report.get('passed') is True, 'Review incomplete: ' + name)
            pin(N / name)
        closure = read(N / 'review/CINEMATIC_FINAL_CLOSURE_REVIEW.json')
        require(closure.get('native_sha256') == native['sha256'], 'Final review refers to a different native build')
        # The final closure explicitly distinguishes immutable runtime inputs
        # from canonical project files inspected only at publication time.
        for path, digest in closure['immutable_inputs'].items():
            pin(path, digest)
        external = closure.get('external_evidence_at_publication', {})
        for path, digest in external.items():
            require(sha(path) == digest, 'Publication evidence changed: ' + path)
        for path in [
            Path(__file__), N / 'build_editor_candidate.py',
            N / 'launch_editor.sh', N / 'launch_combat_polish.sh',
            N / 'launch_authored_demo.sh', N / 'CINEMATIC_STUDIO_GUIDE.md',
            N / 'COMBAT_POLISH_NOTES.md',
            ROOT / 'Launch_Valwood_Editor.sh',
            ROOT / 'Launch_Valwood_Cinematic_Editor.sh',
            ROOT / 'Launch_Valwood_Combat_Polish_Test.sh',
            ROOT / 'Launch_Valwood_Authored_Demo.sh',
            ROOT / 'Import_Valwood_Cutscene_Voice.sh',
        ]:
            pin(path)
        for path, snapshot in frozen_stats.items():
            require(stat(path) == snapshot, 'File changed before publication: ' + path)
        # Refresh the inherited arena's verification metadata once, without
        # rewriting its historical receipt. Hard-link/copy bookkeeping can
        # change ctime while preserving bytes; rehashing that entire historical
        # source/asset closure on every arena launch would cause long pauses.
        parent = N.parent / 'arena-animation-polish-2026-11-15'
        parent_path = parent / 'ANIMATION_READY.json'
        parent_ready = read(parent_path)
        arena_stats = {path: stat(path) for path in parent_ready['inputs']}
        parent_digest = sha(parent_path)
        subprocess.run([sys.executable, str(parent / 'check_animation_ready.py'), '--check-only'], check=True)
        require(sha(parent_path) == parent_digest, 'Arena receipt changed during validation')
        require(all(stat(path) == snapshot for path, snapshot in arena_stats.items()), 'Arena dependency changed during validation')
        pin(parent_path, parent_digest)
        report = {
            'schema': 'valwood.cinematic-studio.ready.v1', 'ready': True,
            'native_only': True, 'Windows_work_performed': False,
            'native_sha256': native['sha256'], 'inputs': inputs,
            'frozen_stats': frozen_stats,
            'arena_inputs': parent_ready['inputs'],
            'arena_frozen_stats': arena_stats,
            'external_evidence_at_publication': external,
            'designer_project_and_saves_mutable': True,
            'hardware_frame_rate_verified': False,
            'final_user_narration_supplied': False,
        }
        temporary = READY.with_suffix('.tmp')
        temporary.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
        temporary.replace(READY)
    require(READY.is_file(), 'The cinematic editor is still being reviewed')
    ready = read(READY)
    require(ready.get('ready'), 'The cinematic editor is not ready')
    for path, expected in ready['inputs'].items():
        current = stat(path)
        if current != ready['frozen_stats'][path] or current[2] < 1048576:
            require(sha(path) == expected and stat(path) == current, 'Reviewed file changed: ' + path)
    if args.arena:
        for path, expected in ready['arena_inputs'].items():
            current = stat(path)
            if current != ready['arena_frozen_stats'][path] or current[2] < 1048576:
                require(sha(path) == expected and stat(path) == current, 'Reviewed arena asset changed: ' + path)
    print('Valwood cinematic editor ready: camera shots, area/NPC triggers, MP3 import, preview and Publish.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
