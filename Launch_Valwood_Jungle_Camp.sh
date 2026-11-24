#!/usr/bin/env bash
set -euo pipefail
valwood_camp_launcher_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$valwood_camp_launcher_dir/Launch_Valwood_Demo.sh" --scenic rainforest-camp "$@"
