#!/usr/bin/env bash
set -euo pipefail
arena_opponents_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
arena_opponents_stage="$arena_opponents_root/new_engine/build/arena-orcs-2026-11-14"
python3 "$arena_opponents_stage/check_opponents_preview_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$arena_opponents_stage/launch_arena_opponents_candidate.sh" "$@"
