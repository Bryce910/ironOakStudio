# Live world integration — 29 October 2026

Valwood's custom engine now connects scene identities, NPC schedules, skeletal
rendering, physical props, dialogue/training and automatic scenery residency in
the running world. Studio exposes reusable component bindings and working
surface, wind, road and river authoring. The default castle/jungle integration
contains six skinned people and two physical supply crates.

**Both completed walking routes meet the average and statistical 1% target.
Neither passes a strict, every-frame 45 FPS floor.** This is a tested integration
build; it does not establish a finished commercial AAA engine or final AAA art.

## Implemented

- One authoritative logical world drives explicitly bound physical NPC targets.
  Nearby actors turn, walk and patrol with terrain/scenery/capsule checks. The
  existing purchased knight uses real skinning, crossfades, terrain foot
  adjustments and shared color/reflection/shadow poses. Explicitly bound edited
  NPCs use the existing crowd pose library without duplicating their renderers.
- Typed scene/body/actor IDs connect Jolt bodies, full quaternion prop rendering,
  player collision queries, entity activation, conversations and the training
  host. **E** at the instructor opens a conversation; its choice starts a real
  round, and finishing/cancelling permits another. The jungle steward also has
  a local conversation.
- Pending component edits activate only through **Configure**. Scene Apply
  validates active references and visual capacity before replacement. Both Stop
  choices restore transient physics, gameplay, logical and animation state;
  failed restoration holds Save/Play and exposes repair/retry. Committed source
  records retain their editor identity, including coincident model placements.
- Normal startup defers distant near meshes. A CPU worker prepares geometry,
  image data and full mip chains; the graphics owner uploads bounded chunks.
  Actual placement distances drive demand, eviction and reload. Collision data
  and mid/far fallbacks remain available. Destination travel waits for immediate
  required detail; unrelated prefetch does not hold travel indefinitely.
- Surface image imports, painted/projection materials, shared wind volumes,
  graded road/river terrain fields and river rendering use the normal Apply,
  Undo and persistence paths. New rigid/skinned renderers retain authored
  fragment and vertex graphs, with matching displacement in shadows.
- Softer afternoon lighting and a licensed CC0 overcast HDR sky improve street
  readability. The jungle keeps its canopy darkening, cloud coverage and mist.
  The published change preserves every geometry/material/placement row and the
  saved graphics settings. [Lighting publication proof](../../../studio/surface-finish-2026-10-29/look-published/publication.json).

See [controls, authoring and limits](LIVE_WORLD.md), [streaming implementation](../STREAMING.md)
and the [surface editor guide](../../../studio/surface-finish-2026-10-29/USER-GUIDE.md).

## Verification

Executable SHA-256: `b01f0fdef228a765029cf506cee2b8c8dcb0cc1174e31485c88aeadfbc1ae65d`.
The two final shader optimizations are runtime source files; their hashes, the
binary and the tested project/settings are pinned in the
[route source freeze](routes-optimized/source-freeze.json). The
[after-test check](routes-optimized/freeze-after.json) confirms unchanged inputs.

- [120 coordinator contracts](runtime-components-contract.log), including 1,200
  allocation-free frames; [71 Studio contracts](components-studio-contract.log);
  [visual admission boundaries](visual-admission-cpu.log).
- [Final native editor workflow](native/editor-workflow3/results.json): 39 checks,
  five actual commits, one intentional rejection, Play/Discard, retained draft,
  durable identities and Studio5 save/reload. Earlier failed attempts are kept.
- [Final live world workflow](native/live-qa2/result.json): actual SDL interaction,
  dialogue choices, training finish/restart, impulse and tipped-prop pivot,
  all-system rollback, jungle travel/streaming and local conversation. It loaded
  18 groups and evicted 67, ending with zero pending/failed groups.
- [Final surface workflow](native/surface-workflow2/results.json): three Applies,
  three expected rejections, immutable imports, paint/field bytes, road/collision
  heights, river queries/meshes, wind/material/graph resources, undo and reload.
  [Tool 24 native capture](native/tool24/panel.png) also passed.
- [Skeletal vertex graph GPU fixture](skeletal-renderer-vertex-graph-contract.log)
  verifies identical displaced geometry in color, reflection and depth, including
  floating origins and binding reset. [Population GPU fixture](population-runtime-gl.log)
  checks runtime motion, activation and matching elevated color/shadow geometry.
