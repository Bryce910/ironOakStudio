# Editor Mutation Inventory

Scope: first-pass audit of editor mutations for `props`, `markers`, `spawns`, and `NPCs`.

Goal: identify every current mutation entry point, classify its behavior, and name the function that should own the action after the refactor.

## Classification

- `undo-safe`: records undo and updates the edited object correctly.
- `save-immediate`: persists the edited domain immediately.
- `memory-only`: mutates runtime/editor state without saving.
- `compound`: mutates more than one object or system as one user action.
- `legacy-path`: older in-loop HUD/runtime mutation path that bypasses the newer shell-dispatch layer.

## Summary

The current editor has two overlapping mutation layers:

1. `engine/editor_shell_dispatch.inc`
   This is the newer shell/ImGui action layer. It already owns most generic selection actions and inspector callbacks.

2. `engine/editors_runtime.inc` and `engine/editors_runtime_tail.inc`
   These still contain direct mutation logic for the legacy HUD and in-world placement flows. This layer duplicates several actions and is the main source of inconsistent undo/save behavior.

The refactor target should be: one owner function per logical action, with UI/runtime code only dispatching into that owner.

## Canonical Actions By Domain

### Props

| Logical action | Current owner | Key call sites | Classification | Notes |
| --- | --- | --- | --- | --- |
| Select active prop | `editor_shell_selection_set` | `engine/editor_shell_dispatch.inc:729` | memory-only | Good selection sync path. |
| Move selected prop planar | `editor_shell_selection_move_planar` | `engine/editor_shell_dispatch.inc:320` | undo-safe, memory-only | Used by generic selection tools. |
| Nudge/ground selected prop | `editor_shell_selection_nudge_y` | `engine/editor_shell_dispatch.inc:268` | undo-safe, memory-only | Used by generic selection tools. |
| Rotate selected prop | `editor_shell_selection_adjust_yaw` | `engine/editor_shell_dispatch.inc:370` | undo-safe, memory-only | Used by generic selection tools. |
| Scale selected prop | `editor_shell_selection_adjust_scale` | `engine/editor_shell_dispatch.inc:404` | undo-safe, memory-only | Used by generic selection tools. |
| Snap selected prop to grid | `editor_shell_selection_snap_grid` | `engine/editor_shell_dispatch.inc:446` | undo-safe, memory-only | Used by generic selection tools. |
| Face selected prop to camera | `editor_shell_selection_face_camera` | `engine/editor_shell_dispatch.inc:494` | undo-safe, memory-only | Used by generic selection tools. |
| Duplicate selected prop | `editor_shell_selection_duplicate_offset` | `engine/editor_shell_dispatch.inc:527` | undo-safe, memory-only | Duplicates one prop with undo. |
| Delete selected prop | `editor_shell_selection_delete` | `engine/editor_shell_dispatch.inc:769` | undo-safe, memory-only | Shell path only. |
| Set prop yaw from inspector | `editor_shell_prop_set_rotation_y` | `engine/editor_shell_dispatch.inc:852` | memory-only | No undo. |
| Set prop scale from inspector | `editor_shell_prop_set_scale` | `engine/editor_shell_dispatch.inc:862` | memory-only | No undo. |
| Set pickable/pickup data from inspector | `editor_shell_prop_set_pickable`, `editor_shell_prop_set_pickup_item`, `editor_shell_prop_set_pickup_count` | `engine/editor_shell_dispatch.inc:873`, `:888`, `:898` | memory-only | No undo. |
| Prop edit Y-/Y+/Ground toolbar actions | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:2050` | undo-safe, memory-only | Action buttons do record undo. |
| Prop edit delete toolbar action | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:2059` | undo-safe, memory-only | Action buttons do record undo. |
| Prop place: place model | legacy placer loop | `engine/editors_runtime.inc:1454` | undo-safe, memory-only, legacy-path | Direct runtime placement path. |
| Prop place: place prefab | legacy placer loop | `engine/editors_runtime.inc:1440` | compound, legacy-path | No visible undo entry here for prefab placement result. |
| Prop place: delete hovered prop | legacy placer loop | `engine/editors_runtime.inc:1473` | undo-safe, memory-only, legacy-path | Duplicates shell delete behavior. |
| Prop edit HUD Y-/Y+/Ground | legacy prop edit HUD | `engine/editors_runtime.inc:1577` | memory-only, legacy-path | No undo here, unlike shell action path. |
| Prop edit HUD delete | legacy prop edit HUD | `engine/editors_runtime.inc:1600` | undo-safe, memory-only, legacy-path | Duplicates shell delete behavior. |
| Bulk align/distribute | `editor_bulk_align_axis`, `editor_bulk_distribute_axis` | `engine/editor_shell_dispatch.inc:35`, `:60` | undo-safe, compound | One undo entry per prop, not grouped. |
| Bulk replace selection with current model | `editor_bulk_replace_selection_with_current` | `engine/editor_shell_dispatch.inc:113` | mixed, compound | Model replace uses transform undo; prefab replace is not fully reversible. |
| Bulk scatter current model/prefab | `editor_bulk_scatter_current` | `engine/editor_shell_dispatch.inc:145` | compound | No undo entries recorded. |
| Bulk stamp current model/prefab on path | `editor_bulk_stamp_path_current` | `engine/editor_shell_dispatch.inc:171` | compound | No undo entries recorded. |
| Persist props | `placer_save`, world save | `engine/editor_shell_actions.inc:263` | save-immediate only on explicit save | Most prop mutations are unsaved until save. |

