#!/usr/bin/env python3
"""Run the existing isolated native editor workflows sequentially.

Build first and close the live game. These are functional checks; loading,
Apply and screenshot stalls are not performance measurements.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CASES = (
    ('placement', 'check_editor_placement.py'),
    ('clipboard', 'check_editor_copy_paste.py'),
    ('terrain-snap', 'check_editor_terrain_snap.py'),
    ('vfx', 'check_editor_vfx.py'),
    ('surfaces', 'validate_surface_editors.py'),
    ('checkpoint', 'check_editor_completion.py'),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    report = dict(started_utc=datetime.now(timezone.utc).isoformat(),
                  binary_sha256=hashlib.sha256((ROOT/'rpg').read_bytes()).hexdigest(),
                  scope='Sequential native functional checks, not a performance benchmark.',
                  cases=[])
    for name, helper in CASES:
        print('Checking '+name, flush=True)
        command = [sys.executable, str(ROOT/'tools'/helper), '--output', str(out/name)]
        with (out/(name+'-runner.log')).open('w') as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        path = out/name/'results.json'
        evidence = json.loads(path.read_text()) if path.is_file() else {}
        passed = result.returncode == 0 and evidence.get('passed') is True
        report['cases'].append(dict(name=name, passed=passed, exit=result.returncode,
                                    evidence=str(path.relative_to(out))))
        report['passed'] = all(case['passed'] for case in report['cases'])
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
        print(name+(': PASS' if passed else ': FAIL — see '+str(out/(name+'-runner.log'))), flush=True)
        if not passed:
            return 1
    report['completed_utc'] = datetime.now(timezone.utc).isoformat()
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
