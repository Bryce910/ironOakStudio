# Valwood demo polish guide

Run `/home/computer/Desktop/final_engine/Launch_Polished_Demo.sh` for the tested snapshot at 1600×900, starting in the castle street. It launches `new_engine/builds/relocated/Valwood Demo Polish 2026-10-29/launch.sh`. The complete package can be moved; use its own `launch.sh` after moving it. The earlier verified Live World Demo package is preserved unchanged. The original source launchers continue to use the current development project.

## Walk the two demonstration routes

Start the package's `launch.sh`. WASD walks, Shift sprints, right-drag turns the camera, and V switches between first and third person. Tab opens the atlas, F9 advances the demonstration tour, F10 opens graphics settings, and F11 switches to Studio. F toggles preview flight. E interactions require walking mode. Escape releases camera capture and leaves training.

The castle route includes the arcade, shops, market, training yard and tavern. Look for physical bread, herb and tankard signs and the furnished tavern tables. The jungle route includes varied understory, the pool and dry-bank planting, canopy groupings and the inhabited fire camp. The small camp includes existing residents and animals.

At the training yard, approach the master of arms and press E, then choose Begin training. Mouse movement or arrow keys choose attack direction. Left mouse attacks, right mouse guards, T chooses thrust, middle mouse thrusts and Space dodges. A finished or cancelled round can be restarted. The jungle camp steward also offers an E conversation.

## Quality and performance

The measured comparison uses the existing Custom settings: 1600×900 output, 1120×630 internal rendering, automatic quality disabled, with reflections, contact shadows, volumetric fog and clouds retained. The four full walks measured 57.4–59.9 FPS average and 46.5–49.7 FPS 1% lows. There were 30 isolated sub-45 frames in 35,920 samples. RESULTS.md preserves every missed frame; this is not a locked 45 FPS floor.

## Studio

F11 opens the combined designer workspace with resizable outliner, inspector and content browser. It includes registry asset search, favorites, drag placement, grouping, transforms, locking, Undo/Redo, the command finder and the existing focused authoring tools. See the designer verification under `docs/designer-workspace/2026-10-29/optimization-verification.json` in the source tree for its measured checks.

## What to watch during review

Check near-ground vegetation, the camp ridge, tree silhouettes, repeated building materials, NPC feet/turns and camera clearance. Broad slopes and repeated character silhouettes still need art direction. The new breathing idle is fitted to the existing live knight; it does not replace every population animation or add finger/facial animation.

The camp motion clip is silent and shows unmodified native display frames. It is a short correctness/animation proof and is not a timed performance capture or a finished promotional trailer.
