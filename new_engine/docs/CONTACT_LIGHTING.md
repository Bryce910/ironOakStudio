# Bounded contact lighting

`engine/world_occlusion.cpp` adds local ambient obscurance and short sun-contact
shadows to the custom renderer. This is a screen-space depth effect, not global
illumination, ray-traced shadows, GTAO, or a substitute for the existing terrain
horizon and shadow-map visibility.

## Frame integration

The main HDR target has two RGBA16F color attachments: scene radiance and the
already fog-attenuated indirect surface contribution. Terrain and model/foliage
shaders write both. The sky draws only to attachment 0 after attachment 1 is
cleared to zero; the viewer restores both draw buffers for surfaces. Water
explicitly writes zero indirect radiance and uses stability alpha .28, which
also disables its short contact-shadow attenuation. The reflection target
keeps its single color attachment and does not run this contact pass.

After all main-view terrain, water and scenery draws, the pass reads scene
color, depth and indirect radiance. It writes separate owned targets and sends
the final HDR color into the existing temporal resolve, before tone mapping.
It preserves the scene's temporal-stability alpha. The main depth attachment
is neither overwritten nor replaced.

Ambient obscurance attenuates only the recorded indirect contribution. The
remaining color receives the separate contact visibility. That remainder
includes direct BRDF, leaf wrap/transmission, and any aerial fog already
composited by the surface shaders: this economical implementation does not
separate fog into a third lighting attachment. Contact influence fades away
by 100m, where the fog error is small for current renderer haze settings. AO
itself does not darken sunlight or the separately composed fog contribution.

## Bounded work

Four fullscreen draws run per resolve:

1. A half-resolution field reconstructs view-space position and a geometric
   normal from depth, choosing the less discontinuous horizontal and vertical
   depth neighbors. A rotated 16-sample spiral estimates obscurance within
   1.15m; projected radius is capped at 104 full-resolution pixels. It rejects
   sky and out-of-range neighbors and biases the normal response to reduce
   planar self-occlusion. Raw obscurance is capped at .74.
2. The same field traces at most eight depth samples toward the view-space sun
   over approximately .035–.770m. Thickness, distance bias and screen-edge
   fading reduce false hits. Back-facing surfaces skip this trace.
3. Horizontal and vertical seven-tap depth-aware filters smooth the two field
   channels, reusing two half-resolution targets.
4. A full-resolution composition gathers four nearby field samples with
   bilinear and depth-rejection weights. If none match, it uses neutral
   visibility, avoiding a dark halo across an unmatched depth discontinuity.

The field packs ambient visibility in R, sun-contact visibility in G, linear
view depth in B and a surface marker in A. Geometry beyond 100m and sky produce
neutral visibility; influence fades from 55–100m. Fragile low-stability foliage
receives reduced obscurance, and color alpha gates contact strength. Defaults
are .90 ambient strength and .70 contact strength. The module has no extra
history buffer: the renderer's existing TAA filters the final composed result.

## Ownership and memory

Create/free `WorldOcclusion` on the thread with the current GL3.3 context. Pass
matching external scene/depth/indirect textures and projection parameters to
`world_occlusion_resolve`. It returns a separate owned result texture, valid
until resize or destruction. `world_occlusion_field` exposes a borrowed
filtered field for inspection, with the same lifetime. Inputs must not alias
owned output/intermediate textures. Invalid inputs or a failed resize return
the original scene color; the viewer treats this as a required-pass failure.

| Added resource at 1600×900 | Bytes | MiB |
|---|---:|---:|
| Two 800×450 RGBA16F fields | 5,760,000 | 5.49 |
| One 1600×900 RGBA16F composed result | 11,520,000 | 10.99 |
| Module total | 17,280,000 | 16.48 |
| Main target's indirect attachment | 11,520,000 | 10.99 |
| Total contact-lighting additions | 28,800,000 | 27.47 |

Textures use nearest sampling and clamp-to-edge; the compose shader performs
its own depth-aware upsample. Dimensions are capped at 8192 per axis and
4096² pixels in total. Resources resize when the main dimensions change and
are reused at steady size. A failed resize releases the contact targets and
returns the caller's source; the next resolve can allocate them again. Stats
report live field/result bytes, dimensions, successful resizes and resolves.
The viewer validates its main target's resize result, exits cleanly on failure,
and resets temporal history after a successful window resize. The resolve API
rejects inputs aliasing any owned field or result texture before drawing.
There are no steady-state allocations, CPU readbacks, or per-draw state queries.
The module leaves its result FBO/program/VAO bound, full-size viewport, RGBA
writes enabled, and depth writes/depth/blend/cull/scissor disabled. The caller
owns subsequent state setup. All owned textures, FBOs, programs and VAO are
freed before the viewer destroys the context.

`VALWOOD_DISABLE_OCCLUSION=1` bypasses resolve for an A/B comparison. The main
MRT still exists in that diagnostic configuration, so it is not a complete
memory/bandwidth comparison to a renderer built without the feature. Viewer
JSON reports enablement, indirect/module memory, resolve/resize/failure counts
and measured contact GPU time. Use complete native frame timings to establish
45–60FPS; the bounds above do not establish a hardware time by themselves.

## Limits and regression checks

Depth contains only visible surfaces. Off-screen or hidden occluders are
missing; thin leaves, depth discontinuities and disocclusion can lose detail or
produce a small halo. A single depth layer and finite thickness are
approximations. Half resolution and seven-tap filtering soften the smallest
contacts. Screen-fixed spiral rotation plus TAA can retain some noise during
motion; there is no independent contact-history reprojection or multi-bounce
light transport. Macro shadow maps and terrain occlusion remain necessary.

`tests/world_occlusion_gl_test.cpp` runs the actual module and shaders. It
checks a planar control, a depth-blocker contact, ambient-only versus
direct-only lighting, neutral sky, depth-edge separation, stability-alpha
preservation, resource reuse, odd resize, invalid input/feedback rejection and
GL cleanup. Run the repository's `make test-world-occlusion` target for the
correctness fixture. This is not a visual-quality oracle or a benchmark.

The code is independently implemented. Primary references for the general
screen-space obscurance/filtering and indirect-lighting ideas are
[McGuire, Mara and Luebke, *Scalable Ambient Obscurance* (2012)](https://research.nvidia.com/publication/2012-06_scalable-ambient-obscurance)
and [McGuire et al., *The Alchemy Screen-Space Ambient Obscurance Algorithm* (2011)](https://research.nvidia.com/publication/2011-08_alchemy-screen-space-ambient-obscurance-algorithm).
The current estimator does not implement those papers' complete algorithms;
their published performance/quality results do not describe this engine.
