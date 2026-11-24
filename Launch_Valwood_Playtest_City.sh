#!/usr/bin/env bash
set -euo pipefail
playtest_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$playtest_root/Launch_Valwood_Playtest.sh" --area city "$@"
