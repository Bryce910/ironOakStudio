# Grandstone locomotion adapter acceptance

The dedicated movement cast discards nonopposing contacts before selecting the closest hit. This lets the character move away from a slightly overlapped doorway jamb or along a floor while retaining a later wall contact. Its tolerance is one part per million of displacement, with a one-micrometer minimum.

The query retains two-sided triangle collision. It supplies Jolt's documented `mActiveEdgeMovementDirection` hint, which prevents inactive-edge correction from replacing a separating contact normal with a blocking backface normal. Generic `cast_capsule` and `sweep_body` keep their previous query settings and closest-hit behavior.

The geometry bounds are 147,456 vertices and 294,912 triangles per document. They accommodate the expanded source stair geometry while preserving the 512-body and 16 MiB document limits. Parsing, validation and atomic rejection use the same limits.

All tests passed against the compiled adapter:

- 45 targeted checks, including both sides of one triangle-mesh doorway, floor underside head contact, floor seams and a later perpendicular wall.
- 2,400 movement casts with zero C++ allocations and zero Jolt query heap fallback.
- Exact geometry limit codec round-trip; aggregate geometry, body and file-byte overflow rejection; previous documents and scenes preserved after rejection.
- Existing 936 actual Jolt contracts, including 900 frame/query cycles and 120 maximum-capacity frames.
- Generic third-person camera open-boom and thin-wall contraction regression.

CPU reproduction from `new_engine`:

```sh
make -j1 engine/world_physics.o
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_physics_movement_test.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-physics-movement-test
/tmp/valwood-physics-movement-test
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_physics_test.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-physics-regression-test
/tmp/valwood-physics-regression-test
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_follow_camera_physics_test.cpp engine/world_physics.o build/physics/libJolt.a -pthread -o /tmp/valwood-follow-camera-regression-test
/tmp/valwood-follow-camera-regression-test
```

Source hashes at acceptance:

| File | SHA-256 |
| --- | --- |
| `engine/world_physics.h` | `7c380970bfb85bcc4e98d11cf32e16e4faae7bff98003bea60194cdde273b04a` |
| `engine/world_physics.cpp` | `ac806fb393ebc9ba8e60c659f0603259c6a7f232a548208ddc14138bac35fb50` |
| `engine/world_physics.o` | `e0851ba5e0d12636d13c1c30c1aff460474186590210363e3b9566417c0bfb75` |
| `tests/world_physics_movement_test.cpp` | `f962b41de6ed9131b7c68797af0320d3182ef72a89572f68a4743756aec75df4` |

This report covers CPU query behavior. Actual source-sector routes and native rendering are covered by the surrounding cleanup evidence.
