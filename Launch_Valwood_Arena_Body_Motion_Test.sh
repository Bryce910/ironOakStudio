#!/usr/bin/env bash
set -euo pipefail
body_motion_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
body_motion_candidate="$body_motion_root/new_engine/build/arena-body-motion-2026-11-14"
python3 "$body_motion_candidate/check_body_motion_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$body_motion_candidate/launch_knight_candidate.sh" "$@"
