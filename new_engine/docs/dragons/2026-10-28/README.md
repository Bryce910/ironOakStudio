# Castle dragons — 28 October 2026

Both supplied Hellfire and Frostshade archives are integrated into the custom Valwood engine. This is an ambient castle sequence using the purchased character meshes, authored animation and original skin maps.

The two dragons follow a 149.9-second cycle with staggered schedules: circling flight, an aerial breath pass, a curved descent, landing, a grounded idle/breath display, takeoff and departure. Native wingbeats continue through the airborne breath transition. The source flight body is retained while neck/head/jaw motion supplies that breath pose. Mouth transforms come from the same interpolated poses used by the rendered meshes.

Hellfire produces an orange flame jet; Frostshade produces a blue plume. Purchased fire/smoke art supplies layered flame, a hot core, smoke and embers, with depth-softened intersections, HDR bloom and nearby terrain/prop illumination. The plume draws after contact lighting and before weather and temporal AA. Dragon bodies and above-water breath also appear in planar reflections.

## Content and budgets

- Two skins, each with 50 compatible vertex poses: five native motion families plus an airborne breath composite. All pose pairs and cross-clip transitions are linked during loading; ready-frame drawing allocates nothing and performs no instance uploads.
- Each pose has 8,287 vertices and 12,067 triangles. The offline reduction measured approximately 2.6–3 mm mean surface deviation from the native mesh, with 1.8–2.8 cm at the 99th percentile. The maximum isolated deviation was 9.1 cm. These are imported demo meshes, not the full native approximately 100,566-triangle source.
- Runtime skin maps use 2K color and 1K normal/occlusion/roughness/metallic. Validated BC7 color caches preserve those cooked dimensions and save 33,554,376 bytes. Geometry and skin stores total approximately 91.1 MB before driver-private allocations. Breath adds approximately 12.6 MB of sprites, an 87 KB smoke map and a 14 KiB fixed instance buffer.
- At most 224 breath billboards and two draws; the staggered production sequence has one active breath display at a time. Glow points share the existing eight-light shader budget. There are no new point-light shadow maps or full-screen breath passes.
- The model bank and breath resources load only near the castle, with entry/exit hysteresis. Leaving for the jungle releases them behind the existing destination loading screen. Main materials and both breath palettes are prepared during loading.

## Landing sites and motion checks

The east and west natural benches are outside the outer town wall and moat, at village-local `(350,60)` and `(-375,60)`. Trees and large procedural dressing are excluded within 18 m; low grass feathers toward the edges. Existing terrain support and authored structures retain their geometry.

An independent audit checked 7,777 authored placements using conservative model bounds. The closest model bounds are 125.5 m and 151.7 m from the roosts. It caught one procedural tree 17.91 m from Hellfire's landing center; the strict positive-weight exclusion removes that root. Grounded meshes sampled against actual terrain have approximately 3.8–4.1 cm maximum isolated folded-wing/tail penetration on the uneven surface. Foot support remains above terrain; the controller uses a surveyed carrier floor and baked motion, rather than runtime per-foot terrain IK.

CPU tests exercise 600 seconds for both actors, every stage, airborne and grounded breath, 332 atomic load failures, residency hysteresis, three render passes and cleanup. Minimum inter-dragon separation is 58.66 m. Conservative 32 m breath paths stay outside the town/moat envelope. The native two-second wingbeat has an additional 2,521 phase checks across the airborne transition. Tests verify no hot allocations, draw uploads or live pose relinking.

Software GPU tests inspect actual imported dragon interpolation, tangent frames, matching main/reflection/shadow positions, depth-softened breath, warm/cold palettes, hostile caller state restoration and zero GL errors. Native visual captures are stored separately from timed measurements in `captures/`; `captures-before-polish/` contains the rejected initial framing/plume look.

## Performance

Native measurements in `native/` used the Radeon RX 5300. Tests retain every frame after the initial 120-frame warmup; no screenshot readbacks occur during these timed runs. The 1% low is the reciprocal of the mean interval of the slowest 1% of retained frames. A statistical 45 FPS gate does not certify a 45 FPS minimum on every individual frame.

The initial frozen-build acceptance results were:

| View | Retained frames | Average FPS | 1% low FPS | Frames below 45 FPS |
| --- | ---: | ---: | ---: | ---: |
| Frostshade clearing, dragons disabled | 9,480 | 59.95 | 41.71 | 562 |
| Same clearing, complete dragon cycle | 9,480 | 59.95 | 41.45 | 618 |
| Castle overlook, dragons enabled | 2,280 | 59.95 | 44.83 | 12 |
| Full jungle return route | 7,560 | 59.30 | 44.02 | 45 |

All five functional cases passed: every dragon stage was observed, at most one dragon breathed at once, no dragon/VFX load failures or GL errors occurred, all textures released at teardown, the jungle route completed without collision/slope obstruction, and destination travel unloaded the dragon bank. Input metadata and saved preferences were unchanged throughout. The table does **not** pass the 45 FPS low-percentile target. The comparable no-dragon clearing already exhibited dips; adding dragons changed its mean GPU render time from 12.22 to 12.25 ms. This comparison does not certify frame-time neutrality under every view or motion.

