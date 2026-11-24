#!/usr/bin/env bash
set -euo pipefail
valwood_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
valwood_mode="${1:-play}"
if [[ "$#" -gt 0 ]]; then shift; fi
case "$valwood_mode" in play|editor) ;; *) printf '%s\n' 'Usage: benchmark_valwood.sh [play|editor] [extra engine options]' >&2; exit 2;; esac
mkdir -p "$valwood_root/docs/performance" "$valwood_root/projects/seldonne/captures"
if command -v flock >/dev/null 2>&1; then
    exec 9>"$valwood_root/projects/seldonne/.valwood-instance.lock"
    if ! flock -n 9; then
        printf '%s\n' 'Close the running Valwood window before benchmarking; two instances invalidate GPU and frame-time measurements.' >&2
        exit 2
    fi
fi
exec "$valwood_root/rpg" --project "$valwood_root/projects/seldonne" "--$valwood_mode" \
    --no-hud --benchmark --warmup 120 --frames 3600 --width 1600 --height 900 \
    --profile "$valwood_root/docs/performance/local-$valwood_mode.json" "$@"
