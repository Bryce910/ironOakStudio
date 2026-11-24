#!/usr/bin/env bash
set -euo pipefail
valwood_demo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$valwood_demo_root/new_engine/tools/demo_castle_village.py" "$@"
