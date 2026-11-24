# Grandstone rendering source check

The close black patches on the first hall's red aisle are present in the active purchased floor albedo. The extracted `M_Marble_floor_red_path.png` and `M_Marble_floor_red_rosete.png` previews contain the same worn-stone craters seen in the final combat capture. Both materials are opaque; these patches do not establish holes in the floor or failed model coverage. The previews are texture extractions, not game screenshots.

`source-render-audit.json` records the current `world.studio` hash, exact active imported GLBs, texture hashes and source carpet, stair and gate leaf positions. The source gate leaves are visibly closed in the purchased layout. Walking into them should stop at their visible surface; unrelated doorway trim should not create a raised walking floor.

All 242 current source HIGH/MID model bounds have a nonnegative local floor datum. The source scenery sphere calculation therefore encloses these models; a guessed culling or material replacement is not justified by these captures. The already-installed bridge continuity correction remains separate from the close floor textures.

There is a concrete tight-camera risk: the renderer's current near plane is 0.4 m, and at 1.08 rad vertical FOV and 16:9 aspect its near-plane corner is 0.633 m from the camera eye. A small ray/sphere probe of only the eye can pass while that near rectangle intersects a door frame or stair surface. The report includes both rectangle and full corner radii so camera clearance can be checked against actual framing. This is a geometric diagnosis; native doorway/stair motion captures are required to accept a camera fix.

The cleanup runtime now uses a 0.12 m near plane while the source triangle walking service is active. At the same FOV/aspect the corner is 0.190 m from the eye, fitting inside the existing 0.22 m follow-camera clearance radius. The source audit above records the earlier diagnosis; acceptance screenshots and the obstacle sidecar must establish the corrected runtime's appearance and active near-plane value. Ordinary projects retain their previous projection.

Reproduce the source audit with `python3 -B tools/audit_grandstone_render_sources.py`. It reads active project/source data and writes only this evidence directory. No model, material, shader, texture bank, source scene or project placement was modified by this audit.
