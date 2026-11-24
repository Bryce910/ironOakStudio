#!/usr/bin/env bash
set -euo pipefail
valwood_engine_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "$valwood_engine_root/projects/valwood_world/project.valwood" ]]; then
    printf '%s\n' 'Prepare the world with: python3 tools/build_valwood_world.py' >&2
    exit 2
fi
mkdir -p "$valwood_engine_root/docs/performance/world-terrain"
exec 9>"$valwood_engine_root/projects/seldonne/.valwood-instance.lock"
if ! flock -n 9; then
    printf '%s\n' 'Close the running Valwood window before benchmarking.' >&2
    exit 2
fi
exec "$valwood_engine_root/rpg" --project "$valwood_engine_root/projects/valwood_world" \
    --play --benchmark --view sweep --width 1600 --height 900 --warmup 120 --frames 3600 \
    --profile "$valwood_engine_root/docs/performance/local-world.json" "$@"
