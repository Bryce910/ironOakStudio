# Core editor workflow review — 2026-11-02

The CPU workflow checks pass, including a reproduced and repaired imported-source transform reset. This review did not open a GPU window or modify an art asset, project, shader, or release binary. Native mouse/gizmo/render acceptance is a separate root-owned check.

## Repaired issue

The viewport gizmo created a source override with default pitch, roll, and independent axis scales. Moving an already tilted/stretched imported model therefore reset its shape on the first drag. The source-hide paths had the same initialization problem, visible when restoring an object later.

`engine/world_studio_tools.cpp` now retains the effective source pitch/roll/axis scales when transforming or hiding it. `engine/world_designer.cpp` retains them when deleting/hiding a lazy source through designer commands. A regression failed before the fix at the tilt-preservation assertion and passes afterward. A second designer regression verifies hide/restore without changing the source shape.

Evidence: `source-transform-before.log`, `source-transform-after.log`, and `designer-source-hide-after.log` in this directory. Engine source was frozen before the parent rebuild.

## Checks performed

| Workflow | Evidence/result |
|---|---|
| Source selection and persistent identities | Designer324 checks after the new regression; source reorder/replacement, locks, mixed groups, cancellation, protected live references, and atomic failure paths. |
| Large private-scene picking |8,000 source objects,353 rays; indexed and linear selections agree. Scene-index suite:2,328 checks. |
| Clipboard and repeated house rows |76 checks: transformed mesh bounds, all row directions, nonuniform scale/roll, occupancy, undo branches, snapshot ownership, limits, group/attachment copying, and failed-paste rollback. |
| Terrain snapping |44 checks: negative mesh minima, renderer support offset, rotated/stretched bounds, sampled uneven terrain, groups, source overrides, repeated no-op snaps, undo/redo, locks and invalid terrain rollback. |
| Viewport and grid interaction |CPU viewport suite verifies DPI/projection, input ownership and guarded shortcuts. Source review confirms move/yaw snapping quantizes the gesture delta; Ctrl also enables it. |
| Asset browser/workspace |7,000-entry clipped browser fixture; metadata/search/safe payloads, bounded preferences and atomic workspace saves. |
| Full transforms |1,092 matrix contracts plus pitched/rolled/stretched transforms, shear rejection, exact picking and scene/prefab persistence. |
| Save/reopen and undo/redo |Studio atomic persistence/history suite; recovery suite covers debounce, latest-edit flush, corrupt/stale recovery, interrupted writes, worker join, bounded generations, and Play rollback/Keep. |

The main CPU run is recorded in `workflow-cpu-tests.log`; additional recovery/transform results are in `recovery-transform-tests.log`.

The new `tests/world_editor_private_project_test.cpp` also loads the actual current documents, creates only temporary authored objects, copies a pitched/stretched house at city-scale coordinates, checks equivalent transform matrices, undoes/redoes, saves/reopens a `/tmp` copy, and verifies the input project bytes stay unchanged. All three passed (`private-project-roundtrip.log`):

- Current city: `projects/valwood_city_showcase/world.studio`,99 asset families,0 added placements.
- Current Grandstone launcher target: `projects/valwood_showcase/world.studio`,121 asset families,0 added placements.
- Legacy stronghold: `projects/valwood_stronghold/world.studio`,86 asset families,982 added placements.

Undo deliberately retains the issued entity-ID high-water mark. The test compares document content after undo without requiring that allocator watermark to decrease. Clipboard transforms may use an equivalent uniform/axis-scale factorization; the matrix check verifies the shape stays unchanged.

Reproduction commands, from `new_engine`:

```sh
make test-world-designer test-world-editor-optimization test-world-studio test-world-studio-tools
make test-world-production-recovery test-world-full-transform
```

These targets build temporary test executables in `/tmp`. The checked prerequisite plan did not rebuild shared objects.

## Limits that matter for tomorrow

**City structural collision is not synchronized with Studio scenery edits.** The current imported houses use collision0; their exact UCX mesh is loaded once from `city.physics` into a separate runtime world. Moving, duplicating or hiding a house changes its scenery representation but does not rebuild that fixed collision. The new persistent city-architecture inspector notice states that city collision must be rebuilt and walking routes verified after structural edits. It also covers selected groups and copies. Decoration remains editable. This warning documents a limitation; it does not implement collision synchronization.

**Snap to Terrain follows the active height service.** It seats transformed bounds over the highest sampled support and preserves orientation. It does not rotate a building to the surface normal, sculpt a flat plot, or regenerate an independently imported paving mesh. The city paving mesh and generated height fields remain separate authoring resources. A terrain or source-ground rebuild is needed for structural paving changes.

**Grandstone's authored architecture is spatially combined into source chunks.** Picking geometry inside one of these models selects the chunk. It does not expose each original pillar, doorway, or ornament as an independent editor entity. Edit/reimport the original module or recook the source composition when individual module changes are required. New individual props can be placed separately.

**Operational limits are bounded.** A selection contains at most128 entities; added Studio placements and persistent graph nodes are limited to1,024 each; source models to8,192; history retains32 checkpoints. These are enforced limits, not guarantees that a large art addition will fit this GPU's memory. Copy/paste separates matching occupied roots and uses oriented model bounds; it is not a general physical collision-avoidance solver.

The custom asset palette is limited to128 families. Current Grandstone already uses121, leaving7 additional family slots; replacing/reimporting an existing family avoids consuming another slot. The legacy stronghold's982 added placements should not be confused with the current launcher target's0 added placements.

Native acceptance should explicitly confirm click selection, visible gizmo response, Ctrl snapping, three sequential visible pastes, End snapping, Escape cancellation, undo/redo, Apply+Save, and reopening a temporary project copy. A structural city move additionally requires rebuilt UCX before walking-route approval.
