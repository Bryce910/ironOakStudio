#!/usr/bin/env python3
"""Production state-machine tests against swept phase-plane geometry fixtures."""
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
    test = OUT / 'test_clash_core.cpp'
    binary, dep = OUT / 'test-clash-core', OUT / 'test-clash-core.d'
    log = OUT / 'clash-core.log'
    compiler = Path(shutil.which('g++')).resolve()
    sources = [SOURCE / 'engine/world_training_combat.h', SOURCE / 'engine/world_training_combat.cpp', test]
    frozen = {str(path): sha(path) for path in sources}
    command = [str(compiler), '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
               '-MMD', '-MF', str(dep), '-I', str(SOURCE), str(test),
               str(SOURCE / 'engine/world_training_combat.cpp'), '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    result = subprocess.run([str(binary)], cwd=ROOT, text=True, capture_output=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Core fixture failed: {log}\n{result.stderr}')
    raw = json.loads(result.stdout)
    assert raw['passed'] and raw['cases'] >= 80
    assert frozen == {str(path): sha(path) for path in sources}, 'Core changed during compile/test'
    inputs = {**frozen, str(Path(__file__)): sha(__file__), str(compiler): sha(compiler)}
    for path in shlex.split(dep.read_text().replace('\\\n', ' ').split(':', 1)[1]):
        inputs[str(Path(path).resolve())] = sha(path)
    receipt = {
        'schema': 'valwood.blade-clash.continuous-core-cpu.v1', 'passed': True,
        'inputs': inputs, 'outputs': {str(path): sha(path) for path in [binary, dep, log]},
        'compiler_command': command, 'results': raw,
        'coverage': [
            'Four weapon loadouts with early .31 steel/body and late .48 body',
            'Thin .3601 transition crossing and .595 late release in both directions',
            'Complete continuous .26-.60 clear-air query coverage',
            'First sweep_t across multiple targets wins independently of array order',
            'Off-label physical steel accepted and matching-label clear blade cannot guard',
            'Blocked buffered combo cancelled until bounded recovery and fresh input',
            'Incoming physical block/parry latches stopped NPC serial and contact phase',
            'Nine-stamina guard break clang without immediate damage or NPC recoil',
            'Guard-break damage occurs only on a later actual body crossing',
            'At most one incoming hit per serial across Windup-to-Strike carry',
            '30/60/120 Hz partitions, pause and new-match reset without contact replay',
        ],
        'geometric_callback_fixture': True,
        'actual_owned_asset_geometry_acceptance': False,
        'native_visual_acceptance': False,
        'hardware_controller_acceptance': False,
    }
    path = OUT / 'CLASH_CORE_REVIEW.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'passed': True, 'receipt': str(path), 'sha256': sha(path), 'results': raw}))


if __name__ == '__main__':
    main()
