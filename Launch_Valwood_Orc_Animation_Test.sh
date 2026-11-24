#!/usr/bin/env bash
set -euo pipefail
orc_motion_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
orc_motion_stage="$orc_motion_root/new_engine/build/arena-orc-motion-polish-2026-11-14"
python3 "$orc_motion_stage/check_gnur_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$orc_motion_stage/launch_arena_opponents_candidate.sh" "$@"