Recommended owner after refactor:
- `editor_command_prop_transform`
- `editor_command_prop_property`
- `editor_command_prop_create`
- `editor_command_prop_delete`
- `editor_command_prop_duplicate`
- `editor_command_prop_bulk_transaction`

### Markers

| Logical action | Current owner | Key call sites | Classification | Notes |
| --- | --- | --- | --- | --- |
| Select marker | `editor_shell_selection_set` | `engine/editor_shell_dispatch.inc:729` | memory-only | Good selection sync path. |
| Move/height/snap selected marker | generic selection actions | `engine/editor_shell_dispatch.inc:268`, `:446` | memory-only | No undo. |
| Scale selected marker radius | `editor_shell_selection_adjust_scale` | `engine/editor_shell_dispatch.inc:404` | memory-only | No undo. |
| Duplicate selected marker | `editor_shell_selection_duplicate_offset` | `engine/editor_shell_dispatch.inc:566` | undo-safe, save-immediate | Saves immediately. |
| Delete selected marker | `editor_shell_selection_delete` | `engine/editor_shell_dispatch.inc:792` | undo-safe, save-immediate | Saves immediately. |
| Set marker kind/radius/y from inspector | `editor_shell_marker_set_kind`, `editor_shell_marker_set_radius`, `editor_shell_marker_set_y` | `engine/editor_shell_dispatch.inc:907`, `:919`, `:928` | memory-only | No undo. |
| Marker action toolbar save | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:1197` | save-immediate | Explicit save path. |
| Marker action toolbar radius/Y/delete | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:1200` | memory-only | No undo; delete here also skips undo. |
| Marker place/select/delete in world | legacy marker loop | `engine/editors_runtime.inc:1082` | mixed, legacy-path | Place/delete record undo but do not immediately save. |
| Marker HUD save/radius/delete/Y | legacy HUD buttons | `engine/editors_runtime.inc:569` | mixed, legacy-path | Direct mutations, mostly no undo. |

Recommended owner after refactor:
- `editor_command_marker_update`
- `editor_command_marker_create`
- `editor_command_marker_delete`

### Enemy Spawns

