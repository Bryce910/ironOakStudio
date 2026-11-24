#!/usr/bin/env bash
set -euo pipefail
controller_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$controller_root"
controller_tmp="$(mktemp -d /tmp/valwood-controller-unit-XXXXXX)"
trap 'rm -rf -- "$controller_tmp"' EXIT
read -ra controller_cflags <<< "$(pkg-config --cflags sdl2)"
read -ra controller_libs <<< "$(pkg-config --libs sdl2)"
"${CC:-gcc}" -std=c99 -O2 -Wall -Wextra -Werror -I. "${controller_cflags[@]}" -c engine/input_runtime.c -o "$controller_tmp/input.o"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -I. "${controller_cflags[@]}" tests/world_controller_test.cpp "$controller_tmp/input.o" "${controller_libs[@]}" -o "$controller_tmp/controller"
"$controller_tmp/controller"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -I. -Ithird_party/imgui tests/world_controller_ui_test.cpp third_party/imgui/imgui.o third_party/imgui/imgui_draw.o third_party/imgui/imgui_tables.o third_party/imgui/imgui_widgets.o -o "$controller_tmp/ui"
"$controller_tmp/ui"
