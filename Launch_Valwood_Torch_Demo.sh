#!/usr/bin/env bash
set -euo pipefail
valwood_torch_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$valwood_torch_root/new_engine/tools/demo_castle_village.py" --lighting dusk --stop hero "$@"
