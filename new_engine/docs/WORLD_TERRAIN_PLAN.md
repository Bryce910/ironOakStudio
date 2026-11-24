# Valwood map terrain implementation plan

## Current scenery pass

The later cinematic pass keeps this geography/scale contract while adding mature source-derived tree LODs, photographed grass/rocks/cliffs, more detailed alpine shaping,64-cell terrain meshes, macro terrain shadows, contact shading and planar lake reflections. Four scenic presets and H-to-hide-tools are available. See [scenery results](performance/cinematic-scenery/RESULTS.md) and [source assets](CINEMATIC_ASSET_SOURCES.md). Performance values in the original terrain-foundation section below describe its earlier renderer.

## Objective and scope

Build the supplied Valwood map's base geography in the owner's native engine: coastlines, islands, waterways, mountain ranges, uplands and regional terrain materials. Use a separate `projects/valwood_world` project. Preserve the playable Seldonne sample, its authored landscape, props, masks and saves.

This milestone establishes a traversable terrain foundation and inspection workflow. Mapped settlement names identify geographic anchors; they do not establish that full settlements, quests or the living-world simulation have been built. The original image supplies relative geography. Physical dimensions and terrain elevations are production decisions rather than facts stated by that image or the lore document.

## Scale and coordinate contract

- One world unit represents one metre. The map spans **100,800 m west to east** and **56,708.804 m north to south**, preserving the source image's 5009:2818 aspect ratio.
- Standard walking speed is **3.5 m/s**. An uninterrupted straight crossing of the full west/east width therefore takes `100800 / 3.5 / 3600 = 8 hours`.
- This is a width/speed reference, not a measured eight-hour overland route. Terrain slopes, detours, rivers and coastlines affect actual journeys. Preview flight and teleportation are inspection tools and do not change the reference walking speed.
- Origin is the map centre, positive X is east and positive Z is south. Normalized image coordinates convert as `x=(u-0.5)*100800`, `z=(v-0.5)*56708.804`. North is negative Z.
- The runtime atlas resolves landmark anchors onto safe local terrain. The manifest's initial Seldonne reference uses image coordinate `(0.384, 0.611)`; the runtime may adjust it to nearby dry ground.

## Implementation sequence

1. Create the isolated project and retain the original JPEG at `assets/map.jpg`, with an engine source copy under `assets/reference`. Link supporting shader, font and terrain material files individually. Keep project authoring directories and saves local.
2. Introduce the `terrain=valwood_atlas` manifest profile and branch into the atlas terrain runtime. Keep legacy manifest bounds and Seldonne behavior intact. Do not stretch its 512² sculpt or paint maps across the new world.
3. Define coastlines and waterways from the reference, then layer regional elevations: Great Mashito Mountains and northern snow, Valwood lake/woodland, Old Tanthoon lowlands, Wallows escarpment, Kerone plains and Crosswind coast.
4. Render bounded terrain detail around the camera with distant terrain LODs. Terrain queries, visible geometry, collision height and water classification must agree. Separate large terrain coordinates from camera-relative rendering where needed for stable geometry and shadows.
5. Add project-aware landmarks, an overview and a tour/teleport workflow. Permit immediate inspection of Callieut, Seldonne, Valdoa, the Wallows, Kerone and the southern coast without an hours-long test journey.
6. Apply regional material blending, atmospheric perspective, stable lighting, water and locally bounded vegetation. Validate actual gameplay and moving-camera renders; the size of the map alone does not establish AAA fidelity.
7. Verify source preservation, generation determinism, terrain continuity, safe spawn/travel, launch options, rendering correctness and native frame-time/memory budgets. Record evidence and remaining limitations separately from this initial plan.

## Project and launch contract

`tools/build_valwood_world.py` creates missing files and preserves existing project content by default. `--reset` refreshes its generated metadata/reference copies while retaining saves and authoring. It does not erase or rebuild Seldonne. The manifest opts into atlas bounds; manifests without a terrain profile retain the legacy ±245 spawn validation.

From the enclosing `final_engine` folder:

```sh
./Launch_Valwood_World.sh
./Launch_Valwood_World.sh --landmark callieut
./Launch_Valwood_World.sh --tour
```

