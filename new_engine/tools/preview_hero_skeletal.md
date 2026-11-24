# Standalone skeletal asset preview

Build from any directory on Linux (requires C/C++17, SDL2, OpenGL and libpng):

```bash
bash /home/computer/Desktop/final_engine/new_engine/tools/build_hero_preview.sh
```

From the repository root, interactively preview a real existing rig:

```bash
new_engine/build/hero-preview-tool/preview_hero_skeletal \
  new_engine/projects/valwood_world/models/animations/white_knight_brown_armor_demo.glb
```

Drag with either mouse button to orbit. Wheel zooms. Space pauses. `[` and `]`
change clips. Left/Right arrows pause and step by 1/60 second. `R` shows the rest
pose, Home resets the camera, Escape closes only this preview. Clip names and
indices are printed to stdout. Supply `--clip NAME`, `--clip 0` or `--clip rest`.

Deterministic hidden capture, without operating the desktop or launching a game:

```bash
new_engine/build/hero-preview-tool/preview_hero_skeletal \
  new_engine/projects/valwood_world/models/animations/white_knight_brown_armor_demo.glb \
  --clip 0 --phase .35 --frames 1 --capture /tmp/hero-phase-035.png \
  > /tmp/hero-phase-035.log 2>&1
```

`--frames N` renders exactly N frames with fixed steps of `1/--fps` seconds
(default 60), and writes the final frame if `--capture OUTPUT.png` is provided.
`--phase` is the initial normalized looping clip phase. The first frame is at
that phase, so `--frames 1` captures it exactly. `--paused` holds that phase.
Bounded runs are hidden by default; use `--visible` to watch them. They still
require an available OpenGL context. On machines supporting SDL's offscreen GL
driver, prefix the command with `SDL_VIDEODRIVER=offscreen`. Context, shader,
image-decoding, skin-upload and GL failures produce a nonzero exit and a clear
stderr message. Capture failures also fail the run. Existing logs are never
removed; redirect each inspection to its own file.

For an honest motion-preview video, add `--capture-dir /tmp/hero-frames` to a
bounded run, for example `--frames 120 --fps 30`. PNGs are sequentially named
`frame-000000.png`, `frame-000001.png`, and so on. `--capture-every N` captures
every Nth rendered frame (default 1). Encode with ffmpeg using input framerate
`fps / N`, for example `ffmpeg -framerate 30 -i /tmp/hero-frames/frame-%06d.png
-c:v libx264 -pix_fmt yuv420p /tmp/hero-preview.mp4`. Use a new output directory
for each sequence so old frames cannot get mixed into the video.

Other options: `--width`, `--height` (128–4096), `--yaw` and `--pitch` in degrees,
`--zoom` (smaller is closer), `--shaders DIRECTORY`, and `--help`.

The tool imports and validates through `world_animation`, samples the actual
source clips and uploads real palettes through `world_animation_render`. It
uses the existing `world_skeletal.vs.glsl` and all supported source base-color,
normal, metallic/roughness/AO and emissive maps and factors. Geometry is framed
by sampling the selected clip's bounds; its display height is normalized to
1.8 units, with original source height and the preview scale logged. Source root
motion is retained. Material colors and geometry are not replaced with invented
character details. A neutral ground grid and separately identified analytic
preview lights help inspect silhouettes, pose deformation and surfaces.

**This is an isolated asset preview, not gameplay integration or a guarantee of
animation quality.** Its simple preview lighting is not Unity HDRP or Valwood's
world lighting. It has no world collisions, foot IK, movement controller,
retargeting, equipment grips or combat behavior. It obeys the current importer
limits (one skin/mesh node, embedded images, at most four influences, no morph
targets). Unsupported assets reject explicitly. It does not touch launchers,
projects, saves or core engine code and does not build Windows executables.
