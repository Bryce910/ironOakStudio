#!/usr/bin/env bash
set -euo pipefail
orc_roster_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
orc_roster_stage="$orc_roster_root/new_engine/build/arena-orc-roster-2026-11-14"
python3 "$orc_roster_stage/check_orc_roster_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$orc_roster_stage/launch_arena_opponents_candidate.sh" "$@"
