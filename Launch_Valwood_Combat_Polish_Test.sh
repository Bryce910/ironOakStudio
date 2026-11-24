#!/usr/bin/env bash
set -euo pipefail
polish_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec "$polish_root/new_engine/build/cinematic-studio-2026-11-15/launch_combat_polish.sh" "$@"
