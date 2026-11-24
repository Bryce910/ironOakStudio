#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build_dir="$task_root/build/hero-preview-tool"
mkdir -p -- "$task_build_dir"
pkg-config --exists sdl2 libpng
cc -std=c99 -O2 -Wall -Wextra -I "$task_root" \
  -c "$task_root/engine/texture_cache.c" -o "$task_build_dir/texture_cache.o"
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I "$task_root" \
  $(pkg-config --cflags sdl2 libpng) \
  "$task_root/tools/preview_hero_skeletal.cpp" \
  "$task_root/engine/world_animation.cpp" \
  "$task_root/engine/world_animation_render.cpp" \
  "$task_build_dir/texture_cache.o" \
  $(pkg-config --libs sdl2 libpng) -lGL -pthread \
  -o "$task_build_dir/preview_hero_skeletal"
printf 'Built %s\n' "$task_build_dir/preview_hero_skeletal"
