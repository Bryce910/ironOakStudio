#!/usr/bin/env bash
set -euo pipefail
knight_candidate_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
knight_engine_dir=$(cd -- "$knight_candidate_dir/../.." && pwd)
knight_binary="$knight_candidate_dir/runtime-native-v1/native-bin/rpg"
knight_project="$knight_candidate_dir/arena-project"
knight_asset=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$knight_candidate_dir/HERO_ASSET.json")
[[ -x "$knight_binary" && -f "$knight_project/arena.config" && -f "$knight_asset" ]] || { printf '%s\n' 'Private knight candidate is not built completely.' >&2; exit 1; }
for knight_arg in "$@"; do
 case "$knight_arg" in --project|--project=*|--editor) printf '%s\n' 'This candidate always uses its private arena project.' >&2; exit 2;; esac
done
exec 9>"$knight_engine_dir/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'A Valwood session is already open. Close that session before this private test.' >&2; exit 1; }
while IFS= read -r knight_variable; do
 case "$knight_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_VIEW|VALWOOD_NPC_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE_FULL|*_SEQUENCE) unset "$knight_variable";; esac
done < <(compgen -e)
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$knight_asset" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$knight_candidate_dir/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$knight_candidate_dir/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$knight_candidate_dir/loadout-rigs/loadouts.manifest"
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$knight_engine_dir"
exec "$knight_binary" --project "$knight_project" --play --scenic city-walk --width 1600 --height 900 "$@"
