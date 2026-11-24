# Project status for the 3D designer

Reviewed **2 November 2026** from current launchers, project metadata and saved
test artifacts. The parent readiness pass subsequently opened all three current projects in the
rebuilt Linux editor (zero GL errors/texture references), and exercised six
functional editor workflows plus actual graph Compile/Expand and saved reload.
Those are startup/functional checks, not new sustained performance or complete
walking-route certifications. See the final readiness report for Windows tests.

Dates below name
the recorded verification, not a claim that every later build was retested.
This is a playable prototype and several environment reviews, not a finished
AAA game or one fully connected, polished world.

## Open the intended project

All launchers are under `/home/computer/Desktop/final_engine`. Close the current
engine session before opening another. Scenic names select a starting camera;
they do not select independent editable scene files.

| Area | Current project / executable | Launcher | Latest relevant evidence and limits |
| --- | --- | --- | --- |
| Purchased castle city and countryside | `new_engine/projects/valwood_city_showcase` / `rpg_city` | [City editor](/home/computer/Desktop/final_engine/Launch_Valwood_City_Editor.sh), [city walk](/home/computer/Desktop/final_engine/Launch_Valwood_City.sh), [countryside](/home/computer/Desktop/final_engine/Launch_Valwood_Countryside.sh) | **2 Nov:** six final ground/residential captures plus two overhead captures passed; zero GL/streaming failures. Owned village layout, castle backdrop, BEP countryside, dense grass and wind are integrated. This is an environment/walking review: old quest/vendor coordinates were not relocated, and source buildings do not provide a complete set of furnished interiors. |
| Emerald jungle / existing visual gameplay world | `new_engine/projects/valwood_visual` / `rpg` | [Jungle](/home/computer/Desktop/final_engine/Launch_Valwood_Jungle.sh), [visual editor](/home/computer/Desktop/final_engine/Launch_Valwood_Visual_Editor.sh) | **31 Oct:** doubled authored footprint, six views, 1,031 m collision-tested loop, wildlife, controller and complete quest rehearsal passed. Current jungle launcher delegates to Presentation with `rainforest-milestone`. The similarly named Rainforest launcher opens the older `valwood_world` project instead. |
| Grandstone publisher castle scene | `new_engine/projects/valwood_showcase` / `rpg_grandstone` | [Stronghold editor](/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold_Editor.sh), [stronghold review](/home/computer/Desktop/final_engine/Launch_Valwood_Stronghold.sh) | **1 Nov:** twenty source-scene viewpoints passed native render/streaming checks. Authored castle, snow terrain, baked lighting, source fixtures/lava, dwarves and targeted material repairs are present. Source metadata explicitly says triangle navigation is pending; the final route is a segmented visual flight, not an end-to-end walk/collision rehearsal. |
| Older world / packaged gameplay snapshots | `valwood_world`, `valwood_presentation`, relocated builds and `Valwood_Artist_Work` | [World](/home/computer/Desktop/final_engine/Launch_Valwood_World.sh), [journey snapshot](/home/computer/Desktop/final_engine/Launch_Valwood_Journey_Demo.sh) | Separate historical projects/packages. Their art, binaries and evidence do not automatically include the current city, countryside or Grandstone. The existing artist workspace is dated **31 Oct**. The separately verified **2 Nov Windows orientation bundle** now supplies all three current projects; retain older work rather than updating it in place. |

The current [Presentation launcher](/home/computer/Desktop/final_engine/Launch_Valwood_Presentation.sh)
prefers source `rpg + valwood_visual` when available. Older reports saying that
it selects the 30 Oct r2 package describe its historical behavior. Its current
fallback is the separate Visual Upgrade package. Likewise,
`valwood_stronghold` is an earlier project; the active Stronghold launchers
point to `valwood_showcase`.

Current city geometry is pinned in
[city_showcase.json](/home/computer/Desktop/final_engine/new_engine/projects/valwood_city_showcase/city_showcase.json)
to `assets/purchased/bep_reforged/city-ground-final/catalog.json` (SHA256
`12a51a46e7491b8a5bec55d9fc5ed70fcb6ea861313260c4b00089c9d298cb98`).
Grandstone's active material/geometry revision is
`assets/purchased/nature_castle/showcase-panels-quality/catalog.json`, recorded
in its
[publisher_showcase.json](/home/computer/Desktop/final_engine/new_engine/projects/valwood_showcase/publisher_showcase.json).

## Features and their verification scope

