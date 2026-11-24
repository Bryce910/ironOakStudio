#!/usr/bin/env bash
set -euo pipefail
road_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
road_engine=$(cd -- "$road_stage/../.." && pwd)
python3 "$road_stage/check_opening_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for road_arg in "$@"; do
  case "$road_arg" in
    --project|--project=*|--editor|--scenic|--scenic=*|--landmark|--landmark=*|--frames|--frames=*|--benchmark|--no-hud)
      printf '%s\n' 'Use this launcher to play the fixed opening demo with its story and guidance.' >&2
      exit 2;;
  esac
done
exec 9<>"$road_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || {
  printf '%s\n' 'Valwood is already running. Save and close that session before starting the demo.' >&2
  exit 1
}
while IFS= read -r road_variable; do
  case "$road_variable" in VALWOOD_*) unset "$road_variable";; esac
done < <(compgen -e)
unset LD_PRELOAD
export VALWOOD_PRESENTATION=1 VALWOOD_OPENING_PROLOGUE=1
cd -- "$road_engine"
exec "$road_stage/runtime-native-v1/native-bin/rpg" --project "$road_stage/demo-project" --play --width 1600 --height 900 "$@"
