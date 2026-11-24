# Poly Haven assets

The following downloaded assets are published by Poly Haven under CC0. Source URLs and SHA-256 hashes are recorded in `assets/polyhaven/manifest.json` and `assets/polyhaven/models/manifest.json`. Original model download metadata is retained next to each source. These license records apply to the newly downloaded assets; owner-supplied legacy content retains its original provenance.

| Asset | Use | Source |
| --- | --- | --- |
| Leafy Grass | 2K grass albedo, OpenGL normal and packed AO/roughness/metallic | https://polyhaven.com/a/leafy_grass |
| Forest Ground 04 | 2K woodland earth material | https://polyhaven.com/a/forest_ground_04 |
| Rock Boulder Dry | 2K triplanar rock material | https://polyhaven.com/a/rock_boulder_dry |
| Aerial Grass Rock | Retained alternative ground material | https://polyhaven.com/a/aerial_grass_rock |
| Fir Sapling Medium | Source for 65k-triangle runtime fir and rendered distant impostor | https://polyhaven.com/a/fir_sapling_medium |
| Fern 02 | Three runtime woodland fern variants | https://polyhaven.com/a/fern_02 |
| Rock Moss Set 02 | Three reduced runtime boulder variants | https://polyhaven.com/a/rock_moss_set_02 |

Credits: Charlotte Baglioni (Leafy Grass); Rob Tuytel and Rico Cilliers (Forest Ground 04, Fir Sapling Medium, Fern 02); Kless Gyzen (Rock Moss Set 02); Dimitrios Savva and Rico Cilliers (Rock Boulder Dry); Rob Tuytel (Aerial Grass Rock).

License: https://polyhaven.com/license (CC0 public-domain dedication).

`tools/build_forest_assets.py` reproduces the model conversion, UV-transform baking and impostor rendering from the saved source files using Blender 4.0.2. `projects/seldonne/cooked_forest_manifest.json` records source and runtime triangle counts. Source geometry is retained for future LOD refinement.
