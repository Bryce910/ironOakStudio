# Valwood Quest & Story Studio

A separate local authoring tool for chapters, story beats, quests, objectives and branching NPC dialogue. It follows the Memory Studio workflow and keeps editable story projects separate from engine gameplay exports.

Open [Quest & Story Studio](http://127.0.0.1:8872/) after running [Launch_Valwood_Story_Studio.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Story_Studio.sh). The local server only serves its fixed application files; it has no project-writing or installation API. Your drafts autosave in the browser, and **Save project** creates a portable file.

For Windows, extract [Valwood_Quest_Story_Studio_2026-11-02.zip](/home/computer/Desktop/final_engine/Valwood_Quest_Story_Studio_2026-11-02.zip) and double-click **Open_Story_Studio.bat** or **Valwood_Quest_Story_Studio.html**. The same standalone [HTML file](/home/computer/Desktop/final_engine/Valwood_Quest_Story_Studio.html) can be copied on its own. It has all JavaScript and CSS included and requires no internet, Python, Unity or account. Use a modern browser and save project files for reliable backup across computers; browser storage is specific to that browser/file location.

## Start writing

1. Choose **New story** or **Example**. The example plans an ordinary modern father, arrival at Grandstone, and a later oath/armor chapter. Outfit changes are story notes rather than automatic model transformations.
2. Add chapters and story beats to outline the journey. Link beats in the inspector and drag cards on the Story map. Zoom, Fit and Arrange help navigate the outline.
3. Add scene characters/objects and enter their persistent IDs from the game editor. The example's gatekeeper and oath stone are placeholder IDs and positions.
4. Add quests and measurable objectives. **Quest giver & offer** creates an accept/decline conversation and interaction rule. **Completion reward** adds a one-time counter reward on return to the giver.
5. Use Dialogue view to add lines, link choices and set conditions/actions. Flags and counters track decisions, quest state and rewards. Rules and spatial triggers connect supported gameplay events.
6. Use **Preview** to rehearse dialogue choices and send events. The quest log, flags, counters and event log show the result. Planning-only story beats have a separate walkthrough.
7. **Save project** saves all chapters, notes, graph positions and gameplay as `.vwstory.json`. **Export gameplay** saves the validated native `.gameplay` file. **Open** accepts both formats; native gameplay import cannot recover planning metadata that was never in the engine file.

Undo/redo applies to authoring changes. Preview is an isolated rehearsal and does not modify a game or spawn 3D assets. New/Open/Example protects unsaved drafts with a save-first dialog. Unfinished projects can be saved and reopened; invalid runtime definitions block engine export until corrected.

## Try the included example

Choose Example > Preview > Start preview. Send an Interact event to the Grandstone gatekeeper. Refuse to leave the quest inactive, or accept and choose “I will return.” Send Interact to the oath stone marker to complete the objective. Return to the gatekeeper to collect 25 in the Story reward coins counter. Repeating the interaction does not pay twice.

The example project and native export are included in the ZIP's examples/ folder and in tools/quest_story_editor/examples/.

## Use authored content in the engine

Work in a duplicate artist project and use fresh progress when trying changed gameplay. In **Live components**, preserve existing physics, living and entity bindings, select the exported **Gameplay asset**, **Save manifest**, then **Load manifest + referenced assets**. Startup uses `world.components` to reference the gameplay file. Dropping a `.gameplay` file into a folder by itself does not activate it. Entity identities and positions must match the intended scene; IDs remain exact decimal strings even above JavaScript's safe integer range.

Dedicated city/jungle/Grandstone environment playtests currently freeze this quest runtime. Hooking authored quests into those slices remains a game integration step. The separate builder, native parser and ordinary Components integration are implemented; this delivery does not change the game's binaries or existing projects.

Quest objectives progress concurrently while a quest is Active. Use conditions/rules/counters for ordered story steps. Reward counters do not grant physical inventory items or equip armor. Authored combat encounters, actor spawning and cinematic playback require supported host integration; the current combat host accepts its existing training arena/presentation encounter. Dialogue graphs must be reachable and acyclic. Changed gameplay uses a strict save signature; old journey saves are not automatically migrated.

## Verification

The core suite passes **76 checks**, including native C++ parsing and nine runtime/codec scenarios: refusal, pre-acceptance gating, acceptance, reminder, completion, save/resume, one-time reward, full record roundtrip and encounter completion. Independent review checks engine enum contracts, 64-bit identities, dialogue-local line IDs, blocked choices and atomic capacity limits. Seven real HTTP tests cover fixed-resource serving, traversal and write rejection, symlink/missing assets, instance reuse and source preservation.

Browser acceptance covers example loading, refusal/re-offer, acceptance, objective completion, a one-time 25-counter reward and repeated claim rejection. Actual browser-downloaded project and gameplay files roundtrip; gameplay bytes exactly match the C++-tested fixture. The editor was left open. The standalone HTML and all ZIP records pass integrity and inline-JavaScript syntax checks; the included source reproduces both artifacts byte-for-byte. The automated browser only allows HTTP/HTTPS, so direct `file:` opening and a physical Windows browser were not tested; browser UI acceptance uses the local HTTP edition.

See [core tests](core-tests.log), [server tests](server-tests.log), [independent review](review.json), [portable verification](portable-review.json), [browser export checks](browser-downloads.json), [browser workflow](browser-acceptance.json) and [final manifest](FINAL_REVIEW.json).

## Development

Core and compiler: `tools/quest_story_editor/dist/story.mjs`. UI: `index.html`, `style.css`, `app.mjs` in that folder. Local server: `tools/quest_story_editor.py`.

```sh
node tests/quest_story_editor_test.mjs
python3 -B tests/quest_story_server_test.py
node tools/quest_story_editor/compile.mjs STORY.vwstory.json NEW_OUTPUT.gameplay
python3 -B tools/build_quest_story_studio.py
```

The compile command refuses to overwrite an existing output. The portable builder likewise preserves existing delivery paths unless `--replace` is supplied deliberately during development. No external dependencies are downloaded by either command.
