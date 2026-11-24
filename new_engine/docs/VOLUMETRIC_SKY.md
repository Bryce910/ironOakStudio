# Bounded volumetric sky

The old cloud shader drew density on one 3200m plane. The new sky marches a
2700–4100m world-space layer, accumulating extinction and light through its
thickness. It uses twelve bounded view steps, with two sunward density samples
only when the current view sample contains cloud. Empty weather cells,
zero-cloud settings and rays outside the layer skip the volume. Dense rays
stop when transmittance falls below .025.

`engine/world_sky.h` is the render-thread API. Create one `WorldSky`, fill a
`WorldSkyFrame` from the existing camera/light settings, and call
`world_sky_draw(sky, frame, destination_fbo, width, height, view)` before opaque
geometry. Channel 0 caches the main target; channel 1 caches the reflection
independently. The caller restores its terrain depth/cull state after drawing.
The function documents its exact GL state contract and rejects private-target
feedback, nonfinite inputs, excessive dimensions and overflow-prone input.
Free the module while its GL context is current.

The module generates one deterministic, periodic 64³ RGBA8 volume on the CPU.
Its channels contain smooth value/Worley base shape and smaller Worley erosion
scales. Mipmaps and explicit sample LOD limit distant noise. There is no asset
download, external texture, large 3D allocation or internal temporal history.
A narrow, fixed screen-space stratification pattern breaks marching bands. A
four-tap tent in the existing upsample reduces finite-step edge variance
without another pass or any extra 3D density sample. Sky alpha
falls from 1 to .68 through cloud to reduce stale sky history during flight.

The broad two-octave weather field and wind match the terrain/foliage cloud
shadow convention at 3200m. This preserves broad cloud/shadow placement, but
their economical surface shadows do not reproduce the volume's fine shape.
Light uses two approximate optical-depth samples, a bounded dual phase lobe,
sky/ground illumination and a small multiple-scattering floor. The clear sky
uses altitude-dependent air mass, Rayleigh-shaped angular variation, an
aerosol sun lobe and an antialiased HDR disc. This is a compact atmospheric
approximation, not a full spectral atmosphere or multiple-scattering solver.

## Memory and cost

| Resource | Storage |
|---|---:|
| Shared 64³ RGBA8 volume with all mip levels | 1,198,372 bytes |
| Main half-size RGBA16F target at 1600×900 output | 2,880,000 bytes |
| Reflection half-size target at 800×450 output | 720,000 bytes |
| Total for those two views | 4,798,372 bytes (4.58MiB) |

No allocation occurs after each view's size is established. Resizing allocates
a replacement before releasing that view's previous target; allocation failure
retains the old resource. Statistics report live bytes, actual target sizes,
allocation/resize counts, noise uploads and draw calls. Each sky draw consists
of one half-resolution volume pass and one four-tap tent full-size upsample. The
maximum is 36 3D samples per half-resolution pixel; most rays use fewer.
Native GPU cost and 45–60FPS remain subject to the complete renderer benchmark.

Twelve steps and half resolution are deliberate limits: close in-cloud views
can reveal coarse density or stratification; the periodic layer is one cloud
type rather than a weather simulation. Horizon haze reduces compressed,
undersampled cloud streaks. The sun edge is softened by the half-resolution
upsample. This is improved cloud geometry and scattering, without a claim of
AAA fidelity.

## Reproduction and checks

From the engine root, compile the isolated correctness fixture:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -I. \
  $(pkg-config --cflags sdl2) tests/world_sky_gl_test.cpp engine/world_sky.cpp \
  -o /tmp/valwood-world-sky-gl-test $(pkg-config --libs sdl2) -lGL
LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-world-sky-gl-test
```

The fixture runs the real module and volume. It checks zero-cloud consistency,
world/wind anchoring, camera parallax, inside/above-layer and high-altitude
rays, night/zero-sun inputs, finite nonnegative HDR, reactive alpha, independent
main/reflection target reuse, resize accounting, unchanged destination depth,
scissor/state handling and resource deletion. `VALWOOD_SKY_PREVIEW=1` also
writes three diagnostic cloud-coverage PPM images under `/tmp`. These are
software correctness/visual checks, not native performance evidence.

The existing lighting fixture's previous assertion that 3300m lies above all
clouds was explicitly updated to compare cloud-on/off at 4500m, above this
volume. Its zero-cloud, wind and horizon controls remain intact.

Conceptual references: [Guerrilla's 2017 NUBIS presentation](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine)
and [Hillaire's atmospheric rendering publications](https://sebh.github.io/publications/).
The implementation is original; these references do not establish this
engine's measured performance or visual quality.
