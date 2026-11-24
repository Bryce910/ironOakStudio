# Surface authoring and visual completion

Finish the material, image-decal, wind, shader-target and road/water authoring gaps in the existing custom renderer, then polish the existing castle street, tavern and jungle. Preserve purchased originals and coordinate integration with the active production-engine chat.

1. Audit source maps, active LOD representations, texel density, map roles and baked material differences. Correct verified fidelity problems; do not enlarge images or add sharpening to manufacture detail.
2. Add persistent texture imports, reusable PBR layers, model material bindings, and terrain painting. Share the renderer's existing landscape texture array so new surface features stay within the GL3.3 sampler limit. Keep source facade textures and sky/roof/canopy fields intact.
3. Complete imported-image projection boxes with alpha, material response, orientation and edge controls. Add deterministic local wind volumes and integrate particles and vegetation consistently.
4. Extend graph inputs and target bindings to texture images, terrain, water and character materials. Keep compile failure atomic, preserve unassigned source behavior and maintain shadow/animation compatibility.
5. Complete editable road/river spline workflows with conforming surfaces, banks, water queries and rendered river meshes. Validate terrain/collision/navigation and reject unsafe foundations or floating water. This pass does not implement a fluid solver or interactive ripple simulation.
6. Use these tools for a coherent demo look: consistent source material response and scale, restrained wear and vegetation transitions, clear indoor/outdoor lighting and stable distant building transitions. Compare real gameplay captures with the previous look at unchanged saved quality.
7. Exercise Apply, undo/redo, save/reload, bad-resource rejection and program reload. Run meaningful CPU/native GL checks, then sequential complete walking routes, recording low FPS and individual budget misses. Do not run competing builds, cooks or GPU tests during timed routes.

Shared ownership: this chat owns new surface modules, surface contract tests, visual staging tools and look review. The other production chat owns central document/viewer/Makefile integration, entity/prefab/recovery/physics/animation/regions/gameplay, combined builds, final route tests and packaging. Native GPU testing is explicitly serialized between the chats; no build or cook runs during timed walking tests.

Completion means connected editing and runtime behavior with verification, rather than empty tabs or a claim that every commercial-engine feature is finished. Document any remaining limits precisely.
