# World lighting contract

The world terrain, foliage and water use one supplied daylight state. Values
enter the HDR scene in linear space; exposure and tone mapping happen later.

| Uniform | Meaning |
| --- | --- |
| `uSun` | Unit direction from a surface toward the sun. |
| `uSunColor` | Direct irradiance divided by π. A Lambert BRDF is multiplied by this value and π. |
| `uSkyLight` | Upper hemisphere diffuse irradiance divided by π. |
| `uGroundLight` | Lower hemisphere diffuse irradiance divided by π, including its broad ground tint. |
| `uSkyZenith` | Visible zenith radiance, separate from the diffuse irradiance. Used by the sky and water's analytic sky fallback. |
| `uFog` | Distant aerial-scattering color. |
| `uHaze` | Exponential distance extinction coefficient in inverse meters. |
| `uCloud`, `uTime` | Cloud amount in `[0,1]` and continuous visual time in seconds. |
| `uEye` | Global camera XYZ, including the mirrored camera for a reflection. |

For old shader fixtures, zero `uSkyLight` selects the previous `uAmbient`
hemisphere: sky is `uAmbient*(1.25,1.38,1.52)` and ground is
`uAmbient*(.48,.50,.43)`. A zero `uSkyZenith` retains the previous analytic
zenith. Production sets the explicit values together, so visible sky color and
surface illumination can follow the same time of day without treating a single
sky direction as the complete diffuse environment.

Terrain uses the geometric normal for the broad hemisphere, preserving normal
map detail for direct lighting. Four fixed 550 m cardinal height probes estimate
the surrounding horizon's cosine-weighted sky obstruction. Each probe subtracts
the local tangent plane, so an unbroken hillside is not darkened twice after
its normal has already tilted the lighting hemisphere. The coarse height
map receives a 14 m bias to avoid self-darkening from interpolation and omitted
local detail. Sky access is bounded to at least 0.38; existing material AO still
modulates indirect light and the existing shadow map plus five directional
height probes attenuate sunlight. A small ground-colored return uses the
supplied lower hemisphere energy. These are bounded illumination approximations,
not traced indirect lighting or a full global illumination solution. They cannot
resolve caves, nearby mesh occlusion, or every direction of a mountain horizon.

The sky's broad cloud opacity occupies a 3,200 m plane in world coordinates.
Its two-octave coarse field also supplies terrain's weak moving cloud shadow.
Two finer edge scales, sunward highlights and restrained underside shading
remain visual sky detail. The layer fades through the low aerosol horizon
(`direction.y` 0.07–0.27), hiding its compressed distant ribbons. Coverage is off
at zero cloud amount and fades out when the observer approaches the layer;
this plane does not simulate volumetric cloud travel. Terrain cloud
transmission stays between 0.66 and 1.0. Foliage and water evaluate the same
bounded two-octave field for direct sunlight. Foliage performs this arithmetic
after alpha/LOD rejection, with no new texture reads; sky/ground fill stays
unchanged. Prelit distant cards can only scale their average direct energy,
so they still cannot reproduce exact per-leaf directional relighting.

Shore materials use actual terrain elevation, a broad coastal climate gate and
the existing world-space material patches. The gravel scan keeps its pebbles
and neutral stone color; the dry strand and damp reach vary softly instead of
following a uniform altitude contour. The same coastal gate prevents low inland
ground from acquiring beach gravel or shoreline wetness. This adds no texture
fetch sites, samplers or noise evaluations. The gate is a coarse atlas field,
so it does not encode every small inlet's exact dry edge.

Terrain adds four macro-height reads and two bounded cloud-noise octaves when
cloud shadows are enabled. The sky uses at most six noise evaluations. The lighting shaders
add no render targets or marching loops; the separate shallow-water snapshot
is documented in WORLD_WATER.md. The cost and final appearance
require native scene captures and GPU measurements; the small GL fixture is not
a frame-rate or visual-quality benchmark.

The focused test compiles the production terrain and sky shaders, then checks
actual rendered HDR output for hemisphere color, terrain sky obstruction,
exposed-surface and inclined-plane behavior, legacy fallback, directional shadowing, bounded cloud
transmission, cloud world anchoring, cloud-off/above-layer/horizon-fade behavior and finite
horizon/night-like output. Input-texture perturbation verifies that low inland
ground excludes beach gravel and wetness, while shore output transitions
continuously through the damp and dry bank heights:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Werror -I. tests/world_lighting_gl_test.cpp -lSDL2 -lGL -o /tmp/valwood-world-lighting-gl-test
/tmp/valwood-world-lighting-gl-test
```

The default uses an SDL offscreen context. Set `VALWOOD_TEST_NATIVE_GL=1` to
retain the caller's native driver. Read the printed renderer to establish which
driver actually ran. No new texture-cache ownership is introduced by this pass.
