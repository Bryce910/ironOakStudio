#!/usr/bin/env bash
set -euo pipefail
animation_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
animation_stage="$animation_root/new_engine/build/arena-animation-polish-2026-11-15"
python3 "$animation_stage/check_animation_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$animation_stage/launch_animation_polish.sh" "$@"
