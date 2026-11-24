#!/usr/bin/env bash
set -euo pipefail
story_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
story_engine=$(cd -- "$story_stage/../.." && pwd)
python3 "$story_stage/check_cinematic_studio_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for story_arg in "$@"; do
 case "$story_arg" in --project|--project=*|--editor|--scenic|--scenic=*) printf '%s\n' 'This launcher tests the authored design project from its opening.' >&2; exit 2;; esac
done
exec 9<>"$story_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save and close that session before playtesting the opening.' >&2; exit 1; }
while IFS= read -r story_variable; do
 case "$story_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_*|*_SEQUENCE|VALWOOD_MODULAR_KNIGHT*) unset "$story_variable";; esac
done < <(compgen -e)
unset VALWOOD_COMBAT_ARENA VALWOOD_SHOWCASE VALWOOD_CINEMATIC_EDITOR VALWOOD_SKELETAL_LOADOUTS
export VALWOOD_PRESENTATION=1 VALWOOD_OPENING_PROLOGUE=1
cd -- "$story_engine"
exec "$story_stage/runtime-editor-native-v1/native-bin/rpg" --project "$story_engine/projects/valwood_design" --play --width 1600 --height 900 "$@"
