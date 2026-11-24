#!/usr/bin/env python3
"""Bounded CPU checks for real-contact presentation and motor lifecycle."""
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'source-v1'
OUT = ROOT / 'clash-review'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    OUT.mkdir(exist_ok=True)
    compiler = Path(shutil.which('g++')).resolve()
    production = [SOURCE / f'engine/{name}' for name in [
        'world_combat_feedback.h', 'world_combat_vfx.cpp', 'world_combat_vfx.h',
        'world_training_combat.cpp', 'world_training_combat.h']]
    frozen = {str(path): sha(path) for path in production}
    tests = [
        ('envelopes', SOURCE / 'tests/world_combat_clash_feedback_test.cpp'),
        ('contact-vfx', SOURCE / 'tests/world_combat_contact_feedback_vfx_test.cpp'),
        ('vfx-regression', ROOT.parents[1] / 'tests/world_combat_vfx_test.cpp'),
    ]
    inputs = {**frozen, str(Path(__file__)): sha(__file__), str(compiler): sha(compiler)}
    outputs = {}
    results = {}
    commands = []
    for name, test in tests:
        binary, dep = OUT / f'test-clash-{name}', OUT / f'test-clash-{name}.d'
        command = [str(compiler), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-MMD', '-MF', str(dep), '-I', str(SOURCE), str(test),
                   str(SOURCE / 'engine/world_combat_vfx.cpp'),
                   str(SOURCE / 'engine/world_training_combat.cpp'), '-o', str(binary)]
        subprocess.run(command, cwd=ROOT, check=True)
        raw = subprocess.check_output([str(binary)], cwd=ROOT, text=True)
        log = OUT / f'clash-{name}.log'
        log.write_text(raw)
        results[name] = json.loads(raw) if name == 'envelopes' else {'passed': True, 'stdout': raw.strip()}
        commands.append(command)
        for path in [test, binary, dep, log]:
            (inputs if path == test else outputs)[str(path.resolve())] = sha(path)
        for path in shlex.split(dep.read_text().replace('\\\n', ' ').split(':', 1)[1]):
            inputs[str(Path(path).resolve())] = sha(path)
    assert all(result['passed'] for result in results.values())
    assert frozen == {str(path): sha(path) for path in production}, 'Production changed during review'
    receipt = {
        'schema': 'valwood.blade-clash.feedback-cpu.v1', 'passed': True,
        'inputs': inputs, 'outputs': outputs, 'compile_commands': commands,
        'results': results,
        'coverage': [
            'Distinct bounded steel attack and settling motor stages at30/60/120Hz',
            'Strongest simultaneous and overlapping motor requests retained',
            'Vibration off, pause, reset, deduplication and no resume replay',
            'Real-event-only metal; miss and stale counters cannot create impact',
            'Anchored90ms steel streak and directional sparks expiring before180ms',
            'Existing blood, dust, trails and bounded particle pool retained',
        ],
        'peak_motor_amplitudes_unchanged': True,
        'native_visual_acceptance': False,
        'actual_owned_geometry_acceptance': False,
        'hardware_controller_acceptance': False,
        'audio_hearing_acceptance': False,
    }
    path = OUT / 'CLASH_FEEDBACK_REVIEW.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'passed': True, 'receipt': str(path), 'sha256': sha(path), 'results': results}))


if __name__ == '__main__':
    main()
