#!/usr/bin/env bash
set -euo pipefail
valwood_farms_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$valwood_farms_root/new_engine/tools/demo_castle_village.py" --route farms --stop farms "$@"
