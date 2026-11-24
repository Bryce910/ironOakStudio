#!/usr/bin/env bash
set -euo pipefail
knight_work=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
knight_engine_root=$(cd -- "$knight_work/../.." && pwd)
knight_source="$knight_work/source-v1"
knight_build="$knight_work/controller-preview-diagnostic"
mkdir -p -- "$knight_build"
pkg-config --exists sdl2 libpng
cc -std=c99 -O2 -Wall -Wextra -I "$knight_source" -c "$knight_source/engine/texture_cache.c" -o "$knight_build/texture_cache.o"
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I "$knight_source" \
  $(pkg-config --cflags sdl2 libpng) \
  "$knight_work/preview_knight_controller.cpp" \
  "$knight_work/diagnostic-preview-adapter.cpp" \
  "$knight_source/engine/world_training_combat.cpp" \
  "$knight_source/engine/world_animation.cpp" \
  "$knight_source/engine/world_animation_render.cpp" \
  "$knight_build/texture_cache.o" \
  $(pkg-config --libs sdl2 libpng) -lGL -pthread \
  -o "$knight_build/preview_knight_controller"
printf 'Built %s\n' "$knight_build/preview_knight_controller"
