"""Run assigned immutable contact suites sequentially, never finalize a failure."""
from pathlib import Path
import hashlib, json, os, subprocess, sys, time

P = Path(__file__).resolve().parent
ids = ['orc06', 'azrak', 'costalmed', 'dardan', 'girra', 'kanit', 'magur', 'ursag']
env = os.environ.copy()
env.update(OPENBLAS_NUM_THREADS='1', OMP_NUM_THREADS='2', PYTHONDONTWRITEBYTECODE='1')
rows = []
for identity in ids:
    with (P / ('contact-suite-' + identity + '.log')).open('w') as log:
        result = subprocess.run([sys.executable, str(P / 'run_orc_contact_suite.py'), identity],
                                env=env, stdout=log, stderr=subprocess.STDOUT)
    row = {'id': identity, 'contacts_passed': result.returncode == 0, 'time': time.time()}
    if row['contacts_passed']:
        manifest = P / 'arena_orcs_roster' / identity / 'ORC_ASSETS.json'
        if manifest.exists():
            row['finalized'] = False
            row['error'] = 'Manifest already exists; refusing to replace a frozen producer manifest'
        else:
            with (P / ('finalize-' + identity + '.log')).open('w') as log:
                result = subprocess.run([sys.executable, str(P / 'finalize_orc_roster_assets_v2.py'),
                                         '--character', identity], env=env,
                                        stdout=log, stderr=subprocess.STDOUT)
            row['finalized'] = result.returncode == 0
            if row['finalized']:
                with manifest.open('rb') as stream:
                    row['manifest_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    rows.append(row)
    print(json.dumps(row), flush=True)
    (P / 'REMAINING_ROSTER_CONTACT_STATUS.json').write_text(
        json.dumps({'characters': rows}, indent=2) + '\n')
