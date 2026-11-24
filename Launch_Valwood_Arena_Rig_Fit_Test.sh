#!/usr/bin/env bash
set -euo pipefail
rig_fit_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
rig_fit_candidate="$rig_fit_root/new_engine/build/arena-rig-fit-2026-11-14"
python3 "$rig_fit_candidate/check_rig_fit_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$rig_fit_candidate/launch_knight_candidate.sh" "$@"
