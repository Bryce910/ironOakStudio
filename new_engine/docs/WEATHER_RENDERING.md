# Rain, ground mist and wind

The native OpenGL 3.3 renderer adds bounded world-space rain and ground splashes,
plus a low ground-fog pass with shadowed sunlight scattering. These run after
contact/ambient occlusion and before the existing temporal resolve. The incoming
scene depth and indirect-light attachment remain untouched.

This is a real-time approximation with fixed budgets. It does not simulate
cloud microphysics, water accumulation, volumetric multiple scattering or
particle collisions against a complete world mesh. Rain uses terrain/water
support and the visible scene depth; roof and shelter simulation would need a
separate world representation. The feature does not establish AAA quality or
guarantee a frame rate without native measurements.

## Rendering and limits

| Part | Implementation | Bound |
| --- | --- | --- |
| Rain | World-cell seeded 3D streak quads, continuously advected and tilted by world-X/Z wind; soft rejection against scene depth | 4,096 submitted instances |
| Splashes | Short-lived expanding ground rings on the terrain/water support plane, rejected on steep support or behind visible geometry | 768 submitted instances |
| Ground mist | Eight integration steps through a terrain-relative height layer, low-frequency density variation and one shadow comparison per step inside the sun map | Half-resolution field; maximum 1,200 m ray extent |
| Composition | Four neighboring fog samples with depth separation weights | One full-resolution pass |
| Surface support | 65 × 65 cached heights covering 192 × 192 m, on 3 m cells; recentered on 32 m anchors | At most eight rows, or 520 requested surface samples per update |

Intensity controls a deterministic subset of rain and splash instances, rather
than increasing their caps. Drops wrap at the bounds of a moving world volume;
they are not a camera-locked screen overlay. The surface cache publishes only
complete generations. While a new generation is staged, sampling outside the
old cache falls back to the existing macro-height texture. A teleport may
therefore briefly use coarser surface support. The local water cap can supply an
exact pool plane to avoid a coarse height interpolation lifting splashes above
the water. Its conservative dry-bank portion lies below real land and is hidden
by the scene depth.

Ground fog attenuates scene light and integrates a colored ambient contribution
plus shadowed sun scattering. It supplements the older distant aerial haze;
clear sky pixels remain unchanged by this pass. It is not global illumination.
Eight steps and a single shadow tap can undersample sharp light shafts or small
occluders. The macro-height fallback also cannot describe every nearby hollow.
Those quality limits need visual review in motion.

Rain and splashes blend ordinary coverage into HDR color while reducing the
scene alpha used for temporal stability. Fog preserves this alpha. There are no
particle motion vectors: reducing history is a conservative anti-ghosting
measure, rather than exact reprojection of moving droplets.

## Integration API

The public interface is in [world_weather.h](../engine/world_weather.h):

```cpp
WorldWeather *weather = world_weather_create("shaders");
// Fill a WorldWeatherFrame with matching camera, jitter, scene textures,
// lighting, shadow VP, wind and optional local-water support.
world_weather_update(weather, frame, 8); // CPU staging, before scene drawing
frame.color = color_after_contact_ao;
frame.depth = scene_depth;
unsigned color_for_temporal = world_weather_resolve(weather, frame);
// ... TAA, tone mapping and UI ...
world_weather_free(weather);
```

The frame's basis vectors are orthonormal, with `forward` pointing into the
scene. Camera coordinates are doubles; the shadow matrix maps camera-relative
world positions. Its shadow texture uses ordinary normalized depth with texture
comparison disabled. The jitter NDC must match the scene color/depth pair.
Wind is world-X/Z meters per second. An optional stable height callback should
return the greater of terrain height and active local-water surface height.
It is called only during bounded CPU staging. The statistics count requested
surface samples; a callback may make additional internal atlas queries.

Both intensity zero **and** fog strength zero bypass all resolve work and return
the caller's color texture. Setting rain to zero alone preserves mist. Creation
still compiles shaders and creates the module object/VAO; size-dependent image
targets are first allocated on active resolve. The five shader files must be
available relative to the provided shader directory, including when the game
changes its working directory to the selected project.

Active resolve leaves its result framebuffer, program and VAO bound. It sets the
full viewport and fill rasterization, enables all color writes, and disables
depth writes, depth testing, blending, face culling and scissor testing. The next
renderer must establish its own state. There are no production GL readbacks or
per-frame GL state queries. Resize is bounded and release deletes all owned
textures, framebuffers, programs and the VAO. The caller retains its input
textures. Invalid input, feedback from an owned texture or failed target setup
returns the caller's source texture.

At 1,600 × 900, image payload is **14,416,900 bytes (approximately 13.75 MiB)**:
one full-resolution RGBA16F result, one half-resolution RGBA16F field and one
65² R32F cache. This excludes driver bookkeeping and shader programs and is not
a measurement of whole-process VRAM. The CPU keeps one fixed 16,900-byte height
array. No rain/splash instance buffer or per-frame particle upload is required.

## Focused verification

From the engine root:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  $(pkg-config --cflags sdl2) tests/world_weather_gl_test.cpp \
  engine/world_weather.cpp -lSDL2 -lGL -o /tmp/valwood-world-weather-gl-test
/tmp/valwood-world-weather-gl-test "$PWD"
```

The fixture supplies a controlled terrain callback and `world_atlas_height`
stub, so it must not also link the atlas implementation. By default it uses
SDL's software offscreen GL path; `VALWOOD_TEST_NATIVE_GL=1` preserves a caller's
native display/driver. The test renders the actual module and shader files. It
checks finite output, depth-hidden rain, rejection below raised terrain, pool
plane override, frozen-frame determinism, wind/time response, temporal-alpha
reduction, independent fog with rain disabled, shadowed scattering and neutral
sky. It also checks the row and instance caps, unchanged source color, zero
steady C++ allocations, explicit output state, odd resize, feedback rejection
and GL resource teardown. This is a correctness fixture, not a performance
benchmark or a complete scene-quality assessment.

The initial native pool and path previews ran on the AMD Radeon RX 5300. Each
recorded 300 active resolves, one target allocation, one complete 4,225-sample
surface-cache generation, 13.749027 MiB of owned image payload and zero weather
failures. These are startup/rendering checks from still-image captures;
controlled weather-on/off frame profiles are separate evidence.
