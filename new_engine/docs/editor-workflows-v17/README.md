# Designer editor workflow revision (native v17)

The editor launcher uses `build/editor-quickspots-2026-11-10/runtime-native-v17` after its readiness check. This revision was built from the isolated `source-v17` snapshot; the working Valwood authoring project is not rewritten by installation.

## Controls

- Select one or several objects. Arrow keys nudge in X/Z, Page Up/Down nudge height. Shift multiplies the step by 10; Ctrl uses a tenth of the step. The Move step field accepts 0.01–10 m.
- The Objects selection panel toggles Local axes for a single object; World axes is the default. Snap and yaw increments remain adjustable. The drag label shows moved metres.
- The right Details panel starts with five workflows: Terrain, Vegetation, Structures, Water, Living World. The subtool menu lists the current workflow, and All 27 specialist tools keeps the complete tool set accessible.
- The top action bar keeps Apply, Save All, Undo/Redo, transform modes, Snap, Frame and Play Here visible. Copy/Paste, Audit, terrain snap and default-start Play are under More; Audio, Weather and VFX are under Tools. Their keyboard shortcuts still work.
- The outliner shows Imported scenery as a closed section above the Authored scene. Open it to browse objects near the camera (150 m), buildings, trees/plants, ground/water, props, other items or all imported objects. Search spans every area regardless of the chosen filter, and a selected imported object stays visible even when outside the active filter.
- Landscape shows a filled draft surface during sculpting. A release publishes the terrain; the incremental validator checks the changed sculpt tiles and their neighbors when road/water paths are unchanged.
- Vegetation painting offers per-layer coverage multipliers, a density preview in the viewport, and a candidate-cell estimate and streaming-cost warning.
- River editing previews channel edges and outer banks. Existing numbered handles remain draggable for walls and bridges, with geometry/collision rebuild after commit.
- Audit beside Save All scans for invalid documents, possible floating exterior props, risky solid-box doorway assets and nearby blocking props, broken NPC routes, dense foliage regions, and missing/untextured model files. Findings are advisory and do not rewrite the draft.
- Play Here uses the selected object or picked ground. It seeks a nearby walkable point. Click Return to Editor or press F7 to discard playtest changes and restore the editor camera and selection. Ordinary Play keeps its previous behavior.

The density and audit checks are estimates/heuristics; the audit reports candidates for designer review rather than proving all collisions or render costs. Terrain fill is an immediate shape preview; final material and lighting still appear after release.