| Logical action | Current owner | Key call sites | Classification | Notes |
| --- | --- | --- | --- | --- |
| Select spawn | `editor_shell_selection_set` | `engine/editor_shell_dispatch.inc:729` | memory-only | Good selection sync path. |
| Move/height/snap selected spawn | generic selection actions | `engine/editor_shell_dispatch.inc:268`, `:446` | memory-only | No undo. |
| Scale selected spawn | `editor_shell_selection_adjust_scale` | `engine/editor_shell_dispatch.inc:404` | memory-only | No undo. |
| Duplicate selected spawn | `editor_shell_selection_duplicate_offset` | `engine/editor_shell_dispatch.inc:589` | undo-safe, save-immediate | Saves immediately. |
| Delete selected spawn | `editor_shell_selection_delete` | `engine/editor_shell_dispatch.inc:807` | undo-safe, save-immediate | Delete compacts array. |
| Save selected spawn data | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:1287` | save-immediate | Used by ImGui inspector save operations. |
| Place/select/delete spawn in world | legacy spawn loop | `engine/editors_runtime.inc:2169` | undo-safe, save-immediate, legacy-path | Also mutates runtime enemy array on create. |
| Spawn inspector field edits in ImGui | direct bound pointers + save action | `engine/editor_ui_imgui.cpp:718`, `:742`, `:746` | save-immediate through explicit action dispatch | UI edits mutate bound values before save. |

Recommended owner after refactor:
- `editor_command_spawn_update`
- `editor_command_spawn_create`
- `editor_command_spawn_delete`

### NPCs

| Logical action | Current owner | Key call sites | Classification | Notes |
| --- | --- | --- | --- | --- |
| Select NPC | `editor_shell_selection_set` | `engine/editor_shell_dispatch.inc:729` | memory-only | Good selection sync path. |
| Move/height/snap selected NPC | generic selection actions | `engine/editor_shell_dispatch.inc:268`, `:446` | memory-only | Updates runtime NPC position, but no undo. |
| Rotate selected NPC | generic selection actions | `engine/editor_shell_dispatch.inc:370`, `:494` | memory-only | Updates runtime NPC yaw, but no undo. |
| Duplicate selected NPC | `editor_shell_selection_duplicate_offset` | `engine/editor_shell_dispatch.inc:611` | undo-safe, save-immediate, compound | Also creates runtime NPC via `npc_add`. |
| Delete selected NPC | `editor_shell_selection_delete` | `engine/editor_shell_dispatch.inc:827` | undo-safe | Uses `editor_npc_delete_spawn_at`; array compaction issues remain in undo layer. |
| Change NPC model from inspector | `editor_shell_npc_set_focus_model_index` | `engine/editor_shell_dispatch.inc:937` | save-immediate | Mutates spawn and runtime NPC, no undo. |
| Commit NPC name/backstory | `editor_shell_npc_commit_focus_name`, `editor_shell_npc_commit_focus_backstory` | `engine/editor_shell_dispatch.inc:953`, `:971` | save-immediate | No undo. |
| NPC action toolbar edits | `editor_shell_tool_action` | `engine/editor_shell_dispatch.inc:1493` and `:1536`-`:1784` | mostly save-immediate | Many direct per-field mutations plus immediate save; no uniform undo. |
| NPC place/select/delete in world | legacy NPC loop | `engine/editors_runtime_tail.inc:273`, `:278`, `:300` | mixed, legacy-path | Place/delete record undo; many other edits save immediately without undo. |
| NPC waypoint, merchant, shop, dialogue tree edits | legacy NPC loop and shell tool actions | `engine/editors_runtime_tail.inc:178`-`:253`, `engine/editor_shell_dispatch.inc:1536`-`:1784` | save-immediate, compound | Important mutation surface for later passes; not in first-pass scope unless touched by shared command layer. |

Recommended owner after refactor:
- `editor_command_npc_update`
- `editor_command_npc_create`
- `editor_command_npc_delete`
- later: `editor_command_npc_dialogue`, `editor_command_npc_waypoints`

## Duplicate / Conflicting Implementations

These actions exist in more than one mutation path today:

- Prop delete
  - `editor_shell_selection_delete`
  - `editor_shell_tool_action` prop delete
  - legacy prop placer delete in `editors_runtime.inc`
  - legacy prop HUD delete in `editors_runtime.inc`

- Prop transform
  - generic selection actions in `editor_shell_dispatch.inc`
  - inspector callbacks in `editor_shell_dispatch.inc`
  - legacy HUD prop edit controls in `editors_runtime.inc`
  - legacy drag gizmo flows in `editors_runtime.inc`

- Marker delete and marker radius/Y edits
  - generic selection + inspector/action callbacks in `editor_shell_dispatch.inc`
  - legacy marker in-world loop in `editors_runtime.inc`
  - legacy HUD marker controls in `editors_runtime.inc`

- Spawn create/delete
  - generic selection duplicate/delete in `editor_shell_dispatch.inc`
  - legacy in-world create/delete loop in `editors_runtime.inc`

- NPC create/delete and property edits
  - generic selection duplicate/delete in `editor_shell_dispatch.inc`
  - shell action NPC editor in `editor_shell_dispatch.inc`
  - legacy in-world NPC edit loop in `editors_runtime_tail.inc`

## Highest-Risk Gaps Found During Inventory

These are the mutations most likely to block the first pass if left as-is:

1. Inspector prop edits bypass undo.
   - `engine/editor_shell_dispatch.inc:852`
   - `engine/editor_shell_dispatch.inc:862`
   - `engine/editor_shell_dispatch.inc:873`

2. Generic selection edits for markers, spawns, and NPCs bypass undo.
   - `engine/editor_shell_dispatch.inc:294`
   - `engine/editor_shell_dispatch.inc:300`
   - `engine/editor_shell_dispatch.inc:306`
   - `engine/editor_shell_dispatch.inc:428`
   - `engine/editor_shell_dispatch.inc:467`
   - `engine/editor_shell_dispatch.inc:474`
   - `engine/editor_shell_dispatch.inc:480`
   - `engine/editor_shell_dispatch.inc:514`

3. Legacy HUD/runtime paths still mutate objects directly, including actions that the shell path already covers.
   - marker HUD in `engine/editors_runtime.inc:569`
   - prop HUD in `engine/editors_runtime.inc:1577`
   - in-world prop place/delete in `engine/editors_runtime.inc:1440`
   - in-world spawn place/delete in `engine/editors_runtime.inc:2169`
   - in-world NPC place/delete and mode edits in `engine/editors_runtime_tail.inc:178`

4. Bulk prop operations are not routed through transactions.
   - `engine/editor_shell_dispatch.inc:113`
   - `engine/editor_shell_dispatch.inc:145`
   - `engine/editor_shell_dispatch.inc:171`

## Refactor Ownership Target

For the first pass, all UI and runtime entry points should be reduced to dispatch-only code. Mutation ownership should move into a dedicated command layer:

- props:
  - create
  - delete
  - transform
  - property update
  - duplicate
  - bulk transaction

- markers:
  - create
  - delete
  - update

- spawns:
  - create
  - delete
  - update

- NPCs:
  - create
  - delete
  - update

Anything outside that layer should only:

- gather input
- choose the target object
- call one command
- refresh selection/UI from the result

## Audit Sources

- `engine/editor_shell_dispatch.inc`
- `engine/editor_shell_actions.inc`
- `engine/editors_runtime.inc`
- `engine/editors_runtime_tail.inc`
- `engine/editor_ui_imgui.cpp`
- `engine/undo.inc`
