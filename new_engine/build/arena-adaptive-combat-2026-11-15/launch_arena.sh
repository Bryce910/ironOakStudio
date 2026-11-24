#!/usr/bin/env bash
set -euo pipefail
arena_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
arena_engine=$(cd -- "$arena_stage/../.." && pwd)
arena_assets="$arena_engine/build/arena-animation-polish-2026-11-15"
arena_equipment="$arena_engine/build/cinematic-studio-2026-11-15/equipment-assets"
python3 "$arena_stage/check_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for arena_arg in "$@"; do
    case "$arena_arg" in
        --project|--project=*|--editor|--scenic|--scenic=*|--frames|--frames=*|--benchmark)
            printf '%s\n' 'Use this launcher for the reviewed combat practice arena.' >&2
            exit 2 ;;
    esac
done
exec 9<>"$arena_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save and close that session before opening the arena.' >&2; exit 1; }
while IFS= read -r arena_variable; do
    case "$arena_variable" in VALWOOD_*) unset "$arena_variable" ;; esac
done < <(compgen -e)
unset LD_PRELOAD
arena_hero=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$arena_equipment/HERO_ASSET.json")
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$arena_hero" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$arena_assets/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$arena_assets/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$arena_equipment/loadouts.manifest"
cd -- "$arena_engine"
exec "$arena_stage/runtime-native-v1/native-bin/rpg" --project "$arena_stage/arena-project" --play --scenic city-walk --width 1600 --height 900 "$@"