- [Streaming GPU fixture](../streaming-native-gl.log) verifies legacy/staged
  pixels, bounded upload chunks, cancellation and ownership, and rejects owner
  decoding/file opens/source writes during staged uploads.
- Sky dead-work removal preserved **all pixels exactly across 768 cases**;
  foliage BRDF zero-support removal preserved **10,485,760 components across
  1,280 cases exactly**. Cloud sample counts, fog and material inputs were kept.
  [Sky proof](sky-dead-work-native.log), [foliage proof](brdf-zero-native.log).

All listed final native workflows exited normally with zero GL errors and zero
remaining shared texture references. Live tests reported zero physics update
errors, heap fallbacks and failed gameplay ticks. Editor Apply/import/loading
pauses are correctness tests and are **not walking FPS measurements**.

## Complete walking measurements

Native AMD Radeon RX 5300, 1600×900 display, **1120×630 scene render (0.7 scale)**,
automatic quality off, 2048 shadows, half-scale reflections, contact shadows,
volumetric fog and clouds enabled. Textures and authored scenery were retained.
All 120 warmup frames are retained separately; no subsequent interval is dropped.
There were no simultaneous builds, asset imports or other Valwood GPU contexts.

| Completed third-person return walk | Average FPS | 1% low FPS | Frames below 45 | Worst frame |
| --- | ---: | ---: | ---: | ---: |
| Jungle, 432.63 m | 58.04 | 45.05 | 25 / 7,580 | 25.59 ms |
| Castle streets / tavern, 585.07 m | 59.89 | 49.73 | 2 / 10,380 | 22.99 ms |

The longest run of under-budget frames was one frame on each route. Both walks
completed with zero collision/slope blocks, zero GL errors and zero remaining
texture references. **Both strict zero-miss floor gates failed.** The jungle's
1% margin is small; further headroom and longer sessions remain necessary before
promising a 45 FPS minimum.

[Raw routes and gates](routes-optimized/results.json),
[jungle gate](routes-optimized/jungle-third.gate.json),
[castle gate](routes-optimized/castle-streets-tavern-third.gate.json).

The [first combined runs](routes-final/results.json) are retained: jungle
57.92 average / 44.82 1% low / 27 misses, castle 59.88 / 47.87 / 10 misses.
Firefox was subsequently closed normally under the user's earlier explicit
permission; Steam, Discord, Cinnamon and ChatGPT remained running. The
[final GPU preflight](native-gpu-preflight.json) records that setup. Browser
state therefore differs between attempts: the FPS change cannot all be
attributed to the shaders. The exact pixel comparisons establish visual
equivalence independently of timing.

## Package and remaining scope

The [final launcher](<../../../../builds/relocated/Valwood Live World Demo 2026-10-29/launch.sh>)
belongs to the new 1,441-file, 6.48 GB Linux package, content ID
`85113dbec60faacc20a381c95fc93dd96d856ad9b8fefda7b5ef4f6953a729aa`.
[Build](package-final-build.json), [verification](package-final-verify.json),
[relocated launch](native/package-final/result.json),
[actual first-person package screenshot](native/package-final/castle-street.png).
It launched from `/tmp` after moving the package to a path containing spaces;
the original package path was absent. The editable source workspace remained
mounted, so this is not clean-machine compatibility certification.

The source project remains the editable working engine. The package contains
eligible application libraries, runtime assets and license/provenance notices,
including the CC0 sky and the authored NPC memory example. Player saves remain
excluded. [26 inventory contracts](asset-database-final-tests.log) include this
authored-data versus player-save boundary. Intermediate packages from this pass
are superseded by this final verified package; the prior Studio v4 checkpoint is
separate historical evidence.

The existing baked crowd is not globally replaced by the new skeletal system.
The supplied rig has Walking, Running and Skill_03; its held walking pose is not
an authored idle clip. Navigation is bounded local steering rather than a general
world-scale crowd/navmesh solution. The combat host dispatches the training
arena; arbitrary hostile encounters require their own host. Offline catch-up
advances the logical world on reopening; it is not a computer-off AI server.
Additional rig/animation authoring, richer landscape composition, broad hardware
testing and removal of the measured frame dips remain before an AAA claim.

More asset purchases are not required to test or extend these connections.
