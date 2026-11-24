#!/usr/bin/env bash
set -euo pipefail
arena_opponents_candidate_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
arena_opponents_engine_dir=$(cd -- "$arena_opponents_candidate_dir/../.." && pwd)
arena_opponents_binary="$arena_opponents_candidate_dir/runtime-native-v1/native-bin/rpg"
arena_opponents_project="$arena_opponents_candidate_dir/arena-project"
arena_opponents_asset=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$arena_opponents_candidate_dir/HERO_ASSET.json")
[[ -x "$arena_opponents_binary" && -f "$arena_opponents_project/arena.config" && -f "$arena_opponents_asset" ]] || { printf '%s\n' 'Arena opponent candidate is not built completely.' >&2; exit 1; }
for arena_opponents_arg in "$@"; do
 case "$arena_opponents_arg" in --project|--project=*|--editor) printf '%s\n' 'This candidate always uses its private arena project.' >&2; exit 2;; esac
done
exec 9>"$arena_opponents_engine_dir/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'A Valwood session is already open. Close that session before this private test.' >&2; exit 1; }
while IFS= read -r arena_opponents_variable; do
 case "$arena_opponents_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_VIEW|VALWOOD_CAPTURE_ARENA_START|VALWOOD_NPC_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE_FULL|*_SEQUENCE) unset "$arena_opponents_variable";; esac
done < <(compgen -e)
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$arena_opponents_asset" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$arena_opponents_candidate_dir/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$arena_opponents_candidate_dir/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$arena_opponents_candidate_dir/loadout-rigs/loadouts.manifest"
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$arena_opponents_engine_dir"
exec "$arena_opponents_binary" --project "$arena_opponents_project" --play --scenic city-walk --width 1600 --height 900 "$@"
