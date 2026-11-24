# Model instance buffer streaming

Production static/fading and GPU pose-blend uploads use the established `glBufferData(live_count * 24, live_data, GL_DYNAMIC_DRAW)` path. A proposed orphan-plus-subdata policy passed geometry correctness checks but caused severe stalls on the user's AMD Radeon RX 5300, so it was rejected as the production default.

The isolated full 10,320-frame route comparison retained every sample after 120 warmup frames:

| Upload policy | Average FPS | Slowest 1% FPS | Worst frame | Frames over 22.222 ms |
| --- | ---: | ---: | ---: | ---: |
| Experimental orphan + subdata | 46.80 | 3.63 | 583.028 ms | 1,321 |
| Exact-sized dynamic upload | 59.68 | 36.16 | 394.059 ms | 34 |

Evidence is retained in `native-final/route-castle-west-street.json` and `diagnostic-exact-instances/route-castle-west-street.json`, with complete CSVs and variant/source-freeze metadata. The exact policy removed most stalls in this comparison, but its 1% low still falls below the required 45 FPS. Neither result establishes completion of the performance target, and correctness parity does not imply a performance improvement.

With `VALWOOD_QA_EXACT_INSTANCE_BUFFERS` unset, or set to `1`, each new asset/pose batch selects exact-sized dynamic uploads. Setting it to `0` explicitly enables the experimental policy for isolated driver comparisons. The choice is latched at creation; changing the variable later does not change an existing stream. The experimental path retains a power-of-two instance capacity from 16 to 4096, orphans storage with `glBufferData(capacity * 24, nullptr, GL_STREAM_DRAW)`, then copies the live prefix with `glBufferSubData`. Its capacity grows without shrinking; its largest store is 98,304 bytes. This path is retained for diagnostics only.

Both policies preserve geometry, materials, instances, ordering, fades, weights, VAO IDs and VAO bindings. CPU staging remains reserved at creation; uploads add no caller-module C++ allocation.

The caller's declared pose-batch capacity remains its exact validation limit even when the GPU capacity class is larger. All instance/fade/weight values are validated before staging or GL calls. Invalid input therefore preserves previous GPU contents, counts, fade mode and GL bindings. Empty uploads retain allocated storage and clear the draw count as before. Driver errors retain the existing failure contract; this change does not add an out-of-memory transaction mechanism.

No new public API was added; this renderer had no instance-capacity GPU byte accounting API to update.

`tests/world_instance_stream_gl_test.cpp` first unsets the override and verifies the actual production default: static, fading and pose uploads each use a 17 × 24-byte `GL_DYNAMIC_DRAW` store with exact live-byte readbacks, even after the environment changes following construction. It then exercises both explicit policies against temporary, generated triangle GLBs. It checks actual GL buffer sizes, usage and live-byte readbacks; orphan/subdata calls and lengths; stable instance-buffer and VAO IDs; actual transformed positions, fades, yaw and pose weights through transform feedback; zero caller-module C++ allocation; retained clears/shrinks/refills; and late-invalid transactions. The allocation guard is thread-local and excludes actual GL entry calls through scoped wrappers, so Mesa's internal C++ allocations and other driver threads are not attributed to module staging. A positive controlled allocation outside GL proves that the guard still detects module-side allocations. The complete transformed output hash must match byte for byte between both policies. A 17-instance batch also verifies that a 32-slot GPU class does not permit an 18-instance input.

The fixture is compiled with strict warnings. Native GPU execution and isolated frame-time measurements are owned by the central acceptance runner. Software correctness execution makes no native frame-time claim.

Compile the fixture without changing the engine executable:

```sh
gcc -std=c99 -O2 -Wall -Wextra -Werror -I. -c engine/texture_cache.c -o /tmp/demo-finish-stream-cache.o
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. $(pkg-config --cflags sdl2) tests/world_instance_stream_gl_test.cpp engine/world_models.cpp /tmp/demo-finish-stream-cache.o -Wl,--wrap=glBufferData -Wl,--wrap=glBufferSubData -Wl,--wrap=glDrawElementsInstanced -Wl,--wrap=glUseProgram -Wl,--wrap=glBindBuffer -Wl,--wrap=glGetError -o /tmp/valwood-instance-stream-gl-test $(pkg-config --libs sdl2) -lGL -lm
```
