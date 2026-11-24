# Authored landscape and vegetation pass

Continue in the native SDL/OpenGL engine, retaining the 100.8 km map and the
original Seldonne project. The preceding scenery build is the measured visual
and performance baseline, retained under `performance/authored-landscape/before`.

1. Compose three local landscape districts: an asymmetric alpine headwall above
   Ulasse, a Valwood clearing with an actual walking trail, and broken Crosswind
   headlands. Bake bounded local residual height fields; preserve map water,
   settlement foundations and seamless streaming. Expose rock, meadow, moss and
   trail masks for matching materials and vegetation placement.
2. Introduce a rounded broadleaf crown, young open conifers and grounded
   deadwood alongside the existing mature trees. Preserve source leaf/twig UVs
   and provide bounded near/mid/far variants with documented source provenance.
3. Replace uniform dressing around the scenic trail with explicit, editable
   compositions of trees, rock groups, fern beds and deadwood. Keep the path
   clear. Blend forest litter, greener meadow cover and rock/moss into the
   authored landforms rather than covering every slope with the same material.
4. Add depth-reprojected temporal antialiasing with camera cuts, disocclusion
   rejection, neighborhood clamping and conservative history for animated
   foliage/water. Keep actual animation in repeatable performance checks.
5. Inspect native stills and movement before measuring the final build. Check
   the full test suite, source/asset budgets, actual mixed-LOD skirts, resource
   cleanup and native Radeon frame times at 1600×900. Retain a feature only if
   its visual benefit and observed 45–60 FPS behavior justify its cost.

The goal of this pass is a visibly more intentional landscape with richer
plant silhouettes and steadier edges. Production AAA fidelity still requires
continued art direction, authored content and broad hardware/weather testing;
it cannot be established by a feature list or an average frame rate alone.
