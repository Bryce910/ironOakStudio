#!/usr/bin/env bash
# AAA lighting candidate: bloom mip chain, auto exposure, hue-stable tone mapping, dither.
# Uses its own private copy of the world; nothing published is modified.
# Usage: ./Launch_Valwood_Tonemap_Candidate.sh [demo|editor|arena|joust|world]
set -euo pipefail
valwood_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 -B "$valwood_root/new_engine/build/aaa-lighting-2026-11-20/launch.py" "$@"
