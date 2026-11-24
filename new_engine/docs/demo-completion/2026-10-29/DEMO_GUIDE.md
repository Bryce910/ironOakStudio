# Integrated Valwood demo

Launch the new Journey demo shortcut once the package is verified. The earlier polished visual demo remains available as a separate snapshot.

## Optional playable journey

Press **J** for the journal. In the castle west training court, approach the master of arms and press **E**. The first conversation offers *A place in the watch*. Choose **Begin training**, win the three non-lethal sparring rounds, then visit the jungle camp steward. Choose **Collect the trail report** and return to the master of arms. Deliver the report to earn **25 coin and one watch reputation**. The reward is granted once; sparring remains repeatable.

The two locations are far apart in the full-sized world. **F9** cycles the existing demo stops; it is useful for this review. The journal does not require walking the entire 100 km map. Free walking remains the default activity.

Combat: mouse flick or arrow keys select left/right/high/low; **T** selects thrust guard, left mouse swings, middle mouse thrusts, right mouse guards, **Space** dodges, **WASD** moves and **Esc** leaves safely. Read the incoming angle, match the guard, parry near impact and attack openings. **V** changes first/third person.

## Save and resume

**F5** or the journal's Save button writes `saves/demo.progress` inside the current project. **F8** loads it. Player feet/camera/view, quest counters and rewards, current dialogue and deferred events, NPC witnessed experiences, physical prop poses/velocities/doors, and authoritative living-world resources/schedules/debt are saved together. Save is available while walking outside an active fight/loading/editor Play session. Flying and active encounters must finish first. Authored project files are separate.

A versioned checksum and strict scene, gameplay, physics, living-definition and memory signatures reject incomplete/corrupt/incompatible files without partially replacing running progress. Updated authored scenes or memory profiles may require a new game/save; this pass does not provide old-save migration. Loading observes offline time through the existing bounded scheduler; the application does not run after it is closed.

The coin/reputation rewards are persistent demo progression. This pass does not add shopping, a full inventory, hostile-world combat, voice acting or a campaign.

## NPC memory in the actual world

The master of arms and jungle steward have explicit memory bindings. They recall shareable local knowledge, recognize repeated visits and remember witnessed sparring results. Private authored memories never appear in public conversation. Only committed gameplay events become witnessed observations; rejected/budget-deferred inputs do not create false experiences. History is bounded to 256 aggregate records across 32 minds; oldest records are evicted deterministically at capacity.

Open **F11 → Living world → NPC memory** to load a browser export. Select **Memory scene entity**, then **Bind profile to live NPC**. This uses the exact persistent scene ID. If the profile has a logical actor ID, it must match that entity's explicit living binding; set it to zero for a scene-only profile. **Save live mind bindings** embeds portable copies in `ai/npc_minds.vwminds`. Experiences are saved separately by F5. Existing appearance bindings remain independent.

A Goal memory with tags **duty** plus **guard**, **work** or **rest** can guide an explicitly bound actor to its authored destination. The strongest eligible recalled goal wins. Sleep and meals retain priority. These are physical destination decisions; economic resource proposals still use the existing authoritative scheduler/policy. No online model, automatic personality inference or generated speech runs in this pass.

The local authoring tool remains available through `Launch_Valwood_Memory_Studio.sh` at http://127.0.0.1:8871/. The portable package also includes `memory-studio.sh` and the complete offline editor. It uses Python 3 and your browser; browser drafts remain local.

## Visual and motion pass

The pool’s former broad mound is a lower western spine with two offset spurs and an open saddle. Small stone shelves support surveyed existing rigid bases at the new elevation. There are 36 additional measured fern, rock, root and deadwood placements in six authored vignettes; the nine prior source files and their placement identities stay intact. The shared CPU surface drives rendered geometry, walking, rain and asset grounding. Broken mineral exposure uses the existing baked shelter layer. Photographic leaf/stone contrast breaks the path shoulder and adds mineral/organic color variation without new textures, draw calls or animated noise.

Mapped live knights use bounded stance foot anchoring, eased planting/release and teleport reset, alongside the existing breathing idle, acceleration, turn handling and terrain IK. Directional combat uses the existing 216 owned body/arm poses, five attack families, guard, hit and parry motion. Paused/unchanged combat pose packets skip redundant GPU uploads.

Faces, individual fingers, lip sync and new hairstyles still need compatible authored assets/rig work. The full world, AAA art production and an uninterrupted FPS floor are not certified by these finite demo checks. See RESULTS.md for measured performance and exact limitations.