| Feature | Actual status |
| --- | --- |
| Gameplay and combat VFX | Contact-triggered hit/parry/guard sparks, impact/dodge dust and weapon trails are implemented. **30 Oct** packaged journey exercised defeat/retry, trial, hostile encounter, reward, save/load and ending with zero effect failures. The expanded jungle rehearsal passed again **31 Oct**. This proves the tested existing journey, not its integration into the new city/Grandstone layout. |
| VFX editor | Eight continuous-emitter presets, appearance/motion controls, placement, duplicate, preview pause/restart/scrub/solo and persistence. **2 Nov CPU review:** all 41 existing VFX checks passed. This is not an arbitrary particle graph, one-shot event-authoring or cinematic sequencing system. |
| Controller | Player movement/combat and title/pause/settings/journal/dialogue/retry/ending/showcase navigation are implemented. Native SDL virtual-controller replays passed 41 assertions on **30 Oct**, repeated in the jungle on **31 Oct**. F310 detection was checked; hands-on physical sticks, wireless behavior/motors and a physical PlayStation pad were not certified. Editor authoring remains keyboard/mouse. |
| Graphics and display | F10 / pause settings provide Performance, Balanced, Cinematic and Custom presets, scene scale, optional automatic scaling, target FPS, shadows, reflections, contact shading, fog/clouds and display controls. Preferences are project-local and apply transactionally. Current city, visual and Grandstone files use Custom, automatic quality on, 100% ceiling / 70% floor; the older saved 85% fixed-profile report is not their current setting. Higher settings improve rendering, not missing source-art detail. |
| Materials and importing | Static embedded GLB, immutable revisions, original-source inspection, PBR layers/normal convention, surface paint/projections and typed material graphs are available. **2 Nov CPU review:** all eight focused checks and the real editor translation-unit syntax check passed. A stale background texture-reimport/Undo bug was fixed in source. The 2 Nov native surface and Graph Compile/reload workflows subsequently passed. The final export-only relink and exact binary coverage are recorded in READINESS_REPORT.md. |
| Town interactions | The existing gameplay world has armory sharpening, grocer provisions, tavern services and persistent food/benefits. These do not establish that the newly recovered city has functioning vendors/tavern interiors at its new building positions. |

Current evidence: [material/assets review](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/material-assets-review.md),
[controller report](/home/computer/Desktop/final_engine/new_engine/docs/controller/2026-10-30/RESULTS.md),
[gameplay polish report](/home/computer/Desktop/final_engine/new_engine/docs/slice-polish/2026-10-30/RESULTS.md),
[jungle report](/home/computer/Desktop/final_engine/new_engine/docs/jungle-expansion/2026-10-31/RESULTS.md).

## Remaining content and integration work

- **Hero identity:** the existing helmeted knight is still the prototype hero.
  The intended ordinary father in modern everyday clothing, then the same man
  in knight armor, is not implemented. Current visual-project metadata still
  says the modern source model is required. Preserve one face/body/skeleton
  across the civilian and armored looks.
- **Modular gear:** legacy inventory/equipment code and runtime animation
  sockets exist. That is not a verified, finished interchangeable armor and
  weapon system for the new father/knight. It still needs compatible separated
  meshes, body-hide rules, attachment/retargeting, equipment state/persistence,
  clipping/contact checks and native validation on the chosen hero.
- **Faces/hands/dialogue:** existing close-up camera, head attention and body
  gestures are present. The documented current rig has no facial morphs or
  jaw/eye/finger controls. Facial performance, lip sync, individual finger
  animation and acted dialogue remain unfinished. Four optional human WAV
  lines have a prepared script/loader; no completed recording delivery is
  established by this review.
- **Ocean/coast:** water, reflections/refraction, foam, pool/moat/pond controls
  and local shoreline shaping exist. They are not a finished authored ocean
  region or ocean gameplay. The dedicated **24 Oct** shoreline review retained
  broad bare banks and visual limitations; no new full-ocean acceptance was
  found. Current water documentation still describes analytic waves, without
  geometric wave/fluid simulation or water collision.
- **Complete demo integration:** choose which authored city/stronghold spaces
  become gameplay areas, then relocate characters/interactions, validate
  collision/navigation, rehearsal/save/retry and route performance together.
  Art-viewpoint passes cannot substitute for this step.

Source requirements:
[hero brief](/home/computer/Desktop/final_engine/new_engine/docs/visual-upgrade/2026-10-30/HERO_REQUIREMENTS.json),
[character/recording limits](/home/computer/Desktop/final_engine/new_engine/docs/slice-polish/2026-10-30/CHARACTER_ASSETS.md),
[water contract](/home/computer/Desktop/final_engine/new_engine/docs/WORLD_WATER.md).

## Performance and handoff caution

The latest dry-town captures are **visual checks, not benchmarks**. A preceding
2 Nov no-readback city/country camera replay measured 57.70 FPS mean / 48.48 FPS
1% low at 100% scene scale, with two sub-45 frames. It predates the final
ground-envelope/material revision. The 31 Oct full jungle walk measured
54.89 / 37.35 FPS at fixed 70%; the 1 Nov Grandstone flight measured
44.29 / 29.30 FPS at fixed 70%. Neither has a locked 45 FPS floor. These are
RX 5300 results for specific revisions/routes, not universal performance claims.

Evidence: [latest town captures](/home/computer/Desktop/final_engine/new_engine/docs/village/2026-11-02/dry-town-ground/native-final/result.json),
[preceding city/country timing](/home/computer/Desktop/final_engine/new_engine/docs/countryside/2026-11-02/cleanup/native-timing/result.json),
[jungle measurements](/home/computer/Desktop/final_engine/new_engine/docs/jungle-expansion/2026-10-31/validation-summary.json),
[Grandstone limitations](/home/computer/Desktop/final_engine/new_engine/docs/stronghold/2026-11-01/visual-finish/native-final-review-summary.json).

The **2 Nov Windows orientation bundle** now contains the current city, jungle and Grandstone as separate selectable projects, with updated guides and change-return tools. All archive members and manifests passed verification. Six native editor workflows, graph Compile/reload, three Linux opening views and three final Windows-under-Wine views passed. See [READINESS_REPORT](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/READINESS_REPORT.md) for exact binary provenance and the delivery link. The designer's actual Windows installation, GPU/input, small-prop save/reopen and return are first-day acceptance tasks. Opening/resource checks do not certify full walking routes or sustained performance.
