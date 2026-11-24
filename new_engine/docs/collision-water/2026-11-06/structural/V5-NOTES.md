# Source-v5 exact source reconstruction

The staged collision cook now publishes successfully after Undo. Its strict comparison against the original startup fingerprint exposed a separate renderer transaction precision loss: the scene writer converted stored float radians through a float degree field before parsing them back to radians.

The transaction writer now retains yaw, pitch and roll in double precision until the existing renderer parser converts degrees to its float radian fields. No scene format, source asset, geometry budget or fingerprint tolerance changed. Only the in-memory transaction record type and the editor writer's angle parameters changed; existing aggregate consumers remain compatible.

[The strict real-row regression](v5-precision/CPU-RESULTS.json) verifies all 5,327 retained City rows and all 121 Grandstone rows. The previous degree intermediate changed 1,988 City source rotations, including 39 physical instances. The corrected writer changes zero pose fields and preserves identical hashes of all 187 City and 102 Grandstone physical poses. Yaw, pitch, roll, signed zero, positions, pivot, scale and axis factors are compared exactly.

The actual scenery loader and `world_scenery_prepare_files` additionally retain every exact pose field across all 187 City and 102 Grandstone physical source rows, with the model/GL boundaries stubbed for CPU execution.

The existing actual CAPI/parser explicit-pivot regression also passes all 37 checks against the retained Grandstone source area. Its model/GL boundaries are CPU stubs and no graphics context is created. It confirms elevated imported roots, ordinary terrain grounding, provenance, repeated transactions and release behavior remain compatible.

Undo QA retains its exact original Capture and prints the first exact original/restored components if a future mismatch occurs. Source fingerprints remain strict.

The v5 build helper derives from the successful immutable v4 native and Windows graphs. It pins the exact reviewed source changes, the versioned water patch and legacy payload, and the unchanged legacy patch helper. Native and Windows inspections and final viewer syntax passed with the root-owned nearest-pond reflection selection and authored-pond startup prewarming included. The root owns final inspection/freeze/build and actual GPU/Windows acceptance.
