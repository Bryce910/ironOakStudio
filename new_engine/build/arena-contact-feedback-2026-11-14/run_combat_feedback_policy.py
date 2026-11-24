#!/usr/bin/env python3
"""Compile and execute isolated production event/presentation policy checks."""
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'source-v1'
OUT = ROOT / 'feedback-review'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    OUT.mkdir(exist_ok=True)
    binary = OUT / 'combat-feedback-policy'
    dep = OUT / 'combat-feedback-policy.d'
    compiler = Path(shutil.which('g++')).resolve()
    command = [str(compiler), '-std=c++17', '-O2', '-MMD', '-MF', str(dep),
               '-I', str(SOURCE), str(SOURCE / 'tests/world_combat_feedback_test.cpp'),
               str(SOURCE / 'engine/world_training_combat.cpp'), '-o', str(binary)]
    subprocess.run(command, check=True, cwd=ROOT)
    raw = subprocess.check_output([str(binary)], cwd=ROOT, text=True)
    result = json.loads(raw)
    assert result['passed'] and result['whoosh_timing_cases'] == 20
    assert result['whoosh_lead_min_seconds'] >= .15 - result['timing_step_seconds'] - 1e-8
    assert result['whoosh_lead_max_seconds'] <= .15 + result['timing_step_seconds'] + 1e-8
    raw_path = OUT / 'COMBAT_FEEDBACK_POLICY_RAW.json'
    raw_path.write_text(raw)
    # GCC -MF with multiple TUs retains the final TU's dependencies. Include
    # the test and policy explicitly so every actual source input is pinned.
    dependencies = shlex.split(dep.read_text().replace('\\\n', ' ').split(':', 1)[1])
    inputs = {str(Path(p).resolve()): sha(p) for p in dependencies}
    for path in [Path(__file__), compiler, SOURCE / 'tests/world_combat_feedback_test.cpp',
                 SOURCE / 'engine/world_combat_feedback.h']:
        inputs[str(path.resolve())] = sha(path)
    receipt = {
        'schema': 'valwood.combat-feedback.policy-cpu.v1', 'passed': True,
        'inputs': inputs,
        'outputs': {str(p): sha(p) for p in [binary, dep, raw_path]},
        'compiler_command': command, 'result': result,
        'coverage': ['typed miss/steel/body from real training state transitions',
                     'actual production own block and parry events',
                     'simultaneous independent attack/defense notices',
                     'strongest haptic request retained across same-frame events',
                     'serial deduplication, pause, retry and clock reset',
                     'reduced motion and vibration preferences',
                     'local whoosh/miss independent of historical opponent sentinel',
                     '150ms whoosh-to-resolution lead across four weapons/five directions'],
        'controlled_geometry_queries': True,
        'actual_owned_geometry_acceptance': False,
        'native_visual_acceptance': False,
        'hardware_controller_acceptance': False,
    }
    path = OUT / 'COMBAT_FEEDBACK_POLICY_CPU.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'passed': True, 'receipt': str(path), 'result': result}))

if __name__ == '__main__':
    main()
