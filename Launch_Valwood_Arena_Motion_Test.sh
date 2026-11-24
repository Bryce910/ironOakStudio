#!/usr/bin/env bash
set -euo pipefail
arena_motion_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
arena_motion_candidate="$arena_motion_root/new_engine/build/arena-motion-polish-2026-11-13"
python3 "$arena_motion_candidate/check_arena_motion_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$arena_motion_candidate/launch_knight_candidate.sh" "$@"
