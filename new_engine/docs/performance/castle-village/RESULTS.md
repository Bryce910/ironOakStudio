# Valdoa castle village — native build results

The new walkable settlement sits at the castle landmark on the supplied Valwood map, inside the owner’s existing native engine. It is an authored village prototype, with ambient NPC activity and an initial architecture kit. It does not establish finished AAA art quality.

Run `Launch_Valwood_Castle_Village.sh` from the project root. It opens in the marketplace at 1600×900. WASD walks, right mouse looks, Shift sprints, H shows tools, F enables flight and Tab opens the atlas. The Scenic trails list provides arrival, market, booths and castle courtyard destinations.

## Built area

- A 410×410 m outer enclosure, two open gates, twelve towers and a 35 m keep in a separate northern castle court.
- Forty homes, an inn, smithy, twelve stocked market booths, a well, carts, cargo, barrels, benches, banners and gardens. Inner homes face the avenue. Ground-floor doorways have measured wall-piece collision; the keep’s upper floors and wall walks are not an explorable interior system.
- 54 ambient residents: 8 knights with swords/shields, 8 ground-level archers with bows/quivers/arrows, 6 traders and 32 travelers. Guards/travelers follow scripted routes; traders/archers idle. People reuse 32 baked pose files and three 1024 px images. Knight walking uses a supplied clip; traveler movement uses authored mesh deformation. Animation is stepped, and styles remain mixed.
- 783 new authored placements, sharing the existing renderer’s instancing, LODs, shadow proxies, PBR lighting, contact shading and temporal AA. Procedural boulders/ferns are excluded from the built terrace; authored trees and gardens remain.

## Measured native performance

AMD Radeon RX 5300, visible 1600×900 window, 60 FPS target. Profiles exclude 120 warmup frames. Other game windows and background cook/build jobs were stopped.

| View | Measured frames | Mean FPS | 1% low FPS | p99 ms | Max ms | GPU mean ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| route | 14,040 | 59.95 | 58.71 | 16.721 | 23.739 | 12.768 |
| approach | 1,680 | 59.95 | 58.61 | 16.720 | 20.038 | 12.490 |
| market | 1,680 | 59.95 | 59.75 | 16.719 | 16.786 | 13.131 |
| stalls | 1,680 | 59.95 | 59.29 | 16.723 | 17.614 | 12.811 |
| courtyard | 1,680 | 59.95 | 59.57 | 16.735 | 17.326 | 12.105 |

Across 20,760 measured village frames, 3 exceeded the 22.22 ms / 45 FPS budget (99.986% within budget). The longest frame was 23.739 ms, so this is not a guarantee of a 45 FPS minimum on every frame or at other resolutions. All four stationary sweeps stayed within the 45 FPS budget.

The 806 m walking loop completed in 13,818 ordinary-speed simulation frames (230.3 s at 3.5 m/s). It returned to its start with zero error, blocked steps, slope stops, sliding or travel-loading holds. The only route waits were the 120 intentional warmup frames.

The existing rainforest-pool sweep also ran on the final engine: 55.29 FPS mean, 45.76 FPS 1% low, 21.400 ms p99. Its full weather, water/reflection and shadow checks remain enabled.

## Verification and scope

The full existing suite passed, followed by focused final cover, village terrain, actual-bounds scenery, camera, launch and population tests. Eleven real Radeon correctness fixtures passed. Every final village run reports zero GL errors, failed terrain tiles, loading holds and remaining texture owners.

CPU checks include 4,836 three-lane capsule sweeps over the route, physical doorway/header/back-wall tests, ten minutes / 129,600 resident probes, deterministic culling and zero application hot-path allocations in the population fixture. The actual GL fixture rejects accidental materialless rig helpers, checks the 1.82 m source body, animated pixels, shadows, reactive alpha and cleanup. Native captures were reviewed after removing helper geometry, correcting inverted building normals, tightening the market and clearing procedural rocks from urban yards.

All 2,611 earlier placement records remain byte-identical, along with 35 authored Seldonne files and the supplied map. Compact village terrain changes end 280 m from the town center; 3,536 exterior samples and the retained global geography/climate golden checks pass.

The engine, executable, shaders, runtime models and scenery stayed byte-identical through the final profiles. A subsequent rainforest validation-only assertion was corrected from six to fourteen authored shadow proxies, accounting for eight village families; the exact single-file difference is retained in `validation-only-change.json`. Both profiled and final 318-file manifests are retained. No launch arguments or render inputs changed.

The primary remaining production work is architectural/character variety, richer street surfaces and dressing, distant vegetation quality, animation interpolation and full NPC interaction. Current NPCs provide ambient activity; this pass adds no economy, dialogue/combat system or offline AI simulation.

Model provenance/budgets: [architecture](../../CASTLE_VILLAGE_ASSETS.md), [population](../../VILLAGE_POPULATION.md). Renderer statistics and route details: [summary.json](summary.json). Captures are direct framebuffer BMPs converted to PNG; no retouching.

![Market](market-view.png)

![Castle courtyard](courtyard-view.png)
