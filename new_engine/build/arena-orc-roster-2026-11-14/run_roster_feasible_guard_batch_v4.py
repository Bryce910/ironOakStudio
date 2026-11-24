"""Sequential actual source cooks; planning targets are not acceptance evidence."""
from pathlib import Path
import subprocess, os, json, time

P = Path(__file__).resolve().parent
ids = ['orc03', 'orc02', 'orc06', 'dardan', 'azrak', 'girra', 'magur']
rows = []
for ident in ids:
    env = os.environ.copy()
    env.update(VALWOOD_ORC_CHARACTER=ident, OPENBLAS_NUM_THREADS='1', OMP_NUM_THREADS='2')
    with (P / (ident + '-body-v4.log')).open('w') as log:
        result = subprocess.run(
            ['/usr/bin/blender', '-b', '-t', '2', '--python-exit-code', '1',
             '--python', str(P / 'cook_orc_roster_body_v4.py')],
            env=env, stdout=log, stderr=subprocess.STDOUT)
    passed = result.returncode == 0
    if passed:
        with (P / (ident + '-retarget-v4-review.log')).open('w') as log:
            result = subprocess.run(
                ['python3', str(P / 'check_roster_retarget.py'), ident],
                env=env, stdout=log, stderr=subprocess.STDOUT)
            passed = result.returncode == 0
    row = {'id': ident, 'passed': passed, 'time': time.time()}
    rows.append(row)
    print(json.dumps(row), flush=True)
    (P / 'ROSTER_FEASIBLE_GUARD_BATCH_V4_STATUS.json').write_text(
        json.dumps({'characters': rows}, indent=2) + '\n')
