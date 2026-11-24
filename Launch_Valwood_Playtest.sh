#!/usr/bin/env bash
set -euo pipefail
playtest_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
playtest_engine="$playtest_root/new_engine"
playtest_area=""
if [[ "${1-}" == --area ]]; then
    [[ $# -ge 2 ]] || { printf '%s\n' 'Use --area city, jungle, or grandstone.' >&2; exit 2; }
    playtest_area="$2"; shift 2
elif [[ "${1-}" == city || "${1-}" == jungle || "${1-}" == grandstone ]]; then
    playtest_area="$1"; shift
elif [[ $# -gt 0 ]]; then
    printf '%s\n' 'Use Launch_Valwood_Playtest.sh --area city|jungle|grandstone [runtime options].' >&2
    exit 2
fi
if [[ -z "$playtest_area" ]]; then
    if command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY-}" ]]; then
        playtest_area="$(zenity --list --title='Valwood gameplay slices' --width=620 --height=280 \
            --text='Choose an area for sword combat and free exploration.' --column=Area --column=Playtest \
            city 'Armed hero, city fight and countryside' jungle 'Armed hero, jungle fight and exploration' \
            grandstone 'Armed hero, first hall fight and snowy approach' --print-column=1)" || exit 0
    elif [[ -t 0 ]]; then
        printf '%s\n' 'Playable slices: city / jungle / grandstone'
        read -r -p 'Area: ' playtest_area
    else
        printf '%s\n' 'Choose --area city, jungle, or grandstone.' >&2; exit 2
    fi
fi
case "$playtest_area" in
    city) playtest_scene=city-walk ;;
    jungle) playtest_scene=rainforest-milestone ;;
    grandstone) playtest_scene=stronghold-town ;;
    *) printf '%s\n' 'Unknown playtest area.' >&2; exit 2 ;;
esac
for playtest_arg in "$@"; do
    case "$playtest_arg" in
        --project|--project=*|--editor|--scenic|--scenic=*|--no-hud)
            printf '%s\n' 'The playtest launcher preserves its isolated project, route and guidance UI.' >&2; exit 2 ;;
    esac
done
playtest_binary="$playtest_engine/build/collision-water-2026-11-06/runtime-native-v9/native-bin/rpg"
playtest_project="$playtest_engine/build/playtest-slices/$playtest_area/project"
if [[ ! -x "$playtest_binary" || ! -f "$playtest_project/playtest.route" || ! -f "$playtest_project/project.valwood" ]]; then
    printf '%s\n' "The $playtest_area slice is not prepared yet. The owner must install its guarded playtest runtime and local copy." >&2
    exit 1
fi
if command -v flock >/dev/null 2>&1; then
    exec 9>"$playtest_engine/projects/seldonne/.valwood-instance.lock"
    if ! flock -n 9; then
        printf '%s\n' 'Valwood is already running. Close that session before opening a playtest.' >&2; exit 1
    fi
fi
export VALWOOD_PRESENTATION=1
export VALWOOD_PLAYTEST_SLICE="$playtest_area"
unset VALWOOD_SHOWCASE
cd -- "$playtest_engine"
exec "$playtest_binary" --project "$playtest_project" --play --scenic "$playtest_scene" --width 1600 --height 900 "$@"
