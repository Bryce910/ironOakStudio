#!/usr/bin/env bash
set -euo pipefail
knight_test_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
python3 - "$knight_test_root/new_engine/build/modular-knights-2026-11-13/ANIMATION_POLISH_READY.json" <<'PY'
import hashlib,json,sys
from pathlib import Path
receipt=Path(sys.argv[1])
if not receipt.is_file():
    sys.exit('The new knight animation test is still being prepared.')
state=json.loads(receipt.read_text())
if state.get('accepted') is not True:
    sys.exit('The new knight animation test has not passed its checks yet.')
if Path(state['project_models']).resolve()!=Path(state['verified_model_bank']).resolve():
    sys.exit('The new knight test no longer uses its verified contact model bank.')
for key in ('binary','asset'):
    path=Path(state[key])
    if not path.is_file():
        sys.exit('The new knight test is missing a required file.')
    with path.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=state[key+'_sha256']:
            sys.exit('The new knight test changed after verification; rebuild its checks first.')
for filename, expected in state.get('dependencies_sha256',{}).items():
    path=Path(filename)
    if not path.is_file():
        sys.exit('The new knight test is missing a verified dependency.')
    with path.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=expected:
            sys.exit('The new knight contact or animation files changed after verification.')
PY
if [[ $# == 1 && "$1" == --check-only ]]; then
    printf '%s\n' 'New knight animation test preflight passed.'
    exit 0
fi
exec "$knight_test_root/new_engine/build/modular-knights-2026-11-13/launch_knight_candidate.sh" "$@"
