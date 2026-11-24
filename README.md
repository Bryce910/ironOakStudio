# Legends of Valwood

Source repository: [Bryce910/ironOakStudio](https://github.com/Bryce910/ironOakStudio).
See [the Git workflow](docs/GIT_WORKFLOW.md) for commits, pushes, the current source
snapshot, and the local assets required to rebuild or run the game.

The current Linux demo, editor, combat arena and joust use one executable and the same lighting, material shaders and character texture bank. The demo and editor share the saved working world at `new_engine/projects/valwood_current`.

- [Play the demo](/home/computer/Desktop/final_engine/Launch_Valwood_Demo.sh): apartment, arrival in Seldonne and the first quest.
- [Open the editor](/home/computer/Desktop/final_engine/Launch_Valwood_Editor.sh): edit that same world.
- [Combat arena](/home/computer/Desktop/final_engine/Launch_Valwood_Arena.sh) and [joust](/home/computer/Desktop/final_engine/Launch_Valwood_Joust.sh): separate practice scenes using the same engine.
- [Runtime and saving guide](/home/computer/Desktop/final_engine/new_engine/build/unified-runtime-2026-11-16/UNIFIED_RUNTIME_GUIDE.md): shared graphics preferences, saves and content workflow.

`./Launch_Valwood.sh demo|editor|arena|joust|world` selects a mode. Save and close the running mode before switching. F10 opens graphics settings, including optional original character color textures up to 4K. The active generation is recorded in [CURRENT_VALWOOD.json](/home/computer/Desktop/final_engine/CURRENT_VALWOOD.json).

The main character uses the purchased Thomas model with Kubold motion capture. Third-person play is the default, with the same face and hair when armor equips. In **F10 → Quality → Characters / models**, choose **Low, Medium, High or Ultra**, apply and restart. Low and Medium reduce garment geometry and textures; High retains the full mesh; Ultra adds original source texture detail up to 4K. Every tier keeps the animated face and hair topology. The character has natural blinks, subtle gaze and brow expressions, 22 captured exploration starts/stops/turns, and dedicated brunette hair shading. The latest update smooths start-to-loop and pivot-to-stop handoffs, removes artificial walking hops, and delays new foot plants until the blended stop pose settles. The preceding hair transparency correction is retained. See [integration and validation notes](/home/computer/Desktop/final_engine/new_engine/build/thomas-animation-polish-2026-11-21/GUIDE.txt) and the [updated 60 fps character video](/home/computer/Desktop/final_engine/new_engine/build/thomas-animation-polish-2026-11-21/review/video/Thomas-corrected-hair-and-movement.mp4). First-person hands and sleeves remain available. Dialogue lip sync, Unity cloth/hair simulation and complete HDRP shader parity remain unfinished.

Vegetation now has real **Low / Medium / High / Ultra** generation and draw budgets in **F10 → Quality → Vegetation → Grass and undergrowth detail**. Apply updates the scene without restarting. Grass budgets range from 23,000 to 92,000 roots; fern budgets range from 360 to 3,600. The main world includes denser creek-bank and camp dressing, and the authored scenery ceiling is now 65,536 records. Solid trees/rocks stay consistent across tiers. See the [vegetation guide](/home/computer/Desktop/final_engine/new_engine/build/vegetation-quality-2026-11-21/GUIDE.txt), [before capture](/home/computer/Desktop/final_engine/new_engine/build/vegetation-quality-2026-11-21/review/previews/Before.png), and [Ultra capture](/home/computer/Desktop/final_engine/new_engine/build/vegetation-quality-2026-11-21/review/previews/Ultra.png). The separate lighting candidate has not been merged; the denser canopy still needs lighting refinement.

World art and NPC content still vary between scenes. Sharing the renderer does not replace unfinished scenery or character geometry. Older Windows packages below are historical snapshots and have not been rebuilt for this update.

## Earlier deliveries and supporting tools

**Gameplay area slices (2 November):** use [Launch_Valwood_Playtest.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Playtest.sh) to choose city/countryside, jungle or Grandstone. Start with the armed hero in third person; E challenges the nearby practice knight. Each has attack/parry, defeat/retry, free exploration and separate saves. Controls and the updated Windows add-on are in [PLAYTEST_SLICES.md](/home/computer/Desktop/final_engine/PLAYTEST_SLICES.md).

**Designer orientation (3 November):** the verified [Windows orientation ZIP](/home/computer/Desktop/final_engine/Valwood_Windows_Orientation_2026-11-02.zip) includes the current city/countryside, expanded jungle and Grandstone. Begin with [ARTIST_START_HERE](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/windows/ARTIST_START_HERE.md). Review scope, tests and remaining editor limits are in [READINESS_REPORT](/home/computer/Desktop/final_engine/new_engine/docs/editor-readiness/2026-11-02/READINESS_REPORT.md). On this Linux computer, use `Launch_Valwood_City_Editor.sh`, `Launch_Valwood_Visual_Editor.sh` or `Launch_Valwood_Stronghold_Editor.sh`.

**Presentation preview (30 October):** run `Launch_Valwood_Presentation.sh` for **A Place in the Watch**, the guided castle–woodland journey with a trial, crossing encounter, NPC recognition, checkpoints and an ending. See [the presentation guide](new_engine/docs/vertical-slice/2026-10-30/PRESENTATION_GUIDE.md).

Run `Launch_Valwood_Castle_Village.sh` to start in the detailed Merchants Lane in the native world engine. WASD walks, right mouse looks, Shift sprints and H shows the travel/lighting tools. Use the scenic list to visit the fuller market stalls, market square and castle courtyard; the lighting controls include the purchased village color grade.

Run `Play_Seldonne.sh` to play the starting area, or `Launch_Studio.sh` to open the authoring workspace. Ctrl+Enter switches between them.

Engine sources, project content and build instructions: [new_engine/README.md](new_engine/README.md).

Latest village materials, lighting, street dressing, native performance and captures: [verification](new_engine/docs/performance/village-presentation/verification.json).

Measured rendering/performance changes and gameplay captures: [results](new_engine/docs/performance/RESULTS.md).

AAA production target and remaining work: [roadmap](new_engine/docs/PRODUCTION_ROADMAP.md).

The lore-based development specification remains in `specs/Legends_of_Valwood_Development_Specification.docx`.

## Quest and story authoring

[Launch_Valwood_Story_Studio.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Story_Studio.sh) opens the separate quest/story builder. [Windows offline package](/home/computer/Desktop/final_engine/Valwood_Quest_Story_Studio_2026-11-02.zip), [standalone HTML](/home/computer/Desktop/final_engine/Valwood_Quest_Story_Studio.html), and [workflow](/home/computer/Desktop/final_engine/QUEST_STORY_STUDIO.md). Story export uses the engine's native gameplay format; scene bindings and playtest activation are explicit integration steps.
