#!/usr/bin/env bash
set -euo pipefail
valwood_memory_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$valwood_memory_root/new_engine/tools/npc_memory_editor.py" "$@"
