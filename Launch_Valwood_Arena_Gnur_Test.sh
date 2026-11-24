#!/usr/bin/env bash
set -euo pipefail
gnur_arena_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
gnur_arena_stage="$gnur_arena_root/new_engine/build/arena-orcs-textured-2026-11-14"
python3 "$gnur_arena_stage/check_gnur_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$gnur_arena_stage/launch_arena_opponents_candidate.sh" "$@"
