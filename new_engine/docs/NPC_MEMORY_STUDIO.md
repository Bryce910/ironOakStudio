# Valwood Memory Studio

Local browser authoring for a single NPC's identity and branching memories. Launch `Launch_Valwood_Memory_Studio.sh` from the repository root. The launcher starts or reuses a loopback-only service, normally at `http://127.0.0.1:8871/`. Python3 is the only launcher dependency. The browser editor has no build step or external dependencies.

## Author a mind

New NPC starts with exactly one core profile. Edit its name, role, faction, personality and backstory, select a category, and add a memory. The selected unit becomes the new memory's parent. Select the core again to create another main branch. Each bright dot is an authored unit; the animated signals are visual decoration.

Expand **Physical attributes** on the core to author height (cm), weight (kg), age (years), body build, skin tone, eye color, hair color/style/texture, strand thickness, density, facial hair and distinguishing features. Thickness describes individual hair strands (fine/medium/coarse); density describes scalp coverage (bald/sparse/medium/dense). Descriptive text stays free-form for fantasy characters. Measurements may be left blank, and dropdowns may remain unspecified. A new or migrated profile makes no assumptions about its body or hair.

These attributes are saved profile data, available through `Bank::document().profile.appearance`. The native inspector can now apply supported attributes to a specific 3D crowd resident. They do not infer personality, change recall scores, reassign living-world IDs or select a different mesh.

## Apply appearance in the engine

Load the exported profile in **F11 → Living world → NPC memory**. Under **Apply to a 3D resident**, select the visual resident slot and choose **Apply profile appearance**. The inspector identifies the character, location, neutral height and supported hair features. This visual target is explicit and separate from `profile.actor_id`; applying a profile never guesses a person from a repeated role name. Other residents sharing the same source asset retain their own look. The current scope is the140 stable built-in crowd slots, including the tavern neighbors; independent live skeletal actors and newly authored actor slots need their own adapter.

- Height scales the neutral model envelope in metres. Walk/idle contact correction scales with it, and stride timing accounts for height and width. Neutral height includes the source's hair, headwear and held equipment; it is not a measured bare anatomical skeleton.
- Build and weight alter cosmetic XZ width, bounded0.65–1.45. Recognized builds are slim, lean, average, athletic, sturdy, stocky, broad and heavy. Weight contributes a bounded volume approximation, not physical mass, muscle simulation or collision-body resizing.
- Separate supported hair materials accept black, dark/light brown, brown, blonde/blond, red, auburn, gray/grey, silver, white or `#RRGGBB`. Tint preserves source strand shading. Baldness hides that hair material in all passes. Combined clothing/skin/hair atlases are retained.
- On Sintel's existing alpha hair cards, density and thickness adjust cutoff and alpha shape using the same values for color/reflection/shadows. These controls change existing card coverage, not individual geometric strands or a new groom. Solid purchased hair supports color and baldness; it cannot change strand coverage.
- Skin tone, eye color, age, hairstyle/texture, facial hair and distinguishing features remain descriptive until compatible masks, meshes or morphs are authored. The inspector reports these limits. No additional asset purchase is needed for the features above.

**Clear this resident appearance** restores the original. **Save appearance bindings** saves `ai/npc_appearances.vwlooks` and imports immutable validated profile snapshots into `ai/memories/appearance_imports/` using content-derived names. Existing snapshots are compared, never overwritten. These relative paths keep a saved world portable. Reload or the next engine launch validates every bound profile before applying the saved set; failed loads preserve the current appearances. To update a profile, export and load its new version, apply it, then save again. Unreferenced older snapshots may remain as authored resources.

The render path reuses the original instanced pose batches, meshes and texture owners. Only crowd pose batches receive an optional32-byte appearance stream; ordinary scenery instance buffers retain their previous layout. Width uses inverse-transpose normals, forward tangents and expanded committed culling bounds. Profile IO and binding changes occur at explicit authoring/loading boundaries; ordinary animation updates retain bounded fixed arrays and no heap allocations. Appearance editing is disabled during Play.

