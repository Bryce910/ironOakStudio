# Assignment 01 — Valdoa castle street and tavern frontage

## Goal and bounds

Finish one coherent 50–100 metre walking sequence using the existing medieval village, prop and nature assets. Start at `castle-finished-arcade` and inspect `castle-west-street` / the tavern before choosing the exact working stretch. Agree an annotated overview with the owner before moving structural buildings. Name the root group `VAL_Street01`; use child groups for architecture, ground contact, vegetation and dressing. This is a visual walking demo assignment. Keep combat, quests, simulation rules and global world-generation settings outside this pass.

The existing source scenery is already substantial. Added placements share the 1,024-per-document budget with the rest of the authored scene; that number is not a performance allowance. Start with a small increment and profile before expanding. Terrain sculpt edits share 16 bounded 128 m tiles. Surface paint has a separate budget. Use the guide's limit table and Production diagnostics.

## Pass A — composition and walkability

Establish a strong view into the street, a readable tavern approach and a terminating landmark. Preserve castle gates, drawbridge/moat placement outside the outer wall, the training space and existing access routes. Vary house spacing and angles deliberately; avoid copying an identical row into every view. Preserve source collision for buildings with door openings. A full box collider can seal a visible doorway.

Leave a roughly 3 m clear main route and usable passing/turning space near doors. This is an art brief, not a navigation guarantee: walk the result in first and third person, including corners, steps, ramps and the tavern threshold. Do not move NPC route anchors or training attachments incidentally with scenery groups.

## Pass B — believable ground contact

Seat buildings/props with End, then inspect their foundations and pivots. Use terrain/surface tools for gentle slopes, settled paths, dirt shoulders, mud/wetness pockets and leaf buildup. Add restrained moss/stain projections around water and shaded foundations. Blend edges rather than covering the entire street with a repeated decal. Avoid new sculpting beneath protected foundations, water or route corridors; rejected Apply operations retain the prior live world.

Use existing stone, wood, dirt and vegetation materials consistently. Global alias material changes affect every instance using that alias. Prefer local dressing/projections for location-specific wear.

## Pass C — lived-in detail

Dress the tavern entrance with a coherent cluster of barrels/crates, seating, tools, firewood, signs and appropriate supplies from available aliases. Build clusters that tell a purpose; leave areas of quiet space. Add small plants in plausible untrampled/shaded areas, and keep main foot traffic clear. Break repetition with rotation, selected variants and subtle positive scale changes. Do not scale architecture so much that doors or stairs become implausible.

Preserve existing animated inhabitants first. Animated actors belong in NPC / Navigation; dragging character scenery into Assets does not give it a patrol or animation. Add only a small number of actors after the route is stable; test their local route, footing and behavior without relying on unfinished global crowd avoidance. Add existing cats/dogs only if their supported habitat/route fits the assignment. Record every actor/effect addition in the return notes.

## Pass D — lighting and effects

Review existing daylight, canopy/roof shelter, practicals and fog before adjusting them. Use a warm tavern accent with controlled exposure; keep outdoor views and window/door highlights readable. Rebuild cached lighting with Apply after moving trees/buildings. Use VFX Studio for restrained chimney smoke or a campfire where the space supports one. Practical lights share eight active slots; excessive overlapping lights can displace useful ones. New local lights do not create independent shadow maps.

## Acceptance and deliverables

- A continuous, collision-safe walk in first and third person, plus the same composition after save/close/reopen.
- Three screenshots: street approach, tavern frontage, and a close ground-contact view. Include an overview identifying the edited stretch.
- Changed saved files exported with `Return_Changes.bat`; a short change log and new asset aliases/units/LOD/collision settings.
- Completed `ACCEPTANCE_CHECKLIST.csv`, with problems recorded honestly.
- A before/after benchmark on the owner's reference machine at the same settings. Retain raw JSON, CSV and `.world.json` sidecars. Target 45–60 FPS; record every measured frame below 45 separately from averages. Do not increase detail if the reference route already misses its budget.

The owner reviews a staged copy before accepting the work. Passing this first assignment validates the workflow; it does not mean the whole Valwood world or every machine has passed.
