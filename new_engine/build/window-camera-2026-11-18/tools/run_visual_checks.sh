#!/bin/bash
set -e
cd /home/computer/Desktop/final_engine
export LP_NUM_THREADS=4
export SDL_VIDEODRIVER=offscreen
export LIBGL_ALWAYS_SOFTWARE=1
python3 new_engine/build/window-camera-2026-11-18/tools/review_windows.py --baseline > new_engine/build/window-camera-2026-11-18/review/windows-baseline-run.log 2>&1
python3 new_engine/build/window-camera-2026-11-18/tools/review_windows.py > new_engine/build/window-camera-2026-11-18/review/windows-updated-run.log 2>&1
