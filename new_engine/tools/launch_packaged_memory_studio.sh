#!/usr/bin/env bash
set -euo pipefail
memory_package_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$memory_package_root/tools/npc_memory_editor.py" "$@"
