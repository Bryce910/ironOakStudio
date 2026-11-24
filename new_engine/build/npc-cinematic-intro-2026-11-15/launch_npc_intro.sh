#!/usr/bin/env bash
set -euo pipefail
intro_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
intro_engine=$(dirname -- "$(dirname -- "$intro_stage")")
python3 "$intro_stage/check_npc_intro_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for intro_arg in "$@"; do
    case "$intro_arg" in
        --project|--project=*|--editor|--scenic|--scenic=*|--landmark|--landmark=*|--no-hud|--benchmark|--frames|--frames=*)
            printf '%s\n' 'This launcher preserves the opening story, cinematic trigger, and guidance UI.' >&2
            exit 2 ;;
    esac
done
if command -v flock >/dev/null 2>&1; then
    exec 9>"$intro_engine/projects/seldonne/.valwood-instance.lock"
    if ! flock -n 9; then
        printf '%s\n' 'Valwood is already running. Close that session before starting this demo.' >&2
        exit 1
    fi
fi
while IFS= read -r intro_variable; do
    case "$intro_variable" in
        VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_SHOWCASE|VALWOOD_VILLAGE_LIGHTING|VALWOOD_CAPTURE_VIEW|VALWOOD_NPC_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE_FULL|*_SEQUENCE)
            unset "$intro_variable" ;;
    esac
done < <(compgen -e)
unset VALWOOD_COMBAT_ARENA VALWOOD_MODULAR_KNIGHT_ASSET VALWOOD_MODULAR_KNIGHT_THIRD_PERSON
export VALWOOD_PRESENTATION=1 VALWOOD_OPENING_PROLOGUE=1
cd -- "$intro_engine"
exec "$intro_stage/runtime-native-v1/native-bin/rpg" --project "$intro_stage/story-project" --play --width 1600 --height 900 "$@"
