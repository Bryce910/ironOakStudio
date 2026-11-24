#!/usr/bin/env bash
set -euo pipefail
clash_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
clash_engine=$(cd -- "$clash_stage/../.." && pwd)
clash_binary="$clash_stage/runtime-native-v1/native-bin/rpg"
clash_project="$clash_stage/arena-project"
clash_hero=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$clash_stage/HERO_ASSET.json")
for clash_arg in "$@"; do
 case "$clash_arg" in --project|--project=*|--editor) printf '%s\n' 'This launcher uses its own arena project.' >&2; exit 2;; esac
done
exec 9>"$clash_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'A Valwood session is already open. Close it before opening the updated arena.' >&2; exit 1; }
while IFS= read -r clash_variable; do
 case "$clash_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_*|*_SEQUENCE) unset "$clash_variable";; esac
done < <(compgen -e)
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$clash_hero" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$clash_stage/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$clash_stage/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$clash_stage/loadout-rigs/loadouts.manifest"
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$clash_engine"
exec "$clash_binary" --project "$clash_project" --play --scenic city-walk --width 1600 --height 900 "$@"