The neural view supports drag-to-orbit, individual node dragging, depth editing, a pulsing core and signals along real branches. The branch map provides a flat circular editing view. Use the wheel or zoom buttons to enlarge the network. Arrange redistributes large root sets across expanding rings. Animation can be paused and starts disabled when the browser requests reduced motion; background tabs stop their animation loop. No Framer component code is required; the user's Neural Brain reference informed the visual direction.

Memories have an event/relationship/belief/goal/knowledge/skill category, description, trigger tags, importance, confidence, emotional tone, source, sharing permission, occurrence hour and optional decay half-life. They can be enabled or disabled as branches. A child inherits its ancestors' tags. A disabled or not-yet-occurred ancestor prevents child recall. Confidence expresses the NPC's certainty, not whether the remembered claim is objectively true. Hearsay and witnessed memories remain labeled separately.

Dragging and arranging change layout only. Importance and confidence are explicit values. Branches are a rooted tree; arbitrary cross-links and cycles are not supported in version1. Maximum256 memories,16 levels,8 tags per memory. Search and the branch list support navigation. Deleting a branch removes its descendants; Undo restores the complete branch. The browser keeps32 undo snapshots during the session.

## Save and export

- **Save project** downloads `NPC_ID.vwmemory.json`, containing the profile, memories and editable3D layout. **Open project** restores this versioned JSON. The sample project is under `tools/npc_memory_editor/examples/`.
- **Export to engine** downloads `NPC_ID.vwmemory`, a bounded text document. It contains the core profile, physical attributes, knowledge and semantic parent IDs, but omits visual positions. Files are data; neither the engine nor editor executes memory text as code.
- The current valid draft is saved in this browser's local storage. Use a project download as the portable backup. Local storage is per browser and origin, not a cloud backup. Invalid drafts cannot be exported and do not replace the last valid saved draft.
- Developer command: `node tools/npc_memory_editor/compile.mjs INPUT.vwmemory.json OUTPUT.vwmemory`. It validates the source and refuses to overwrite an existing output.

For engine use, place the export under the active project's `ai/memories/` directory, or enter its absolute path in the inspector. After starting the rebuilt engine, press **F11**, open **Living world**, expand **NPC memory**, enter the path and choose **Load NPC memory**. The supplied default is `ai/memories/npc_mara_fenwick.vwmemory` in the Valwood project. Type context tags and a game hour, then choose **Preview NPC recall**. A failed load retains the previous committed bank and results. The package validator also checks `.vwmemory` files in the project's `ai/` tree.

Actor ID0 means unbound. A nonzero ID references the persistent logical `world_living::Actor::id`, not a mesh index or camera-dependent resident lookup. The inspector reports whether the actor exists. The sample Mara is an authored example with actor ID0; loading it does not rename or reassign an existing resident.

### File compatibility and attribute bounds

New project JSON and engine exports use version2. Existing version1 projects and browser drafts are validated before being migrated, preserving memories, IDs and layout and adding unspecified attributes. Existing version1 engine exports still load in the rebuilt engine. Version2 exports require this updated engine; restart an older running executable before importing them.

JSON stores unknown measurements as `null` and unknown text as an empty string. The engine stores measurements as `std::optional<double>`; age zero is distinct from unspecified. Bounds allow fantasy NPCs: age0–10,000 years, height1–1,000 cm and weight0.1–10,000 kg. Each short descriptor is limited to64 UTF-8 bytes, and distinguishing features to512. Invalid attributes reject an import atomically. Source rigs, authored roads and animation quality may constrain sensible visual sizes more tightly than these data bounds.

In version2 engine text, `APPEARANCE` follows `TRAITS`: age, height, weight, then quoted build, skin tone, eye color, hair color, hair style, hair texture, hair thickness, hair density, facial hair and distinguishing features. Numeric `-1` means unspecified in this transport format only; all other values undergo the bounds above. The browser retains nulls rather than exposing that sentinel. Version1 omits this record.

## Engine API and recall rules

`engine/world_npc_memory.h/.cpp` provides `Bank::load`, transactional `configure`, and allocation-free `recall`. A query supplies up to16 exact canonical tags, a game hour and an optional `shareable_only` filter. The result contains at most8 ranked references into the committed document. All text remains owned by the bank, valid until the next successful configuration. There is no file IO, network access or GPU work in recall.

