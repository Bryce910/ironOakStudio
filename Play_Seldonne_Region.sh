#!/usr/bin/env bash
set -euo pipefail
region_engine=/home/computer/Desktop/final_engine/new_engine
region_project=/home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-workcopy-v1
region_runtime=/home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/rpg
region_registry=/home/computer/Desktop/final_engine/new_engine/docs/riverbed-materials/2026-11-08/CURRENT.json
region_lock="$region_engine/projects/seldonne/.valwood-instance.lock"
region_fail() { printf '%s\n' "$1" >&2; exit 1; }
[[ -x "$region_runtime" && -f "$region_registry" && -d "$region_project" && ! -L "$region_project" ]] || region_fail 'The Seldonne region workcopy or accepted editor runtime is unavailable.'
[[ "$(sha256sum -- "$region_runtime" | cut -d ' ' -f 1)" == 3f5f501f2d9c662cab80346acd65e4c31d1ab660e6dc00d7dcce2469398f8749 ]] || region_fail 'The accepted Seldonne runtime has changed.'
[[ "$(sha256sum -- "$region_registry" | cut -d ' ' -f 1)" == a0250fece5cb6b3c63f66e36df6048b0e6ec8240cd1c5bc28ef08c7a3ea888ab ]] || region_fail 'The accepted editor registry has changed; regenerate these Seldonne shortcuts.'
[[ -f /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/SUPPORT-MANIFEST.json && ! -L /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/SUPPORT-MANIFEST.json && "$(sha256sum -- /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/SUPPORT-MANIFEST.json | cut -d ' ' -f 1)" == 339e80c40f44f83971034574d13ac262a53c5a1311b7c34707ba4ac4ff738657 ]] || region_fail 'The accepted editor support files have changed.'
[[ -f /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/rpg && ! -L /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/rpg && "$(sha256sum -- /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/rpg | cut -d ' ' -f 1)" == 3f5f501f2d9c662cab80346acd65e4c31d1ab660e6dc00d7dcce2469398f8749 ]] || region_fail 'The accepted editor support files have changed.'
[[ -f /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/valwood_project_validate && ! -L /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/valwood_project_validate && "$(sha256sum -- /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/valwood_project_validate | cut -d ' ' -f 1)" == 17f8c011477d751e628fe13b2c8af1c85c9914b0400fc56b716617edbf0c130e ]] || region_fail 'The accepted editor support files have changed.'
[[ -f /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/import_world_asset.py && ! -L /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/import_world_asset.py && "$(sha256sum -- /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/import_world_asset.py | cut -d ' ' -f 1)" == d0910abbb9729180fbdddb11ddaec8892f50ad9e846714bd3a72444a5de260d9 ]] || region_fail 'The accepted editor support files have changed.'
[[ -f /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/check_world_performance.py && ! -L /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/check_world_performance.py && "$(sha256sum -- /home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-support-native-v1/tools/check_world_performance.py | cut -d ' ' -f 1)" == 1cdb8bd3de3575c19fda429572d970979f3c54a11581bd9e1683836855bbe2fc ]] || region_fail 'The accepted editor support files have changed.'

for region_control in project.valwood world.studio publisher_showcase.json city_showcase.json showcase.views; do
    [[ -f "$region_project/$region_control" && ! -L "$region_project/$region_control" ]] || region_fail "Missing ordinary region control: $region_control"
done
for region_guard in opening.config opening.scene arena.config playtest.route opening/home.settings opening/interior.settings; do
    [[ ! -e "$region_project/$region_guard" && ! -L "$region_project/$region_guard" ]] || region_fail "Legacy startup trigger remains in the region workcopy: $region_guard"
done
[[ -d "$region_project/texture_blocks" && ! -L "$region_project/texture_blocks" ]] || region_fail 'The region-owned texture cache is unavailable.'
if [[ "${1:-}" == --check-only && $# == 1 ]]; then
    printf '%s\n' 'Seldonne shortcut preflight passed; no application launched.'
    exit 0
fi
[[ $# == 0 ]] || region_fail 'Use this shortcut without arguments, or --check-only.'
command -v flock >/dev/null || region_fail 'The native session lock tool is unavailable.'
[[ -f "$region_lock" ]] || region_fail 'The shared Valwood session lock is unavailable.'
exec 9<>"$region_lock"
flock -n 9 || region_fail 'Valwood is already running. Close that session before opening Seldonne.'
while IFS= read -r region_variable; do
    case "$region_variable" in
        VALWOOD_QA_*|VALWOOD_TEST_*|VALWOOD_DISABLE_*|VALWOOD_PLAYTEST_*|VALWOOD_SHOWCASE|VALWOOD_PRESENTATION|VALWOOD_OPENING_PROLOGUE|VALWOOD_CAPTURE_VIEW|VALWOOD_SHADOW_RESOLUTION|*_SEQUENCE|LIBGL_ALWAYS_SOFTWARE|GALLIUM_DRIVER|MESA_LOADER_DRIVER_OVERRIDE|LIBGL_DRIVERS_PATH|SDL_VIDEODRIVER|EGL_PLATFORM|LIBGL_ALWAYS_INDIRECT|MESA_GL_VERSION_OVERRIDE|MESA_GLSL_VERSION_OVERRIDE)
            unset "$region_variable" ;;
    esac
done < <(compgen -e)
if [[ "${SDL_AUDIODRIVER:-}" == dummy ]]; then unset SDL_AUDIODRIVER; fi
cd -- "$region_engine"
exec "$region_runtime" --project "$region_project" --play --scenic city-meadow --width 1600 --height 900
