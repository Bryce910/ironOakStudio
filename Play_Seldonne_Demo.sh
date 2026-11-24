#!/usr/bin/env bash
set -euo pipefail
demo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$demo_root/Launch_Valwood_Opening.sh" "$@"
