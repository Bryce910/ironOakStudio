#!/usr/bin/env bash
set -euo pipefail
valwood_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$valwood_root/new_engine/run_current_valwood.py" editor --area grandstone "$@"
