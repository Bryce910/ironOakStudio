# Grandstone mountain citadel — 1 November 2026

This pass replaces the previous isolated entrance and low hall with a composed snowy mountain district using the purchased NatureManufacture Modular Castle & Dungeon kit. The original package and previous stronghold document are retained.

## Implemented

- Monumental mountain gateway, paired guardian statues, buttress towers, carved facades, and a layered mountain skyline.
- Lower defensive ward with a second gate, clan lodgings, market arcade, barracks court, workshop dressing, and resident patrols.
- A 244 m great hall with repeated supported vaults, stone floors, columns, galleries, banners, carved figures, and lighting.
- Two connected, furnished forge arcades and a royal sanctuary with guardians, ceremonial stairs, carved architectural backing, a new bevelled stone-and-bronze throne, and the existing dwarf king.
- Retained frozen waterfall and basin, textured winter granite, snowbanks, snow-covered conifers, powder imprints, and snow footstep effects. Indoor snow and precipitation are suppressed under the inhabited mountain roof. Upper aisle walls and a high excavation ceiling conceal exterior skyline foundations from the hall.
- 982 placed objects, including 495 purchased-kit placements; 30 existing dwarf residents; 24 authored lights and 16 emitters. The bounded palette contains 86 asset definitions.
- Measured ground-level wall collision for the kit, preserving usable openings. Construction pads support buildings and stair approaches. A planned 1.6 km return route visits the town, market, hall, both forge wings, and royal stairs.

## Asset and runtime work

39 source prefabs were exported with their authored assemblies and two source detail levels. A mirrored complete arcade variant provides a matching doorway for the right forge wing. The derived throne retains the original collider bounds and merges its 6,768 triangles into three material batches. Material cooking preserves source grain and shape detail across the kit's two UV sets. The native variants use 1K base/normal maps and 512-pixel ARM maps to bound memory on the 3 GB RX 5300; full purchased source assets remain available for later quality tiers.

The editor palette limit is 128 assets. Live runtime capacity now reserves 64 entities beyond a full 1024-node authoring graph, allowing built-in journey actors and markers to coexist with large environments. A regression test covers that exact boundary and explicit overflow rejection.

## Validation

- Scene persistence, palette capacity, and atomic malformed-document rejection: passed (`make test-world-studio`).
- Connected live systems, real crate support, journey dialogue, full-scene capacity, and persistent-world preservation: passed (`make test-world-live-components`).
- Conservative collision-grid route check: no occupied route segments. Native walking validation remains the authoritative check.
- Final native rehearsal: **passed**, all 55 route segments and **1,597.78 m** completed, with zero collision blocks, slope blocks, sliding steps, or return error. Thirty residents and 48 pose assets loaded without failures.
- **28,380 measured frames** at 1600×900 output with **70% internal render scale (1120×630)**: **59.54 FPS average**, **51.07 FPS 1% low**, **26.025 ms maximum**. Four individual frames exceeded the 45 FPS budget; the strict 45 FPS floor is therefore **not** claimed. Adaptive quality was disabled for this fixed-scale measurement.
- Powder snow: 1,209 alternating footprints, 7,254 powder puffs, 1,209 audio steps, and 3.8 cm deepest imprint; no snow, actor, weather, terrain-streaming, or GL failures. No texture references remained after teardown.
- Peak aggregate VRAM usage was 2,907,688,960 bytes; this includes other desktop GPU users and is not a per-game allocation figure.
- The installed document, binary, shaders, and derived models exactly match the final rehearsal fingerprints. `applied.json` records that comparison. Native checks hash graphics preferences and player saves and leave them unchanged.
- Full-resolution stationary captures are visual checks with short timing samples. The hall remains more demanding at full scale than at 70%; retain adaptive quality on this RX 5300. The 59.54 FPS figure is the sustained fixed-70% route, not a full-resolution benchmark.

The earlier `full-walk` run predates the final cave seal, royal facade placement, and throne. Use `final-full-walk/result.json` as the authoritative sustained result. Older `final-*`, `release-*`, and `sealed-*` captures are intermediate reviews. `production-town` and `production-hall` are reload checks of the installed project, without a candidate override. Both match the final rehearsal document, executable, shaders, and derived model hashes, and preserve protected preferences/saves.

| Installed view | Scale | Average FPS | 1% low FPS | Sample scope |
| --- | --- | --- | --- | --- |
| Full return route | 70% | 59.54 | 51.07 | 28,380 measured frames |
| Town | 100% | 52.42 | 46.74 | 160 measured stationary frames |
| Hall | 100% | 42.48 | 38.55 | 160 measured stationary frames |

The full-resolution hall is below the 45 FPS target on this GPU. The saved adaptive-quality setting is retained; graphics preferences were not silently lowered.

## Run and maintain

Launch `/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold.sh`; it opens directly in the lower town. The stronghold editor launcher opens the same project.

The generated pass is maintained by `tools/build_nature_grandstone.py`. Its default mode applies the production document and updates project metadata; `--output` writes a candidate document. It always composes from the retained pre-citadel baseline. Do not rerun the old `build_dwarf_stronghold.py` over this scene.

`tools/check_nature_grandstone.py` runs native GPU captures or the complete walking rehearsal with isolated progress and hashes of protected preferences/saves. Captures use the actual AMD renderer and explicitly verify that the requested candidate document loaded.

## Current scope

The upper towers and galleries establish the skyline and interior composition; elevated floor traversal is not implemented. This pass does not replace the hero or dwarf character artwork. It is a substantial environment pass, not a claim of finished AAA production quality or a frame-rate guarantee. Full-resolution performance and adaptive-resolution performance must be read separately. Existing player saves and graphics preferences are preserved by the checks.
