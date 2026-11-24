#!/usr/bin/env bash
set -euo pipefail
feedback_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
feedback_stage="$feedback_root/new_engine/build/arena-contact-feedback-2026-11-14"
python3 "$feedback_stage/check_feedback_ready.py" --check-only
if [[ $# == 1 && "$1" == --check-only ]]; then exit 0; fi
exec "$feedback_stage/launch_feedback_candidate.sh" "$@"
