#!/usr/bin/env bash
set -euo pipefail
valwood_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$valwood_root/new_engine"
if [[ ! -x ./rpg ]]; then make -j4; fi
exec ./run-valwood.sh --play "$@"