The launcher shares the existing game's instance lock. It focuses an existing Valwood window when the GPU is already serving a launcher-managed session. The CLI accepts safe landmark identifiers; the atlas runtime validates whether a supplied name exists.

The atlas inspection controls are WASD movement, right-drag mouse look and Shift sprint. Tab opens the atlas; the panel exposes all 15 landmark buttons and keys 1–9 select the first nine. F toggles preview flight, with Q/E vertical movement. Escape closes the atlas and releases the mouse. Preview flight and the automatic tour are explicitly separate from standard walking.

For repeatable captures and profiling, invoke the engine directly after closing the running game:

```sh
./rpg --project projects/valwood_world --play --landmark seldonne --frames 240 --capture /tmp/valwood-world-seldonne.bmp
./rpg --project projects/valwood_world --play --tour --warmup 120 --frames 3600 --width 1600 --height 900 --profile docs/performance/local-world-tour.json
```

The Seldonne benchmark script remains specific to that sample. A world tour evaluates a different workload and must be labeled separately. A capture is renderer evidence; it is not an authored replacement image.

## Verification gates

- Hash Seldonne's authored/configuration files before and after project preparation and atlas checks. Existing project/import/save tests and an old-project visual check must remain valid.
- Check deterministic finite heights, bounds and axis mapping. Coast/water classification and terrain must agree at both shores and confluences. Check shared cell edges and LOD transitions for cracks, missing tiles and height discontinuities.
- Inspect representative landmarks at walking height and from above: Seldonne/Old Tanthoon, Callieut/Brandalhall, Valdoa/Valwood lake, Wallows/Gorshett, Kerone/Etarma, Fallaway/Crosswind coast and Bellweather.
- Teleports must place the camera/player above valid local terrain, clear stale movement state and refresh local terrain/vegetation work. Local collision must follow the rendered surface.
- Check low sun, overcast and night lighting, long-distance silhouettes, shore blending, texture tiling and vegetation in motion. Record which conditions were actually exercised.
- Measure visible native Radeon RX 5300 runs at 1600×900, with one engine process, fixed seed/lighting and a documented warm-up. Report average, p95/p99, maximum, 1% low, percentage within the 22.222 ms/45 FPS budget and resource bounds. Offscreen rendering establishes correctness, not native GPU performance.
- Verify the width/speed calculation and coordinate scale directly. Do not claim an actual eight-hour journey was completed unless it was measured.

## Implemented terrain foundation

The native atlas runtime, regional materials, water, local foliage, first-person walking, preview flight, map markers and landmark controls are implemented. The original Seldonne project remains intact and separate; its detailed buildings/authoring tools are not yet merged into this world. This runtime is for reviewing base geography, rather than a completed campaign or expanded authoring workspace.

The atlas uses about 6.2 MiB of immutable macro data. One worker produces a bounded 252-record terrain tree with 198 target leaves, within 256 slots. Near grids sample every 4 m; seven LOD levels cover a 15 km atmospheric horizon. Mixed-LOD boundaries use consistent height/normal sampling and closed skirts, including at least 96 m skirt depth in the two far levels. GL uploads stay on the render thread and are limited to two completed tiles per normal frame. Existing coverage remains until complete sibling groups are ready.

Long landmark jumps retain the previous image until all coarse coverage and underfoot LOD0 are ready. Walking pauses while this target prepares. Ordinary local movement uses a shared, frame-rate-independent slope check. Terrain positions use double-precision global coordinates and local float GPU vertices. Native input testing confirmed about 4.20 m in 1.20 s.

Vegetation remains local and bounded: at most 22,000 short nine-leaf grass tufts, 80 rendered trees and small rock/fern fields. Placement regeneration is staged across frames; full-resolution static assets reuse the existing texture cache. Water is currently traversable at a sea-level collision surface for inspection. Bridges, ferries and full swimming gameplay are not implemented, so the eight-hour figure remains reference-width arithmetic rather than a certified overland route.

The full suite and targeted sanitizer checks pass. Native frame profiles, captures, terrain budgets, loading holds and remaining limitations are recorded in [the world results](performance/world-terrain/RESULTS.md).
