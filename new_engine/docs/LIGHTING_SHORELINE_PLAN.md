# Lighting, shorelines and vegetation continuity

Continue in the native engine. Retain the preceding authored pass's native
profiles, captures and source fingerprints under
`performance/lighting-shorelines/before` and `before-source`.

1. Share explicit sky/ground diffuse irradiance and visible sky radiance across
   terrain, vegetation, sky and water. Add bounded terrain horizon visibility
   and coherent broad cloud shadows, without claiming ray-traced GI.
2. Shape uneven banks, gravel shelves and drainage mouths within the existing
   bounded Ulasse/Crosswind fields. Preserve the macro water layout, fixed
   settlement foundations, district seams and eight-hour width calibration.
   The native review also requires small coves/spits: within two local shore
   patches, effective signed shore may change by at most8m; signs farther than
   that from the reference edge remain intact. The comparison lake entrance
   stays unchanged.
3. Give water a separate opaque HDR/depth snapshot where shoreline detail is
   visible. Derive optical thickness, shallow bottom transmission and a narrow
   broken contact foam band from actual rendered terrain; reject foreground
   refraction leaks and retain a far-water fallback. No framebuffer feedback.
4. Make tree LOD coverage decisions per root, partition transition coverage
   once, and match the distant-card lighting/fog with nearby foliage. Retain
   species, instance caps, streaming budgets and projected-size rejection.
5. Exercise actual shaders, copy/resize/ownership, map constraints and LOD
   interfaces. Review native on-foot views before the final sequential Radeon
   1600x900 moving-camera checks. Report rare misses, startup, loading holds and
   incomplete visual work alongside averages. Reopen the improved world.

The existing pass averaged59.95FPS across five routes, with8/15,600 intervals
over22.222ms. This is the reference to preserve, not a guarantee for every
position or an eight-hour performance soak. New component memory and GPU cost
must be documented. Cinematic realism remains an iterative production target.
