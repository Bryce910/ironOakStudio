# Integrated surface architecture

The modules are connected to the live editor and renderer. This file describes the integrated contracts; test evidence and pending final delivery gates are in STATUS.md and QA-CHECKLIST.md.

## Persistence and validation

Studio Document contains world_surface::Data surfaces and a derived shared TerrainField cache. Studio writes schema 5 and reads 1–5. Surface codec writes version 2 and reads 1–2, adding the layer normal-Y convention in version 2. Each codec record is individually quoted on its own physical Studio line; the line-based loader must never receive one multiline quoted record. Bounded validation rejects duplicate records, wrong map roles, invalid references and out-of-budget data transactionally. Undo/recovery preserve surface data; derived terrain caches are rebuilt, not serialized.

## Renderer

WorldSurfaceGPU owns a derived array with unchanged copies of the original six 513px landscape fields. LandscapeMaterials retains the original array. Imported images use six 513px layers: four guttered 510px quadrants, one half mip and one packed mip tail. Full runtime image size is 1020px; immutable source copies keep full source resolution. Color mips average in linear light. Normal mips preserve mean-vector length, which is converted to roughness before shading to suppress specular aliasing. CPU and GLSL samplers share the same layout.

Paint uses four extra layers for 64 world tiles. Each tile has a 65×65 grid at 2m spacing; neighboring edge bytes are identical. A 128-entry hash is stored in an unused texture row. Imported surface features reuse terrain uLandscape / model uVillageSkyMask on unit 12, so no new fragment sampler is added. The fullest tested model variant uses 15 of the RX5300's 16 active fragment samplers.

Prepare resources before committing Apply. Retain old resources/programs on any failure. Scalar commits preserve the TerrainField shared pointer read by workers; geometry commits join/rebuild safely. Explicit cleanup releases river meshes and combined arrays before context destruction. Common bindings reset model/NPC surface and graph indices between batches.

## Editing and runtime behavior

Surfaces (tab22) exposes immutable image imports/revisions, PBR layers, material binding, terrain paint/erase and image projection boxes. Wind / Roads (tab23) exposes local wind and terrain road/river curves. The existing History + Apply + Save lifecycle applies to these controls. A model material binding targets an alias and therefore all instances/LODs of that alias.

Projection boxes use normal-facing rejection, alpha/PBR image maps, XYZ rotation and edge feathering. Nearest eight boxes render within 180m. Local wind uses a smooth ellipsoid field and deterministic periodic gusts, capped at 5m/s total. Color/shadow/reactivity sample the same world-root displacement; water and particles share the field.

Road/river curves derive real terrain and CPU water/navigation results. Road removal restores source ground. River meshes have graded channels, dry banks and end caps. A 2m terrain grid requires rivers at least 4m wide with banks at least 2m. Foundation/clearance validation rejects unsafe edits. There is no fluid solver or interactive ripple simulation in this pass.

Texture graph nodes are fragment-only. Existing model/NPC vertex graphs remain subject to stage and shadow consistency checks; terrain and water vertex displacement outputs are rejected in favor of terrain/water controls. Reserved targets include terrain, ocean/pool/moat/pond water and the ten NPC appearance groups. Exposing a target does not mean every PBR output is physically meaningful on every shader; see the user guide for limits.

## Asset restoration and look

Purchased source masonry was restored on castle/gate/wall with exact triangle and geometry checks. Only wall/gate are active in the demo; village_keep is the active keep and already has source stone/PBR. Purchased originals and middle LODs remain unchanged. Architecture reference RGBA+mips rose 447.92→471.92MiB under the 544MiB cap. This is a bounded source correction, not proof that the art is finished.

The reviewed overcast HDR look is now published. look-published/publication.json records its exact backup, before/after SHA256 and byte-identical protected rows. Sky, materials and jungle use consistent biome attenuation. Both castle/tavern and jungle captures passed GL0/ref0; full walking performance is a separate final gate owned by the coordinated production chat.
