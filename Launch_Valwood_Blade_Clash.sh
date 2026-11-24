#!/usr/bin/env bash
set -euo pipefail
clash_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
clash_stage="$clash_root/new_engine/build/arena-blade-clash-2026-11-14"
python3 "$clash_stage/check_blade_clash_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$clash_stage/launch_blade_clash.sh" "$@"
