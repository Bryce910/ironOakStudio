#!/usr/bin/env bash
set -euo pipefail
# Installed at tools/launch_environment_editor.sh. Existing ordinary project
# paths, scene arguments, optional user flags and calling shortcuts are kept.
editor_engine="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor_name="${1:?Project name required}"
editor_scene="${2:?Starting view required}"
shift 2
editor_project="$editor_engine/projects/$editor_name"
editor_native="$editor_engine/build/world-audio-editor-2026-11-09/runtime-native-v1"
editor_runtime="$editor_native/native-bin/rpg"
editor_registry="$editor_engine/docs/world-audio-editor/2026-11-09/CURRENT.json"
editor_lock="$editor_engine/projects/seldonne/.valwood-instance.lock"
editor_fail() { printf '%s\n' "$1" >&2; exit 1; }
[[ "$editor_name" =~ ^[A-Za-z0-9_-]+$ ]] || editor_fail 'Use an ordinary project name under the projects folder.'
[[ -d "$editor_project" && -f "$editor_project/project.valwood" ]] || editor_fail 'The selected environment project is not available.'
editor_require_file() {
    [[ -f "$1" && ! -L "$1" ]] || editor_fail "Required editor support file is unavailable: $1"
    [[ "$(sha256sum -- "$1" | cut -d ' ' -f 1)" == "$2" ]] || editor_fail "The accepted editor support file has changed: $1"
}
editor_require_file "${editor_runtime}" bf7cf9c5c15c79a097f6beb2aac9f1879e239cdf86d4c4c872709f4238167d1b
editor_require_file "${editor_native}/READY.json" aebbded461832ac75174a99f1b4d517069a36b911faa67147e3194e7937b8132
editor_require_file "${editor_native}/BUILD_PLAN.json" 4956de827b6c86c04df23ac54e81f989a02943190e29eff620b942c4a05a35ba
editor_require_file "${editor_registry}" '20ee74aabd08d8d97d5088f683a01fcce92e1c557da808b758377cbfbb04b8bc'
editor_require_file "${editor_native}/native-bin/SUPPORT-MANIFEST.json" 681c33575b6d67c4185c0219cefe6eb090009bb26c8effbabf80bc74fb412b5c
editor_require_file "${editor_native}/native-bin/valwood_project_validate" fb9ee5042616337054daafda79a9025bd84d93d388e0b148d0fd187d0ebb7835
editor_require_file "${editor_native}/native-bin/tools/import_world_asset.py" d0910abbb9729180fbdddb11ddaec8892f50ad9e846714bd3a72444a5de260d9
editor_require_file "${editor_native}/native-bin/tools/check_world_performance.py" 1cdb8bd3de3575c19fda429572d970979f3c54a11581bd9e1683836855bbe2fc
command -v python3 >/dev/null || editor_fail 'Python 3.9 or newer is required for model imports and frame audits.'
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)' || editor_fail 'Python 3.9 or newer is required for model imports and frame audits.'
command -v flock >/dev/null || editor_fail 'The native session lock tool is unavailable.'
[[ -f "${editor_lock}" && ! -L "${editor_lock}" ]] || editor_fail 'The shared Valwood session lock is unavailable.'

if [[ "${1:-}" == --check-only && $# == 1 ]]; then
    printf '%s\n' 'Environment audio-editor preflight passed; no application launched.'
    exit 0
fi
exec 9<>"$editor_lock"
flock -n 9 || editor_fail 'Valwood is already running. Close that session before opening another editor.'
# Retain intentional ordinary shortcut flags, including the City shortcut's
# VALWOOD_PRESENTATION export. The launcher never edits startup project data.
cd -- "$editor_engine"
exec "$editor_runtime" --project "$editor_project" --play --editor --scenic "$editor_scene" --width 1600 --height 900 "$@"
