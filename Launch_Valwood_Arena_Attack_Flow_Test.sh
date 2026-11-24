#!/usr/bin/env bash
set -euo pipefail
attack_flow_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
attack_flow_candidate="$attack_flow_root/new_engine/build/arena-attack-flow-2026-11-13"
python3 "$attack_flow_candidate/check_attack_flow_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$attack_flow_candidate/launch_knight_candidate.sh" "$@"
