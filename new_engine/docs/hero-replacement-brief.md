# Valwood playable hero: replacement asset brief

The playable hero is the same adult man in both chapters: an overworked father
in modern everyday clothes, then that same recognizable face and body wearing
upgradeable medieval armor. Build both looks on one skeleton. The head, hair,
skin tone, height, and proportions should remain identifiable through the
transformation.

## Deliverables

- Editable Blender source plus FBX or glTF/GLB exports, with separate PBR
  textures. Include a neutral bind pose and a list of all bones, materials,
  blendshapes, and animations.
- One base body and face, a modern outfit (casual shirt or jacket, trousers,
  everyday shoes), and a modular knight outfit. Armor pieces must be independently
  equipable: head, chest, hands, legs, and feet. Keep body regions beneath them
  hideable to prevent clipping.
- Separate weapon and equipment meshes with documented hand and back/hip
  attachment points: one-handed sword, shield, two-handed sword, axe, staff,
  and bow. Both hands must have finger bones and usable grip poses.
- Facial controls for blinking, speech, and basic emotion. Prefer named ARKit
  blendshapes, but provide the actual export for verification rather than a
  Unity-only prefab or shader demonstration.
- Head, each arm, and each leg as cleanly separable geometry with capped cut
  surfaces or a documented way to produce them. Armor must follow those parts.
- Game-ready materials and at least three distance levels. The close-up face,
  hair, eyes, and hands need special attention; the existing target hardware
  has 3 GB VRAM.
- Source animations for relaxed idle, walk/run in all directions, starts,
  stops, pivots, jump, land, swim, guard, attacks for each weapon class, hits,
  and death. If supplied separately, include permission and source files to
  retarget them to this skeleton.

## Acceptance scene before purchase sign-off

In the Valwood engine, show the same face in the apartment and armored in the
meadow. Test ten seconds each of forward/backward/sideways walking, a diagonal,
start-stop-repeat, 180-degree turn, a slope, stairs, first-person weapon view,
and third-person combat. Review feet for sliding, hands for weapon clipping,
armor for intersections, facial close-up, and frame time on the RX 5300.

The current renderer uses baked player poses. A new model or Unity Animator
controller does not automatically replace that runtime path; integration must
preserve the source skeleton and animations so the new asset can actually show
its motion quality in the custom engine.
