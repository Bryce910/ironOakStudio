#!/usr/bin/env bash
set -euo pipefail
demo_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
demo_engine=$(cd -- "$demo_stage/../.." && pwd)
python3 "$demo_stage/check_opening_unlock.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for demo_arg in "$@"; do
    case "$demo_arg" in
        --project|--project=*|--editor|--scenic|--scenic=*|--landmark|--landmark=*|--frames|--frames=*|--benchmark|--no-hud)
            printf '%s\n' 'Use this launcher to play the opening demo with its story and guidance.' >&2
            exit 2 ;;
    esac
done
exec 9<>"$demo_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save and close that session before starting the demo.' >&2; exit 1; }
while IFS= read -r demo_variable; do
    case "$demo_variable" in VALWOOD_*) unset "$demo_variable" ;; esac
done < <(compgen -e)
unset LD_PRELOAD
export VALWOOD_PRESENTATION=1 VALWOOD_OPENING_PROLOGUE=1
cd -- "$demo_engine"
exec "$demo_stage/runtime-native-v1/native-bin/rpg" --project "$demo_stage/../demo-final-pass-2026-11-15/demo-project" --play --width 1600 --height 900 "$@"
