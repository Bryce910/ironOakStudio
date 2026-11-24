# Continuous source lava repair

The first native castle capture showed white geometric patches because the static adaptation chose one temperature material independently for each triangle and retained emission peaks up to 14,018. The source flow meshes and their UV frequency were present; the hard face categories and excessive native radiance were the errors.

`tools/prepare_nature_lava_materials.py::vertex_heat` now evaluates the source height, slope and authored color blends at every vertex and returns a continuous temperature. `tools/cook_nature_showcase_lava_smooth.py` preserves the original indices and exports `_VALWOOD_LAVA_HEAT` as float32 SCALAR. Publisher validation confirms all 53,966 source flow triangles plus 216 emissive fixture triangles per LOD remain intact. The original river UV range still repeats 117 times; it was not flattened into a low-density mesh atlas.

`tools/prepare_nature_lava_continuous.py` makes three rows per PBR image from the original cold, medium and hot textures. Rows run cold/medium/hot from GL low V to high V. Base, normal and emission are 1024×3072; ARM is 512×1536. The source project's private foliage fragment samples and blends the rows with interpolated heat. It preserves the original grain and normal detail, adds gentle flow at -0.012 native V tiles per second, and modulates radiance by at most 3%. A small half-texel inset reduces atlas boundary leakage.

Both native model loaders validate the optional heat stream through `engine/world_model_lava_heat.h`: exact float32 scalar count, nonnormalized values in 0..1, no duplicates and no simultaneous architectural paint attribute. Heat is tagged as `-1-heat` in the existing scalar vertex field. Architectural paint remains 0..1. There is no change to vertex stride or GPU geometry memory. Only the private source shader interprets the lava tag.

Emission is deliberately adapted to the native renderer: source crack intensity is normalized and compressed while preserving spatial detail. Cold/medium/hot red peaks are 5/14/24; maximum warm RGB after texture quantization is approximately `(24,8.09,0.022)`. Dark source crust remains unlit between the cracks. Red grilles use `(12,1.4,0.006)` instead of the former much larger factor. This is a bounded native material adaptation, not a complete port of the proprietary UV4 velocity, tessellation, rim and dynamic fluid shader. The original three blend weights are reduced to one interpolated temperature, so an arbitrary simultaneous cold/hot mixture is approximated.

Fifteen CPU layout, range, ambiguity and tag checks pass in `tests/world_model_lava_heat_test.cpp`; both changed loader translation units pass syntax checks. `lava-continuous-map-verification.json` verifies finite HDR maps and no white pixels after the native RGB filmic fit at exposure 0.38. The accompanying emission diagnostic is a texture preview, not an in-game capture. Native appearance and sustained performance remain root's final validation.

Frozen material metadata: `assets/purchased/nature_castle/showcase-lava-continuous/materials.json`.

Frozen geometry catalog: `assets/purchased/nature_castle/showcase_lava_smooth/catalog.json`.