For each eligible memory:

```
relevance = query empty ? 0 : min(1, unique inherited matches / min(3, unique query tags))
age_weight = half_life == 0 ? 1 : 2 ^ (-(current_hour - occurrence_hour) / half_life)
score = (0.5 * importance + 0.2 * confidence + 0.1 * abs(emotion) + 0.2 * relevance) * age_weight
```

A nonempty query requires at least one matching inherited tag. Disabled/future branches are excluded. Private memories remain available for internal recall; `shareable_only` excludes them from a dialogue-facing query. Score ties sort by stable memory ID. Negative emotions increase salience through their magnitude; the sign remains available to downstream behavior code. A decay half-life changes retrieval weight and never deletes authored content. Times use a caller-supplied game clock; the economic simulation can derive hours from its persisted tick and timestep. The browser preview implements these exact rules.

```cpp
world_npc_memory::Bank memories;
std::string error;
if (memories.load("ai/memories/npc_mara_fenwick.vwmemory", error)) {
    world_npc_memory::Query context;
    context.tags[0] = "player";
    context.tags[1] = "jungle";
    context.count = 2;
    context.game_hour = 72;
    context.shareable_only = true;
    const auto result = memories.recall(context);
    // Read memories.document().memories[result.hits[i].index] on a decision.
    // The consumer still decides permitted dialogue/actions using world rules.
}
```

This version authors, validates, imports and retrieves NPC knowledge. It does not train model weights, generate dialogue, infer semantic synonyms, append gameplay experiences automatically, or alter existing NPC decisions. Those consumers are the next integration step. Querying the bank on decisions rather than every render frame preserves its bounded work. No new game FPS result is claimed for this change.

## Verification — 29 October2026

- Strict C++ build with `-Wall -Wextra -Werror` for the memory module/tests.
- `make test-world-npc-memory`: atomic invalid imports, UTF-8 and text budgets, duplicate/missing/cyclic nodes,16-level boundary,256-node bank, privacy, inherited tags, duplicate query tags, future/disabled branches, half-life decay, top8 ranking, and2000 full-bank recalls without heap allocations.
- 240 exact browser-domain/C++ parity cases, including escaped quotes, backslashes, Unicode and multiline content. Large arranged root nodes retain at least70 units of separation.
- Full engine and native project validator built successfully. Current Valwood project validation passed with3 checked documents, including the new sample memory.
- Real browser QA: initial one-core state; example; individual dragging; map/neural modes; pause; title editing; nested branch creation; branch deletion/undo; recall results; draft restoration; project and engine downloads. An observed edit-on-blur persistence issue was fixed by committing on input with grouped undo.
- The actual browser-downloaded engine export was read by the C++ implementation: rescue0.8983333333333333, brother0.7766666666666666, rumor0.6683333333333333 for `player,jungle,bandits` at hour72.

The delivered editor server serves only fixed app resources and health, with no upload, install or filesystem-write endpoint. All project/engine exports are explicit browser downloads. No NPC files or purchased assets are uploaded to another service.

### Physical attributes verification

`make test-world-npc-memory` also checks version1 JSON migration without mutating the source, version1 engine imports, unspecified measurements, age zero, fractional measurements, invalid text/enums/ranges, and every attribute through the actual C++ file loader. Escaped, multiline and Unicode appearance text survives export. Appearance changes leave the existing240 recall parity cases unchanged. Browser QA covers core controls, invalid height rejection and reload persistence. The full engine build is current with the new inspector.

### Appearance runtime verification

Evidence is in `docs/npc-memory/2026-10-29/appearance-runtime/`. The strict population CPU tests cover per-resident isolation, rooted dimensions, input rejection, reset, all-or-nothing multi-target reload and binding persistence while retaining zero hot-path allocations. Memory tests also import portable, immutable profile snapshots and preserve prior bindings on a failed Save. All27 asset-inventory tests and11 package tests pass, including the new `.vwlooks` dependency closure. The native project validator passes.

