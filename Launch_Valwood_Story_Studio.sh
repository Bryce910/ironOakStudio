#!/usr/bin/env bash
set -euo pipefail
valwood_story_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 -B "$valwood_story_root/new_engine/tools/quest_story_editor.py" "$@"
