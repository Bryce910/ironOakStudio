#!/usr/bin/env bash
set -euo pipefail
# Proposed weather-polish-editor cutover. Preserve the original region workcopy and all
# of its authoring drafts, recovery, saved workspace layout and player progress.
region_engine=/home/computer/Desktop/final_engine/new_engine
region_project=/home/computer/Desktop/final_engine/new_engine/build/riverbed-materials-2026-11-08/editor-workcopy-v1
region_native="$region_engine/build/weather-polish-2026-11-09/runtime-native-v2"
region_runtime="$region_native/native-bin/rpg"
region_registry=/home/computer/Desktop/final_engine/new_engine/docs/weather-polish/2026-11-09/CURRENT.json
region_lock="$region_engine/projects/seldonne/.valwood-instance.lock"
region_fail() { printf '%s\n' "$1" >&2; exit 1; }
[[ -d "$region_project" && ! -L "$region_project" ]] || region_fail 'The original Seldonne region workcopy is unavailable.'
region_require_file() {
    [[ -f "$1" && ! -L "$1" ]] || region_fail "Required editor support file is unavailable: $1"
    [[ "$(sha256sum -- "$1" | cut -d ' ' -f 1)" == "$2" ]] || region_fail "The accepted editor support file has changed: $1"
}
region_require_file "${region_runtime}" 3bc9c2bd51686db1d5e16b9c063a6f2c39d20729a9bc97a43f9348016ad71938
region_require_file "${region_native}/READY.json" ebc6b895ba48c469c4c6f9869bfee03afc0bae51b32f9ae4dd05435a81ce9109
region_require_file "${region_native}/BUILD_PLAN.json" fa36a2b7ab1c0dc1ea6f0f45bd69c96949f358d9ec34218a28beb3fb35528e93
region_require_file "${region_registry}" '7ed151bc104258460319a3f3194b7fb17221c7fb239d71621a57362443a6e6cc'
region_require_file "${region_native}/native-bin/SUPPORT-MANIFEST.json" 63c3fcdbb0ce86da5c54822ba2b5a232c4d1cc69cfc14d3fa1a6101ec4988220
region_require_file "${region_native}/native-bin/valwood_project_validate" a713136f1102666b3dd6b1eeb44e79c3e54e60b0e0cfb198b313bf39e6096e4d
region_require_file "${region_native}/native-bin/tools/import_world_asset.py" d0910abbb9729180fbdddb11ddaec8892f50ad9e846714bd3a72444a5de260d9
region_require_file "${region_native}/native-bin/tools/check_world_performance.py" 1cdb8bd3de3575c19fda429572d970979f3c54a11581bd9e1683836855bbe2fc
command -v python3 >/dev/null || region_fail 'Python 3.9 or newer is required for model imports and frame audits.'
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)' || region_fail 'Python 3.9 or newer is required for model imports and frame audits.'
python3 - "$region_project" "$region_native/native-bin/SUPPORT-MANIFEST.json" <<'WEATHER_SHADER_CHECK'
import hashlib,json,sys
from pathlib import Path
project=Path(sys.argv[1]);manifest=json.loads(Path(sys.argv[2]).read_text())
# The versioned runtime is the reference; project overrides must stay matched.
runtime=Path(sys.argv[2]).parent
for p in (runtime/'shaders').rglob('*'):
 if p.is_file():
  q=project/'shaders'/p.relative_to(runtime/'shaders')
  if not q.is_file() or q.is_symlink() or hashlib.sha256(q.read_bytes()).digest()!=hashlib.sha256(p.read_bytes()).digest():raise SystemExit('Weather editor shader mismatch: '+str(q))
WEATHER_SHADER_CHECK
command -v flock >/dev/null || region_fail 'The native session lock tool is unavailable.'
[[ -f "${region_lock}" && ! -L "${region_lock}" ]] || region_fail 'The shared Valwood session lock is unavailable.'

for region_control in project.valwood world.studio publisher_showcase.json city_showcase.json showcase.views; do
    [[ -f "$region_project/$region_control" && ! -L "$region_project/$region_control" ]] || region_fail "Missing ordinary region control: $region_control"
done
for region_guard in opening.config opening.scene arena.config playtest.route opening/home.settings opening/interior.settings; do
    [[ ! -e "$region_project/$region_guard" && ! -L "$region_project/$region_guard" ]] || region_fail "Legacy startup trigger remains in the region workcopy: $region_guard"
done
[[ -d "$region_project/texture_blocks" && ! -L "$region_project/texture_blocks" ]] || region_fail 'The region-owned texture cache is unavailable.'
if [[ "${1:-}" == --check-only && $# == 1 ]]; then
    printf '%s\n' 'Seldonne weather-polish-editor shortcut preflight passed; no application launched.'
    exit 0
fi
[[ $# == 0 ]] || region_fail 'Use this shortcut without arguments, or --check-only.'
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
exec "$region_runtime" --project "$region_project" --play --editor --scenic city-meadow --width 1600 --height 900
