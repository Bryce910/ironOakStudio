#!/usr/bin/env bash
set -euo pipefail
cinematic_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cinematic_engine=$(cd -- "$cinematic_stage/../.." && pwd)
python3 "$cinematic_stage/check_cinematic_studio_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
cinematic_project="$cinematic_engine/projects/valwood_design"
[[ -f "$cinematic_project/project.valwood" && -f "$cinematic_project/world.studio" ]] || { printf '%s\n' 'The Valwood design project is unavailable.' >&2; exit 1; }
for cinematic_arg in "$@"; do
 case "$cinematic_arg" in --project|--project=*) printf '%s\n' 'This editor opens the Valwood design project.' >&2; exit 2;; esac
done
exec 9<>"$cinematic_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save your current edits and close that session before opening the updated editor.' >&2; exit 1; }
while IFS= read -r cinematic_variable; do
 case "$cinematic_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_*|*_SEQUENCE) unset "$cinematic_variable";; esac
done < <(compgen -e)
unset VALWOOD_PRESENTATION VALWOOD_OPENING_PROLOGUE VALWOOD_COMBAT_ARENA VALWOOD_SHOWCASE
cd -- "$cinematic_engine"
exec "$cinematic_stage/runtime-editor-native-v1/native-bin/rpg" --project "$cinematic_project" --play --editor --scenic seldonne-arrival --width 1600 --height 900 "$@"
