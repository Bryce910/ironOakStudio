#!/usr/bin/env bash
set -euo pipefail
arena_task_root=/home/computer/Desktop/final_engine
arena_engine="$arena_task_root/new_engine"
arena_project=/home/computer/Desktop/final_engine/new_engine/build/combat-reaction-polish-2026-11-10/arena-user-project-v1
arena_binary=/home/computer/Desktop/final_engine/new_engine/build/combat-reaction-polish-2026-11-10/runtime-native-v2/native-bin/rpg
arena_check_only=0
if [[ $# == 1 && "${1-}" == --check-only ]]; then arena_check_only=1; set --; fi
for arena_arg in "$@"; do
 case "$arena_arg" in --project|--project=*|--editor|--scenic|--scenic=*|--no-hud|--benchmark|--frames|--frames=*) printf '%s\n' 'The arena launcher preserves its prepared fight and controls.' >&2; exit 2;; esac
done
[[ -x "$arena_binary" && -f "$arena_project/arena.config" && -f "$arena_project/CHARACTER_POLISH.json" && -f "$arena_project/models/unarmed_hero/UNARMED-HERO.json" && -f "$arena_project/collision/structures/catalog" && -f /home/computer/Desktop/final_engine/new_engine/docs/combat-reaction-polish/2026-11-10/CURRENT.json ]] || { printf '%s\n' 'The verified combat practice is incomplete.' >&2; exit 1; }
[[ "$(sha256sum -- "$arena_binary" | cut -d ' ' -f 1)" == d9df7652563a79fbfb5feb56887e304ec0be1ce8ff51f41a13157438849c93ae ]] || { printf '%s\n' 'The verified combat runtime has changed.' >&2; exit 1; }
[[ -f "$arena_project/models/fatality_hero/FATALITY-BANK.json" ]] || { printf '%s\n' 'The combat fatality assets are missing.' >&2; exit 1; }
[[ -f "$arena_project/models/weapon_loadouts/WEAPON-LOADOUTS.json" ]] || { printf "%s\n" "Weapon loadout assets are missing." >&2; exit 1; }
[[ -f "$arena_project/player-profile.config" && -f "$arena_project/../shared-profile/player.progress" ]] || { printf "%s\n" "The shared player profile is missing." >&2; exit 1; }
[[ -f "$arena_project/models/combat_anatomy/anatomy.bin" && -f "$arena_project/models/combat_anatomy/caps/cap_171.glb" ]] || { printf "%s\n" "Anatomical severing assets are missing." >&2; exit 1; }
if (( arena_check_only )); then printf '%s\n' 'Combat practice preflight passed; no application launched.'; exit 0; fi
python3 -c 'import json,sys; sys.exit(0 if json.load(open(sys.argv[1])).get("accepted") is True else 1)' /home/computer/Desktop/final_engine/new_engine/docs/combat-reaction-polish/2026-11-10/CURRENT.json || { printf "%s\n" "The new game build is not activated yet." >&2; exit 1; }
exec 9>"$arena_engine/projects/seldonne/.valwood-instance.lock"
flock -n 9 || { printf '%s\n' 'Valwood is already running. Close that session before opening combat practice.' >&2; exit 1; }
while IFS= read -r arena_variable; do
 case "$arena_variable" in VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_PLAYTEST_*|VALWOOD_DISABLE_*|VALWOOD_CAPTURE_VIEW|VALWOOD_NPC_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE|VALWOOD_FOLIAGE_SEQUENCE_FULL|*_SEQUENCE) unset "$arena_variable";; esac
done < <(compgen -e)
export VALWOOD_PRESENTATION=1 VALWOOD_PLAYTEST_SLICE=city VALWOOD_COMBAT_ARENA=1
unset VALWOOD_OPENING_PROLOGUE VALWOOD_SHOWCASE
cd -- "$arena_engine"
exec "$arena_binary" --project "$arena_project" --play --scenic city-walk --width 1600 --height 900 "$@"
