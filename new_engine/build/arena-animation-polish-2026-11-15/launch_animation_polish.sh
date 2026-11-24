#!/usr/bin/env bash
set -euo pipefail
animation_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
animation_engine=$(cd -- "$animation_stage/../.." && pwd)
animation_binary="$animation_stage/runtime-native-v1/native-bin/rpg"
animation_project="$animation_stage/arena-project"
animation_hero=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$animation_stage/HERO_ASSET.json")
for animation_arg in "$@"; do
 case "$animation_arg" in --project|--project=*|--editor) printf '%s\n' 'This launcher uses its own arena project.' >&2; exit 2;; esac
done
exec 9>"$animation_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'A Valwood session is already open. Close it before opening the updated arena.' >&2; exit 1; }
while IFS= read -r animation_variable; do
 case "$animation_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_*|*_SEQUENCE) unset "$animation_variable";; esac
done < <(compgen -e)
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$animation_hero" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$animation_stage/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$animation_stage/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$animation_stage/loadout-rigs/loadouts.manifest"
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$animation_engine"
exec "$animation_binary" --project "$animation_project" --play --scenic city-walk --width 1600 --height 900 "$@"