The destination-tour case deliberately crosses loading screens: its 57.10 FPS average / 8.92 FPS 1% low includes a 1,480 ms destination transition and is retained as functional evidence, not steady-play FPS evidence. Loading remains visibly gated rather than concealed by dropped timing samples.

Post-timing native named-object inventories measured 2,168,933,729 bytes without dragons and 2,272,748,373 bytes with dragons, a 103,814,644-byte increase (about 99 MiB). These are named texture/buffer storage estimates, not complete driver memory. Aggregate device VRAM peaks were approximately 2.93 / 2.96 GB respectively and include the desktop and other applications.

A separate paired native trial in `pacing-trial/` retained 2,280 measured frames per case. Completing the GPU work before the existing VSync swap improved the Frostshade clearing from 42.48 to 46.47 FPS at the 1% low, and from 68 to zero frames below 45 FPS. Average FPS remained 59.95; the image and saved quality settings were unchanged. The trial used a temporary swap wrapper only in its child test process; the wrapper is not required or used by the final engine.

The production build extends the existing village completion policy only around the two exterior roosts, with 90 m entry / 110 m exit hysteresis. This is independent of dragon visibility or asset residency. The jungle retains its original CPU/GPU overlap, VSync remains unchanged, and every completion wait remains inside measured frame intervals. CPU tests cover exact circle boundaries, diagonal rejection, both clearings, departure/re-entry hysteresis, nonfinite inputs and the remote jungle. Final production-build acceptance results are in `native-final/`.

| Final production build | Retained frames | Average FPS | 1% low FPS | Frames below 45 FPS |
| --- | ---: | ---: | ---: | ---: |
| Frostshade clearing, dragons disabled | 9,480 | 59.95 | 46.52 | 4 |
| Frostshade clearing, full dragon cycle | 9,480 | 59.95 | 48.13 | 0 |
| Hellfire clearing, dragons enabled | 2,280 | 59.95 | 47.54 | 0 |
| Castle overlook, dragons enabled | 2,280 | 59.95 | 44.83 | 11 |
| Full jungle return route | 7,560 | 59.25 | 46.70 | 10 |

These are third-person tests at the saved fixed 70% scale, with clouds, fog, contact lighting, reflections and 2048 shadows enabled. Castle/clearing tests hold the observation camera still while the dragons animate; the jungle test walks the existing approximately 350 m return route. Benchmarks use the dummy audio driver. They do not certify untested camera paths, all areas of the map or every individual frame above 45 FPS. The castle overlook still misses the 45 FPS 1% low gate. The final jungle's slowest interval was 25.52 ms (about 39.2 FPS); the narrow roost policy was never active on that route.

Both dragon clearings meet the measured 45 FPS statistical target. The stronger full-cycle Frostshade result should not be interpreted as dragons increasing performance: the device's background activity and scheduling vary between sequential runs. The retained initial and paired-trial evidence establishes the existing pacing problem and the narrow policy's benefit without claiming every delta is dragon cost.

All six final functional cases passed. The full-cycle test observed all eight stages and at most 112 live breath billboards, with no dragon/VFX failures or GL errors. The jungle retained no dragon pose assets and used zero roost completion frames. The destination tour visited its required stops, unloaded the dragon bank and reloaded on return. Its transition-inclusive 56.96 FPS average / 8.63 FPS 1% low and 1,605 ms slowest interval are recorded as loading evidence, not steady-play performance. Frozen input metadata and saved graphics preferences remained unchanged across every final run; see `native-final/freeze-after.json`.

The saved machine profile remains 1600 × 900 display, fixed 70% internal render scale (1120 × 630), 2048 directional shadows, fog, clouds, contact lighting and reflections enabled. The lower scene resolution is an existing sharpness tradeoff. Native GPU-store inventories are taken after timing, before resource teardown.

## Try it

A later whole-scene stability correction fixes the temporal input filtering and breath confidence metadata. The measurements above precede that correction; newer matched stability captures and FPS results are in the [whole-scene stability report](../../performance/dragon-background-stability/RESULTS.md).

Launch `Launch_Valwood_Dragons.sh` in the project root to open Frostshade's observation clearing. F9 advances the authored demo stops, including both dragon roosts and views over the walls. The destinations list also contains Hellfire, Frostshade and the western-wall flight view. V switches first/third person; F10 opens graphics settings.

The displays have no gameplay damage or building ignition. Dragon combat, sound design and runtime foot IK remain separate work. No additional asset purchase is required to review this sequence.

Original source files and hashes are retained in `assets/purchased/castle_dragons/`; Blender script execution was disabled, and the supplied sources were not modified. The importer and independent validation are in `tools/import_castle_dragons.py`; audit and test results accompany this report.
