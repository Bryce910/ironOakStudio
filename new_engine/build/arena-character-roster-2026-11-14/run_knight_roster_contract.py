#!/usr/bin/env python3
"""Validate cosmetic roster parsing and isolated atomic selection persistence."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

N = Path(__file__).resolve().parent
S = N / 'source-v1'
O = N / 'character-review'

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    O.mkdir(exist_ok=True)
    binary = O / 'knight-roster-contract'
    dep = O / 'knight-roster-contract.d'
    command = ['/usr/bin/g++', '-std=c++17', '-O2', '-MMD', '-MF', str(dep),
               '-I', str(S), str(S / 'tests/world_knight_roster_test.cpp'), '-o', str(binary)]
    subprocess.run(command, check=True, cwd=N)
    raw = subprocess.check_output([str(binary)], text=True, cwd=N)
    result = json.loads(raw)
    assert result['passed'] and result['selection_roundtrips'] == 18
    assert result['failed_operations_preserve_prior_state'] and result['purse_bytes_unchanged']
    output = O / 'KNIGHT_ROSTER_CONTRACT_RAW.json'
    output.write_text(raw)
    dependencies = shlex.split(dep.read_text().replace('\\\n', ' ').split(':', 1)[1])
    inputs = {str(Path(path).resolve()): sha(path) for path in dependencies}
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    inputs[str(Path('/usr/bin/g++').resolve())] = sha('/usr/bin/g++')
    receipt = {'schema': 'valwood.arena-knight-roster.contract-review.v1', 'passed': True,
               'inputs': inputs, 'outputs': {str(path): sha(path) for path in (binary, dep, output)},
               'command': command, 'result': result,
               'economic_profile_format_changed': False, 'original_asset_acceptance': False,
               'native_ui_and_controller_acceptance': False,
               'limits': ['Fixture catalog validates parser and cosmetic save behavior; it does not certify original assets or rendered UI.']}
    path = O / 'KNIGHT_ROSTER_CONTRACT_REVIEW.json'
    path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'passed': True, 'receipt': str(path), 'result': result}))

if __name__ == '__main__':
    main()
