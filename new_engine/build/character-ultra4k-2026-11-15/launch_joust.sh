#!/usr/bin/env bash
set -euo pipefail
joust_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
joust_engine=$(cd -- "$joust_stage/../.." && pwd)
python3 "$joust_stage/check_ultra4k_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for joust_arg in "$@"; do
    case "$joust_arg" in
        --project|--project=*|--editor|--scenic|--scenic=*|--frames|--frames=*|--benchmark)
            printf '%s\n' 'Use this launcher for the reviewed jousting practice arena.' >&2
            exit 2 ;;
    esac
done
exec 9<>"$joust_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save and close that session before opening the jousting arena.' >&2; exit 1; }
while IFS= read -r joust_variable; do
    case "$joust_variable" in VALWOOD_*) unset "$joust_variable" ;; esac
done < <(compgen -e)
unset LD_PRELOAD
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_JOUST_ARENA=1
cd -- "$joust_engine"
exec "$joust_stage/runtime-native-v1/native-bin/rpg" --project "$joust_stage/joust-project" --play --scenic city-walk --width 1600 --height 900 "$@"
