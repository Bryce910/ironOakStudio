# Shoreline water in the native viewer

Water uses the same sky radiance, diffuse hemisphere and direct sunlight state
as the terrain and vegetation. It retains the half-resolution HDR planar
reflection, adds a bounded three-tap reflection filter, and filters wave
frequencies before they become screen-space scanlines. Broad cloud shadows
match the terrain's world-anchored field. Nearby bright sun glints also sample
the existing shadow map with four PCF taps, fading out at 125–160 m.

`engine/world_water_scene.h` owns a separate opaque color/depth snapshot.
After rendering terrain and sky, the viewer blits RGBA16F and DEPTH_COMPONENT24
into this pair before water writes the main framebuffer. It then samples the
snapshot rather than reading attachments being written. Allocation is lazy,
stable dimensions reuse the same objects, resizing replaces ownership, and
freeing happens before the GL context ends. The copy disables scissor and
restores the source as both read and draw framebuffer. No CPU readback occurs.

At 1600×900 the pair costs 16.48 MiB (12 bytes per pixel). Local forest/ridge
views skip the copy when the viewer's water visibility gate is inactive.
This is a component estimate, not total engine VRAM. A color/depth blit and
extra water shader reads add bandwidth; native profiles measure the actual cost.

The shader reconstructs opaque terrain depth with the exact near/far planes,
camera basis and projection jitter. A wavelength-dependent exponential
transmission mixes the lit bottom into shallow water and attenuates it as the
view ray travels through more water. Small normal-driven refraction is accepted
only when every texel in the bilinear color footprint lies below the surface;
otherwise the original bottom sample is retained. This prevents foreground
banks from leaking into the water body. A narrow, broken, distance-filtered
wash band uses actual bottom elevation for shoreline contact.

The snapshot currently contains terrain and sky. Vegetation and rigid props
render later and do not contribute underwater color. Bottom shading already
contains the scene's approximate lighting and aerial fog; this is not a full
participating-medium transport solution. There is no volumetric water, wave
geometry, fluid simulation, exact refraction ray trace or water collision.
Far/high views retain the analytic deep-water fallback. Temporal confidence
remains low for animated water; exact water motion vectors are not implemented.

`make test-world-water` compiles the actual production fragment shader and
checks separate HDR/depth ownership, disabled-scissor full copies, stable reuse,
resize/source bindings, transmission, original and refracted foreground
rejection, a submerged-refraction positive control, shadowed glints, fallback,
finite output and resource teardown. Removing the refraction footprint guard or
glint shadow visibility fails the corresponding actual-output assertions.
Software and native correctness checks are distinct from scene FPS evidence.
