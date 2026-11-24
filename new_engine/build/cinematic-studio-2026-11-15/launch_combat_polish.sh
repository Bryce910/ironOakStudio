#!/usr/bin/env bash
set -euo pipefail
polish_stage=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
polish_engine=$(cd -- "$polish_stage/../.." && pwd)
polish_parent="$polish_engine/build/arena-animation-polish-2026-11-15"
python3 "$polish_stage/check_cinematic_studio_ready.py" --check-only --arena
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
for polish_arg in "$@"; do
 case "$polish_arg" in --project|--project=*|--editor) printf '%s\n' 'This launcher uses the reviewed arena project.' >&2; exit 2;; esac
done
exec 9<>"$polish_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Save and close that session before opening combat practice.' >&2; exit 1; }
while IFS= read -r polish_variable; do
 case "$polish_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_*|*_SEQUENCE) unset "$polish_variable";; esac
done < <(compgen -e)
polish_hero=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["path"])' "$polish_stage/equipment-assets/HERO_ASSET.json")
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
export VALWOOD_MODULAR_KNIGHT="$polish_hero" VALWOOD_MODULAR_KNIGHT_THIRD_PERSON=1
export VALWOOD_MODULAR_KNIGHT_SHADER="$polish_parent/shaders/world_skeletal.vs.glsl"
export VALWOOD_MODULAR_KNIGHT_FIRST_PERSON_SHADER="$polish_parent/shaders/rig_first_person.fs.glsl"
export VALWOOD_SKELETAL_LOADOUTS="$polish_stage/equipment-assets/loadouts.manifest"
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE VALWOOD_CINEMATIC_EDITOR
cd -- "$polish_engine"
exec "$polish_stage/runtime-editor-native-v1/native-bin/rpg" --project "$polish_parent/arena-project" --play --scenic city-walk --width 1600 --height 900 "$@"