Actual OpenGL tests use the owned character poses: hair tint changed27 pixels while depth stayed bit-exact; height/build changed the silhouette; baldness removed41 hair depth pixels; masked density/thickness affected coverage; and clearing restored the original RGBA/depth exactly. Original full-crowd, reflected-frustum and color/shadow consistency checks also pass, with no additional texture owners.

A limited RX5300 market diagnostic at1600×900 measured59.81 FPS mean and47.54 FPS1% low, with one22.341 ms frame (just below45 FPS). Its legacy benchmark wrapper rejected the current shadow preset against an older3072-pixel requirement, so it is not final route acceptance or proof of an always-above45 FPS result. Combined castle/jungle route validation belongs to the coordinated demo-polish pass on the frozen final binary.

### Character shader isolation and performance repair

The coordinated full jungle route exposed a shared-shader regression in the first appearance implementation. Appearance declarations, extra flat outputs, body shape and hair shading are now compiled only with `VALWOOD_CHARACTER_APPEARANCE`. Ordinary scenery, wildlife, rigid props and skeletal programs compile without these inputs or branches. A dedicated crowd color/depth pair uses the existing lighting setup for main, reflected and loading-warm passes. Static probe captures remain static. Both programs participate in the same shader reload/rollback ownership and epoch invalidation as other live programs. Program moves transfer handles uniquely, including a successfully linked candidate rolled back before a scene commit.

Neutral crowd appearance data performs no extra buffer upload. Unchanged custom data reuses a reserved snapshot and skips redundant uploads too. The population uniform cache retains all four crowd/player programs to prevent driver-query churn. Tests verify inactive generic character inputs, bit-exact neutral RGBA/depth/shadows between variants, custom/neutral/clear transitions, upload counts and all prior appearance behaviors. Native QA also checks successful-link rollback, moved-from cleanup, failed linking and a successful commit with ten uniquely owned programs; GL errors and remaining texture owners are zero.

Evidence: `docs/npc-memory/2026-10-29/character-variant/`. Under matched1800-frame GPU timer conditions at the same saved quality, the RX5300 diagnostic improved from48.14 to53.56 FPS mean and40.90 to45.79 FPS1% low. Mean GPU time fell19.878→17.820 ms; forest10.582→9.209 ms; reflection3.527→2.904 ms; terrain remained1.482→1.483 ms. Sub45 frames fell310→2, with22.720 ms worst after the fix. These timer-assisted diagnostics support the targeted repair; they are not full-route certification or an every-frame45 FPS guarantee. Diagnostic binary SHA256: `9bc3da7eb81727b34b6f12303ce8312001792eabcb81a83911aefe4f88b777d4`.

The final loading-warm helper also binds imported surface GPU resources for each generic/crowd program, matching main/reflection preparation. The rebuilt final startup/rollback check passes on the RX5300 with GL0, no remaining texture owners, no failed terrain tiles and unchanged world/settings. Final frozen binary SHA256: `2e1ada962e698b9df92e87244ce7a662ccb9b4936ced0dafae296f45439311ec`; evidence `character-variant/native-final/results.json` and `character-variant/final-build.log`. The earlier matched diagnostic is preserved under its own binary identity.

## Live behavior and witnessed experience

The integrated demo now consumes exported memories in the actual runtime. In F11 → Living world → NPC memory, choose a persistent Memory scene entity and Bind profile to live NPC. The profile's nonzero actor ID must match its explicit living binding. Save live mind bindings embeds the validated profile and conversation tags in `ai/npc_minds.vwminds`, so packaged games have no dependency on the export's original path.

Shareable recalled knowledge appears in that speaker's real conversation. Committed meetings/choices and nearby witnessed training outcomes form bounded runtime experience and recognition. Private authored memories remain private. Goal memories tagged `duty` and `guard`/`work`/`rest` guide the actor's existing authored physical destinations while preserving sleeping/eating priority; they do not rewrite economic resource rules. Recall/observation use fixed storage at frame boundaries and cached destination decisions, without inference/network/IO or hot allocations.

Use F5/F8 to save/load player progress together with witnessed memories, quests, physical props and the authoritative living world. Editor Play rollback includes NPC experience. The versioned progress file checks full system signatures and rejects incompatible profile/scene changes transactionally. Authored mind bindings and personal progress remain separate assets.
