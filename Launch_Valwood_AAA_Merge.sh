#!/usr/bin/env bash
# AAA merge build: Codex's woodland-polish + AAA lighting work + camp/trail/sky/smoke fixes.
# Plays your normal saved world (projects/valwood_current). The published build is untouched.
# Usage: ./Launch_Valwood_AAA_Merge.sh [demo|editor|arena|joust|world]
set -euo pipefail
valwood_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 -B "$valwood_root/new_engine/build/aaa-merge-2026-11-21/launch.py" "$@"
