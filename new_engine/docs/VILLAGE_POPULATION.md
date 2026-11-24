# Valdoa village population

The custom engine draws 64 residents in the Valdoa castle village: eight patrol guards, eight ground-level archers, ten traders and 38 travelers. Three traders and sixteen travelers use female appearances. Residents follow deterministic, nonblocking routes and idle behaviors. Walkers use separate pavement loops, a shared speed per loop, and evenly spaced cycle phases so they cannot overtake and intersect each other; this adds no per-frame neighbor solver. A 600-second all-pair regression measures at least 1.40 m between resident centers. Dialogue, commerce, autonomous planning and offline AI are not implemented.

Four existing travelers now circulate through Merchants Lane, including two
retained female civilians and two purchased male appearances. The population
and appearance counts remain unchanged. Their closed route runs between local
`z=-25` and `47`, with separate north/south pavement strips at `x=60.95` and
`62.55`. Turns remain inside the lane rather than entering either shared cross
street. Equal speed and quarter-cycle spacing retain deterministic motion
without additional runtime pathfinding or assets. All 9,600 lane probes over
ten simulated minutes clear the real village collision inventory and remain
on matching paved terrain. The four residents retain at least 1.60 m between
their centers, while the full crowd retains its 1.40 m minimum separation.

## Imported characters

Twenty-nine residents use all five male appearances from the user-provided purchased `NPC_Character_Pack_Update.zip`: Character01 for four inner-stall merchants, Character02 for eight craftsmen, Character03 for three traders, Character04 for eight wayfarers, and Character05 for six travelers. The crowd also retains nineteen female civilians and sixteen guards/archers. Four previously unstaffed inner stalls now have keepers; six extra walkers make the streets busier. Original FBXs and textures, selected archive entries, conversion details and hashes are retained in `assets/purchased/characters/`. No free or Creative Commons license is asserted for these purchased sources. [Purchased pose manifest](../projects/valwood_world/models/purchased_character_manifest.json).

The remaining residents use two free source collections:

