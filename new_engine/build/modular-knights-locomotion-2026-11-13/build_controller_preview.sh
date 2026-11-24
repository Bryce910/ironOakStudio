#!/usr/bin/env bash
set -euo pipefail
knight_work=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
knight_engine_root=$(cd -- "$knight_work/../.." && pwd)
knight_source="$knight_work/source-v1"
knight_build="$knight_work/controller-preview"
mkdir -p -- "$knight_build"
python3 - "$knight_work" "$knight_source" "$knight_build" <<'PY'
import hashlib,json,sys
from pathlib import Path
work,source,build=map(Path,sys.argv[1:])
paths=[p for p in source.rglob('*') if p.is_file()]
paths += [work/'preview_knight_controller.cpp',work/'build_controller_preview.sh',work.parents[1]/'tools/preview_hero_skeletal.cpp']
def digest(p):
    with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
(build/'BUILD_INPUTS_PENDING.json').write_text(json.dumps({str(p):digest(p) for p in paths},indent=2)+'\n')
PY
pkg-config --exists sdl2 libpng
cc -std=c99 -O2 -Wall -Wextra -I "$knight_source" -c "$knight_source/engine/texture_cache.c" -o "$knight_build/texture_cache.o"
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I "$knight_source" \
  $(pkg-config --cflags sdl2 libpng) \
  "$knight_work/preview_knight_controller.cpp" \
  "$knight_source/engine/world_skeletal_hero.cpp" \
  "$knight_source/engine/world_training_combat.cpp" \
  "$knight_source/engine/world_animation.cpp" \
  "$knight_source/engine/world_animation_render.cpp" \
  "$knight_build/texture_cache.o" \
  $(pkg-config --libs sdl2 libpng) -lGL -pthread \
  -o "$knight_build/preview_knight_controller"
python3 - "$knight_build" <<'PY'
import hashlib,json,sys
from pathlib import Path
build=Path(sys.argv[1]);inputs=json.loads((build/'BUILD_INPUTS_PENDING.json').read_text())
def digest(p):
    with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
binary=build/'preview_knight_controller'
if any(digest(Path(p))!=sha for p,sha in inputs.items()):
    binary.unlink();sys.exit('Animation preview source changed during compilation; rebuild it before review.')
(build/'BUILD_INPUTS.json').write_text(json.dumps({'binary':str(binary),'binary_sha256':digest(binary),'sha256':inputs},indent=2)+'\n')
PY
printf 'Built %s\n' "$knight_build/preview_knight_controller"
