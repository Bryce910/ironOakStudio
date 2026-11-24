# Bridge LOD coverage diagnosis

The native held bridge view has a real color-pass LOD coverage artifact, visible in `route-smoke-v2/checkpoint-26-barbican_bridge.png` around pixels (530–650, 680–800). This is separate from the close stone panel material audit.

The source-specific selection uses a 31–39 m three-dimensional surface-distance crossfade. The fragment rank is fixed to `floor(gl_FragCoord.xy)` for both representations. That gives exact complementary coverage only where the two LODs overlap the same ray at essentially the same distance. The source bridge geometry does not satisfy that assumption.

CPU rays through the captured camera, using exact purchased scene transforms and geometry, show:

| Display pixel | Near source surface | Distance | Near coverage | Nearest MID surface |
| --- | --- | ---: | ---: | --- |
| 620,720 | Buttress_level0_00_j_LOD0 | 36.263 m | 27.1% | Different ornament at 55.489 m |
| 610,680 | Buttress_level0_00_j_LOD0 | 37.642 m | 7.7% | Different buttress at 53.805 m |
| 580,750 | stone_element_28c_wall_line_x3_battlements_LOD0 | 35.153 m | 47.1% | Different wall element at 45.154 m |
| 530,790 | stone_element_28c_wall_line_x3_battlements_LOD0 | 33.439 m | 77.8% | Rock at 50.682 m |
| 650,800 | barbican_floor_01a, both tiers | 35.735 m | 36.4% | Identical surface at 35.735 m |

Thus ordinary floor coverage remains complete while the post/rail expose distant geometry through a fixed dither. More loading or more held frames cannot fix a rank that never changes. The ray audit uses source geometry rather than native frame-buffer readback; it diagnoses the geometry/fade relationship and is not a pixel-exact GPU rasterizer.

`source-lod-ray-audit.json` records the rays, actual source hierarchy, geometry hashes and competing hits. `source-lod-unpaired-unit-audit.json` records logical units without both tiers under the current cooker's name/matrix pairing. There are 1,777 near-only units (1,313,587 triangles; 1,085,262 vertices). Some source LODGroups replace several detailed objects with differently named coarse geometry, so this count alone does not establish missing geometry in the exporter. Blanket near-to-MID promotion would cost approximately 86 MiB of vertex/index data plus shadow work and must not be adopted merely to hide dither.

## Isolated temporal candidate

`source-lod-temporal-candidate/world_foliage.fs.glsl` rotates only the source architecture LOD rank by the existing eight-phase `uTemporalPhase`. Use `VALWOOD_QA_FOLIAGE_FRAGMENT` to test it without changing the project or rebuilding. Vegetation's LOD/alpha ranks, source shadows, lighting and materials remain unchanged. When TAA is disabled the existing phase is zero, preserving the original rank.

The candidate does **not** repair missing opaque coverage: absent/different source counterparts become a temporally resolved crossfade instead of fixed screen stipple. Judge both close opaque coverage and frame-to-frame stability, including movement, before adopting it. The source LODGroup audit is required to decide whether a particular post should instead keep its source near geometry in MID.

CPU verification covered 257,257 rank/weight combinations across all eight phases: near/MID decisions remained complementary; the maximum eight-frame area-coverage quantization error was .124. This is a mathematical check, not native image acceptance. Production shader and candidate hashes are in `source-lod-temporal-candidate/candidate.json`.

No production files were changed and no GPU work was launched for this diagnosis.

## Source proof and targeted continuity candidate

Publisher audit subsequently resolved the original `Prefab_Main_Bridge.prefab` LODGroup: renderer counts 52/52/32, thresholds .9260567/.10299375/.041519083, parent size 48.456024 m. Native MID selected source tier 2. Its 32 renderer IDs are a literal subset of source LOD0: all 8 `Buttress_level0_00_j` posts and 12 `stone_element_28c_wall_line_x3_battlements` rails are omitted with no coarse replacement. Source fileID/GUID evidence is in `source-bridge-lod-proof.json`. This is an early native tier/scale mismatch, not missing purchased geometry.

`tools/prepare_showcase_mid_continuity.py` stages only those 20 structural components into the existing MID files. It matches the source triangles to current native HIGH geometry, then copies exact native position/normal/UV/tangent/paint/GI values into MID, preserving every HIGH file and all original MID binary bytes. Materials and embedded images remain identical. This also preserves the selected 2K pillar material pass. Existing native shader ranks now have exactly coincident source geometry on both sides for these posts/rails, restoring opaque complementary coverage without temporal noise.

Candidate full 117-chunk catalog: `assets/purchased/nature_castle/showcase-bridge-mid-continuity/catalog.json`. Three MID files add 18,720 triangles / 13,644 vertices, totaling 1,152,432 bytes of native geometry. No new images or texture budget are required. Largest MID GLB is 108.622 MiB; maximum candidate CPU retention is 536.55 MiB under the unchanged 768 MiB preparation cap. All three files passed the actual native CPU loader; added SCALAR index counts, exact triangle matching, GI finiteness, geometry/file caps and shared floor alignment were verified. `report.json` and `cpu-validation.json` record exact paths and hashes. No production import or GPU acceptance is implied; the material agent will merge the separate panel UV repair onto this candidate before native bridge capture.