- [System G6 / Qoma's Modular RPG Characters](https://opengameart.org/content/modular-rpg-characters), CC0: male and female civilians, apron vendors and hooded rangers.
- [Animated Defender AKA Brigand](https://opengameart.org/content/animated-defender-aka-brigand), CC BY 3.0: concept by Konstantin Maystrenko, model by AnthonyMyers, retopology/rigging/animation by Danimal. The guard carries its original poleaxe.

[Character credits](../projects/valwood_world/CREDITS_CHARACTERS.txt) record the required attribution and modifications. Original archives, license text, source pages, hashes and preparation scripts remain under `assets/characters/incoming/`. Unused downloaded candidates stay outside the runtime model set. The earlier user-supplied source models remain unchanged.

The [free character manifest](../projects/valwood_world/models/village_character_manifest.json) retains the earlier imported assets; the purchased manifest records the 44 purchased poses. `world_village_pose_archetypes` explicitly selects the active file prefix for each appearance. Both cookers validate selected output before publishing finished GLBs.

## Animation and rendering

Nine appearance aliases share 76 static pose assets:

| Alias | Walk poses | Idle poses | Residents |
| --- | ---: | ---: | ---: |
| knight | 8 | 4 | 8 |
| archer | 0 | 4 | 8 |
| trader (Character03) | 0 | 4 | 3 |
| traveler (Character05) | 8 | 4 | 6 |
| traderwoman | 0 | 4 | 3 |
| travelerwoman | 8 | 4 | 16 |
| merchant (Character01) | 0 | 4 | 4 |
| craftsman (Character02) | 8 | 4 | 8 |
| wayfarer (Character04) | 8 | 4 | 8 |

Guards use eight samples from a stable 33-frame cycle in the supplied `Walk-normal.001` clip. Guard idle breathing and a calm hand-attached weapon carry are authored. The retained female civilians use the supplied Idle clip and a reduced Run clip with neutral foot orientation and a planted sole. The purchased male source contains no animation actions: relaxed breathing idles and a two-bone IK walking cycle are authored offline, with grounded support soles. Families01/02 contain valid meshes but no armatures or skin weights in their UE4, UE5, or Unity FBXs. Their identical bind positions allow an offline closest-triangle skin-weight transfer from the supplied Character04 skeleton. Semantic head and hand donors, at most four normalized bone influences, and a measured maximum donor distance of 10.23 cm keep this adaptation bounded. Original source meshes/UVs and archives remain unchanged; `authored_skin` in the manifest records the donor hash and measured distances. These are authored poses and rig adaptations, not supplied motion-capture clips.

Runtime animation now continuously interpolates vertex positions, normals and tangents between adjacent samples on the GPU. Explicit pose loads validate exact index, UV, paint, tangent-sign and material correspondence before linking a pair. A small index-only normalization gives the knight the same triangle connectivity in every pose, preserving all other asset bytes; the source cooker repeats that normalization on regeneration. See `assets/characters/knight-topology-validation.json` for preservation proofs. No extra images or duplicated character geometry are uploaded.

For these pose assets without authored tangents, generated tangent handedness uses the largest nondegenerate incident UV-triangle determinant in the fixed oriented index order. This avoids geometry-dependent sign changes at folded clothing patches; the audit found 79 affected vertices across the original active appearances. Mixed-sign UV junctions use a documented dominant-face approximation, not a claim of exact MikkTSpace reconstruction. Authored tangent signs still require exact agreement, and ordinary scenery keeps its existing basis generation. See `assets/characters/pose-generated-tangent-audit.json`.

Walkers cruise at 1.35 m/s, with 0.35-second acceleration/deceleration ramps. Gait follows distance traveled and each appearance's measured source stride, with half strides anchored at each leg's endpoints. A 0.18-second eased transition connects idle0 with a matching planted walking pose (frame0 or frame4); dwell animation returns to idle0 before departure. Facing turns smoothly during each dwell along the shortest angle. All placement, gait and facing are functions of absolute simulation time, so teleports, skipped updates and different frame rates give the same state. The existing routes and 64 residents remain in place.

Bodies use a fixed reference scale of 1.82 m rather than renormalizing each animated frame. Body and equipment meshes have separate `Body_` and `Gear_` tags, so a tall weapon cannot shrink the wearer. The offline cook removes root translation and grounds boot soles. Source rig helpers, scenery and lights are excluded.

The female traveler's authored leg blend increases from0.42 to0.54, widening its measured two-step stride from0.89085 to1.14647 m at the shared walking pace. Its eight walking samples change; all four idle samples, texture data, UVs, indices, vertex counts and reference scale remain unchanged. Publication hashes and preservation checks are in `assets/characters/female-gait-publication.json`. Animation remains interpolation of baked poses, without runtime skeletal foot IK: intermediate knight soles can briefly lift roughly1.34 cm even though the exported keys are grounded.

The retained free civilian/ranger cloth, skin and hair maps use one 2048×1024 atlas per appearance. Each body uses one material primitive; archers add one equipment primitive. The guard retains diffuse/normal maps and six shared 1024px maps after a documented conservative conversion from legacy specular material data. Source skin and leather are kept nonmetallic. The source's constant-white vertex colors are validated and removed because the runtime does not read them. Atlas image and UV hashes must remain identical across each appearance's poses.

The active mixed set has 40 effective texture-cache keys and approximately 203 MiB of conservative RGBA8 storage including mipmaps. The purchased males use up to 17,099 triangles and three material primitives per pose; retained female civilians/rangers use approximately 2,800–3,940 triangles, and the guard has 15,953 including the poleaxe. Purchased head/clothing textures retain their source UVs and standard albedo, normal and ORM interpretation, with at most 2K images. Added01/02/04 clothing uses 1K albedo and 512px normal/ORM; added hair uses 512px albedo. This limits the three extra appearances to approximately 33 MiB of extra GPU image storage while sharing source head maps where appropriate. Shared encoded image bytes prevent repeated GPU image uploads.

Every color/reflection population draw sets the actor reactive value for residents and resets it to zero before returning. The actual GL fixture reads this reset; scenery/castle rendering precedes population rendering in each pass.

Each update fills fixed instance arrays, with no runtime file access, texture loading or application allocations. Ninety-one fixed pair batches reference the existing 76 pose buffers: 76 adjacent pairs and fifteen walk/idle handovers. Only occupied groups within 220 m are submitted. Color, reflection and shadow passes reuse the same uploaded placements and blend weights. The rigid foliage material path disables vegetation wind; moving actors retain the temporal reactive mask. Morph mode resets after each draw so scenery and foliage keep their normal rendering. The crowd has nine repeated appearances, not 64 unique character models.

## Tests and regeneration

CPU tests inject all 76 partial-load failures, check role/appearance placement including the four new shopkeepers, probe ten simulated minutes of paths against village terrain and architecture (153,600 resident probes, including 9,600 in Merchants Lane), and verify deterministic culling, closest-resident queries, zero hot allocations and complete cleanup. Every sample checks exactly four travelers occupy the authored lane, lane pair clearance and all crowd pairs; a second population receives skipped updates to confirm absolute-time determinism. The actual GL fixture checks all 76 encoded assets, tagged body bounds and soles, geometry/image budgets, exact effective texture deduplication, visible animation, shadows, temporal mask reset, reflection reuse and zero remaining texture owners. Native captures judge appearance and native profiles verify frame cost.

The continuous-motion regression also samples600 seconds at60 Hz, including3,503 exact-pose handovers and467 opposite-foot stops with the final gait. It checks acceleration, wrapped facing continuity, fractional pose weights and deterministic skipped updates. The GL integration links all91 real pose pairs and checks192 fractional samples across three rendered60 Hz frames, with matching color/shadow depth and a shared reflection snapshot. A separate transform-feedback fixture verifies exact endpoints, intermediate positions, orthonormal tangent frames, draw isolation, invalid input transactions, strict correspondence rejection and generated-basis controls.

Final build/regression logs and actual RX5300 captures/profiles live under `docs/performance/npc-animation/`. Capture and timing are separate: the capture-only follow camera and360-frame readback are disabled for timed profiles and ordinary play. Reproduce the native QA on this machine after closing the game:

```sh
VALWOOD_TEST_NATIVE_GL=1 python3 tools/capture_npc_animation.py
VALWOOD_TEST_NATIVE_GL=1 python3 tools/benchmark_npc_animation.py
```

The final RX5300 run at1600×900 with a60 FPS cap measured:

| Test | Average FPS | 1% low FPS | Worst frame ms |
| --- | ---: | ---: | ---: |
| Market | 59.95 | 59.66 | 17.093 |
| Stalls | 59.95 | 59.66 | 16.912 |
| Merchants Lane | 59.95 | 56.73 | 25.895 |
| Full806 m walking route | 59.95 | 57.54 | 24.657 |
| Lane confirmation | 59.95 | 57.37 | 26.591 |

Across20,760 measured frames, nine isolated frames exceeded the22.22 ms45 FPS budget; none exceeded33.33 ms and every over-budget streak was one frame. The route completed all806 m with zero blocked/sliding/slope steps, failed tiles or return error. These results preserve the45–60 FPS target for typical play but do not establish a strict45 FPS floor. Source/assets stayed unchanged across the main profiles. `docs/performance/npc-animation/verification.json` collects proofs, raw reports and the three60 Hz native walking clips.

Regenerate the retained free source poses from the engine root:

```sh
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup --python-exit-code 1 --python tools/build_village_characters.py
```

Append `-- --roles knight,archer` for a selective recook after the first complete import. The main builder calls `tools/village_defender.py` and `tools/village_civilians.py`; original downloaded source files are retained unchanged.

Regenerate the purchased male replacements with:

```sh
OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=1 blender -b -t 2 --factory-startup --python-exit-code 1 --python tools/build_purchased_characters.py
```
