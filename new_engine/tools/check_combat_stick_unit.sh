#!/usr/bin/env bash
set -euo pipefail
combat_stick_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd -- "$combat_stick_root"
combat_stick_tmp="$(mktemp -d /tmp/valwood-combat-stick-unit-XXXXXX)"
trap 'rm -rf -- "$combat_stick_tmp"' EXIT
read -ra combat_stick_cflags <<< "$(pkg-config --cflags sdl2)"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -I. "${combat_stick_cflags[@]}" tests/world_combat_stick_test.cpp engine/world_training_combat.cpp -o "$combat_stick_tmp/combat-stick"
"$combat_stick_tmp/combat-stick"
