# Valwood engine and starting area implementation plan

## Objective

Build on the native SDL2/OpenGL engine to deliver a directly launchable cinematic fantasy starting area and a usable authoring workspace. Model importing, landscape editing, lighting, materials, scene persistence, and switching between editing and playing are the immediate priorities. AAA visual fidelity is the production target and must be evaluated in actual gameplay captures.

## Execution order

1. Establish a reproducible baseline. Build the engine, repair missing portable dependencies, inventory the existing authoring tools and assets, and add a bounded launch/capture path for repeatable visual checks.
2. Make the starting area a project. Keep sample scene data separate from the existing authored world. Provide clear launch commands, a named project, saved settings, and direct access to editor and play modes.
3. Improve the authoring workflow. Expose a coherent workspace for the scene hierarchy, asset library and model importing, transform and material inspection, landscape brushes, lighting, and saving. Connect controls to real engine operations and preserve existing tools where useful.
4. Build the Seldonne starting area. Author a walkable arrival route, varied woodland, a river or water feature, a settlement or landmark, environmental props and artifacts, lighting, and navigation landmarks using available assets. Provide reliable player and editor spawn points.
5. Improve the rendering path against the sample. Prioritize correct texture color space, material response, shadows, atmospheric depth, water and foliage, and filmic presentation. Keep resource and frame time limits visible. Do not label an unvalidated renderer as AAA quality.
6. Verify and deliver. Build from source; run meaningful checks for project paths, loading and saving, importing, and launch options; render and inspect the starting area and editor; fix observed defects; document controls, evidence, and remaining production gaps.

## Acceptance criteria

- A documented command launches the named starting area in play mode, and another opens its editor.
- The sample preserves the original world files and can be reopened with its own scene data.
- Model assets can be discovered/imported and placed through a functioning editor workflow.
- Landscape and lighting controls change the real scene and support persistence.
- The starting area has a coherent cinematic fantasy composition, usable ground and collision, environmental detail, and readable lighting.
- A bounded capture path produces repeatable screenshots without requiring manual input.
- Build and run evidence distinguishes verified behavior from remaining AAA production work.

## Planning constraints

The current engine already contains model loaders, landscape and prop editing, an ImGui shell, lighting, water, and saving. The first implementation should make those systems work together before replacing them. Valwood must remain the owner’s custom engine. Renderer, editor, asset-pipeline and runtime improvements must be implemented in this codebase. Adopting another general-purpose game engine is outside the project scope.

## Progress

- Planning: complete.
- Prototype implementation and verification: complete for the delivered starting-area and authoring workflow.
- Cinematic/resource/frame-pacing pass: built and tested; see `performance/RESULTS.md`.
- AAA production target: ongoing; see `PRODUCTION_ROADMAP.md`.
