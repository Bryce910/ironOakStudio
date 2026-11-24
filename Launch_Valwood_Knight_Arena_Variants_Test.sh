#!/usr/bin/env bash
set -euo pipefail
knight_variants_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
knight_variants_candidate="$knight_variants_root/new_engine/build/arena-knight-variants-2026-11-13"
python3 "$knight_variants_candidate/check_arena_variants_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then
    exit 0
fi
exec "$knight_variants_candidate/launch_knight_candidate.sh" "$@"
