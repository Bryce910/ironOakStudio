The existing opening acceptance runner checks the ordinary SDL controller path,
title and pause menus, supported walking, notebook and front-door interactions,
transport, meadow arrival, jumping, the final objective, save/load and retry.
It deliberately exercises hold-to-skip. Its stage captures do not establish that
the entire unskipped cinematic has been visually reviewed.

The new apartment helper keeps visual inspection separate. CPU mode loads the
actual production Support/Jolt collision and the production model decoder. It
walks every authored house segment in both directions in steps of at most 5 cm,
resamples support twice, verifies that the coffee table and closed front leaf
block movement, rejects coordinates outside the house, and checks zero query
heap fallback. All coordinates come from the installed opening scene.

Run from `/home/computer/Desktop/final_engine/new_engine` with a fresh output:

```sh
python3 -B tools/check_opening_apartment_refresh.py --project build/opening-prologue/project --mode cpu --output docs/opening-apartment-refresh/2026-11-02/final-cpu
python3 -B tools/check_opening_prologue.py --binary build/opening-prologue/bin/rpg --project build/opening-prologue/project --scale 1 --output docs/opening-apartment-refresh/2026-11-02/native-journey
python3 -B tools/check_opening_apartment_refresh.py --binary build/opening-prologue/bin/rpg --project build/opening-prologue/project --mode captures --include-house-rail --width 1600 --height 900 --output docs/opening-apartment-refresh/2026-11-02/native-inspection
```

The capture helper expects `opening/inspection.views.json` containing an array
of `{ "name": "living-room", "eye": [x,y,z], "target": [x,y,z], "fov": 1.0472 }`
records. It uses the existing native source-view renderer with the opening state
disabled in a private copy. It selects authored camera poses, so those captures
are art review evidence and are not claimed as movement or story acceptance.
`--include-house-rail` adds exact start, midpoint and handoff compositions using
the same quintic rail interpolation as the opening. This samples the complete
rail extent; it does not claim uninterrupted playback. Use `--width 1920
--height 1080` for full-HD output. Render scale is always 1, automatic resolution
is off, and image dimensions, GPU renderer, streaming failures, GL errors and
texture references are checked. These untimed captures are not FPS benchmarks.

The normal opening uses indoor sun strength .16, sky fill .85 and exposure 1.12.
Studio's authored sun control has a .25 minimum. The private inspection copy
uses .25 sun with the city sun RGB scaled by .16/.25, giving the same exact
scene-linear direct-radiance product. Practical lights and shaders stay intact;
the source project and executable are hash-guarded. The equivalent lighting and
selected cameras are recorded alongside every image. Source-view near clipping
is .4 m while normal opening near clipping is .12 m, so choose inspection poses
with at least .4 m foreground clearance and also inspect ordinary journey shots.

All native GPU runs must be serial, hold the existing engine instance lock, and
be launched by the root agent after the user's interactive opening is closed
normally. No native GPU run was performed during QA preparation.
