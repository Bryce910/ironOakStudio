# Foliage continuity validation

This pass changes only the world foliage vertex/color shaders and adds
`tests/world_foliage_gl_test.cpp`. Runtime assets, source licenses, model
budgets, instance caps, radii, and the binary shadow mask are unchanged.

The shader now uses each instance's camera-relative root for LOD bands and
distance fades, matching CPU selection. A tree's outer branches therefore do
not enter a different band from its trunk. Projected-size fade uses the
root plus half the assigned tree height, rather than each fragment's depth.
Alpha-edge coverage and size/distance fades multiply **inside** the chosen
LOD interval. Previously a card's partial LOD interval and size fade could
reject every pixel when separately tested against the same random rank.

Alpha masks have a bounded derivative transition over the existing temporal
sequence. Zero alpha still discards, opaque interiors remain covered, and
the transition width cannot grow past 0.20 alpha units. No alpha blending or
extra texture samples are introduced. Normal-map detail fades between
80–125m before sampling stops. A camera directly above a billboard root
uses a finite facing direction instead of collapsing the quad.

Foliage supports the terrain's `uSkyLight` and `uGroundLight` diffuse
irradiance contract. Both-zero uniforms retain a legacy `uAmbient` fallback.
Thin leaves use a softened sky/ground hemisphere; opaque bark and rocks
use the geometric hemisphere and receive no leaf transmission. Leaf wrap
lighting also responds to the existing shadow visibility. Meshes and cards
share altitude-dependent haze and instance tint ranges. Far cards keep
their inexpensive photographed shading, scaled by average sky/ground and
sun energy; they add no normal, ARM, shadow, or BRDF texture samples.

The final refinement applies the terrain's exact two-octave procedural cloud
transmittance to direct sunlight on both meshes and far cards, including leaf
wrap/transmission. Its projected plane, sun/altitude guards, threshold, smooth
cloud onset, and 0.66–1 attenuation match the terrain. World XZ includes the
current field anchor, so changing that anchor cannot drag shadows. Sky/ground
fill remains intact. This adds arithmetic after mask/LOD rejection, with no
texture reads; native timing is required to measure its overdraw cost.

## Software GL checks

From the engine root:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. $(pkg-config --cflags sdl2) \
  tests/world_foliage_gl_test.cpp -o /tmp/valwood-foliage-gl \
  $(pkg-config --libs sdl2) -lGL
LIBGL_ALWAYS_SOFTWARE=1 /tmp/valwood-foliage-gl
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=offscreen make test-world-models
```

Both passed on llvmpipe (LLVM 20.1.2). The first compiles the actual color
shader with controlled interpolants, and separately compiles the actual
vertex shader. It checks opaque LOD partition at eight band positions,
partial projected-size coverage, crown-independent fade, local/horizon
distance fade, leaf/bark hemispheres, old uniform fallback, equal mesh/card
haze, eight-phase alpha coverage, exact transparent/opaque edges, continuous
normal-detail cutoff, finite overhead billboard geometry, and invariant
root/seed values after an origin shift. The second retains all 28 real-asset
golden geometry/image assertions, color/shadow draws, height-specific card
fade, program-cache behavior, and texture lifetime checks.

These are correctness checks, not native performance or motion-quality
evidence. The parent integration owns native captures and frame timings.
The added actual-GL cloud checks cover 16 world positions (0.660–0.987 direct
transmittance), equal mesh/card factors, unchanged sky fill, zero/negligible
cloud behavior, above-cloud-plane bypass, and exact world-anchor invariance.

## Limits

Far cards remain single-view, prelit billboard images. They cannot provide true
parallax or per-leaf relighting; coarse cloud shadows scale only their average
direct component. Temporal alpha
coverage depends on the existing resolve and still needs motion inspection.
Different near/mid meshes can retain aggregate silhouette differences even
with mathematically complementary fade masks. Local and horizon forests
contain different roots, so their field crossfade is a density blend, not
a one-to-one LOD transition.

The physical mature-tree heights are 25m pine and 22m fir; their transparent
far canvases span 26.5m and 23.32m. Callers should assign physical heights
consistently for projected-size fade. New broadleaf/young-pine values remain
10m/3.2m. Asset-source adaptations and licenses are recorded in
`docs/CINEMATIC_ASSET_SOURCES.md`.
