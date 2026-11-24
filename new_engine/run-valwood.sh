#!/usr/bin/env bash
set -euo pipefail
engine_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ! -f "$engine_root/projects/seldonne/project.valwood" ]]; then
    printf '%s\n' 'Prepare the starting area with: python3 tools/build_seldonne.py' >&2
    exit 1
fi
# Keep a second shortcut click from starting another GPU-heavy copy of Seldonne.
# The descriptor survives exec and the OS releases the lock on every exit path.
if command -v flock >/dev/null 2>&1; then
    exec 9>"$engine_root/projects/seldonne/.valwood-instance.lock"
    if ! flock -n 9; then
        printf '%s\n' 'Valwood is already running. Use Ctrl+Enter to switch between play and the editor.' >&2
        if command -v wmctrl >/dev/null 2>&1; then
            wmctrl -a 'Valwood Studio | Seldonne' 2>/dev/null || true
        fi
        exit 0
    fi
fi
exec "$engine_root/rpg" --project "$engine_root/projects/seldonne" --play "$@"
