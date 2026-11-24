#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT/cooked}"
COOKER="/tmp/asset_cooker"

cc -std=c99 -O2 -I"$ROOT" "$ROOT/tools/asset_cooker.c" "$ROOT/ufbx.c" -o "$COOKER" -lm

declare -a inputs=()

for dir in assets ground models shaders textures audio; do
    if [[ -e "$ROOT/$dir" ]]; then
        inputs+=("${dir}")
    fi
done

while IFS= read -r path; do
    path="${path#./}"
    inputs+=("$path")
done < <(
    cd "$ROOT" && find . -maxdepth 1 -type f \
        \( -iname '*.fbx' -o -iname '*.gltf' -o -iname '*.glb' -o \
           -iname '*.png' -o -iname '*.jpg' -o -iname '*.jpeg' -o \
           -iname '*.tga' -o -iname '*.bmp' -o -iname '*.wav' -o \
           -iname '*.ogg' -o -iname '*.mp3' -o -iname '*.fs' -o \
           -iname '*.vs' -o -iname '*.bin' \)
)

if [[ ${#inputs[@]} -eq 0 ]]; then
    echo "No cookable assets found."
    exit 0
fi

cd "$ROOT"
"$COOKER" "$OUT_DIR" "${inputs[@]}"
