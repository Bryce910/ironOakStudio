# Windows v6 Add NPC: read-only diagnosis

No runtime was launched or manipulated for this diagnosis. No frozen source or executable was changed. The root agent owns the focused ordinary-input retry.

## What the existing evidence establishes

The original and pointer-probe Windows runs fail the assertion `Add NPC button arms placement` in QA phase 2. Their profile reports phase 3 because the frozen phase-2 case unconditionally calls `point_click(0)` and assigns phase 3 after the failed assertion. It is not evidence that an NPC was placed. Both runs retain zero authored actors, zero scene Applies and the initial built-in demo status message, with an empty NPC notice.

The `runtime_panel` Add button has no NPC-specific disabled guard. The enclosing Details pane disables its contents only for `structure_cooking`, a valid project save job, or `project_restart_required`. The QA `ready` gate excludes those three and also excludes `production_freeze`, Apply and loading before it advances.

If `npc_arm(Add)` executes, every failure path calls `npc_fail`: freeze/playing/Apply/cook, missing population library, or the 32-edit limit all change the status message and NPC notice. A successful call changes mode and sets a nonempty placement notice. Later cancellation preserves that normal placement notice. The unchanged initial message and empty notice therefore point to the ImGui Add button not returning true. They do not fit a source-floor raycast, navigation or library rejection after clicking the button.

The pointer-probe early screenshot shows Add in its hovered color and enabled text, while the manual `add-armed.png` screenshot shows the normal button color and no placement notice. The early screenshot proves a correct pointer position at that one instant; it does not establish down/up input delivery, focus or coordinates at the later activation frames. The manual XTest wrapper checks ownership and translates client coordinates, but its recorded events do not include an independently verified active/focused window state.

The NPC production and QA includes are unchanged between the passed native-v5 run and Windows v6. Parent-v5 native success is useful regression evidence, but does not make the failed Windows check pass.

## Candidate input mechanism, not yet proven

`editor_ui_imgui_begin_frame()` calls the SDL backend and then queues an unconditional mouse-position event from `SDL_GetMouseState` before `ImGui::NewFrame`. This can supersede a queued event position. A focused `x11`-driver correction uses global mouse state to recover a stale local snapshot after relative capture. Wine's SDL Windows driver does not enter that branch. The actual retained native and Windows UI objects both import the local/global state APIs and contain the x11 branch; this is not merely an observation about unused source.

The NPC QA hover queues a tagged motion event and a direct ImGui position event, then waits several frames before down/up. A stale local snapshot, a warp/focus transition or a queued real mouse motion can change the position used at activation. The original Windows structure/water button checks passed, so a general claim that Windows UI clicks are broken would be too broad. Focus and per-frame button coordinates must be observed before choosing a production fix.

## Smallest next observation

First finish the root-owned ordinary session with verified activation of its exact window. Keyboard navigation and Enter on Add, if practical, can distinguish pointer routing from the NPC arm guards without changing any source.

If that does not resolve the issue, use a new explicitly versioned diagnostic build; v6 remains immutable. Limit a trace to the Add button and its down/up frames:

- Before UI NewFrame: SDL window flags, keyboard-focus and mouse-focus IDs, relative mode, local and global mouse x/y/buttons, and raw tagged/ordinary event positions/types.
- After NewFrame: ImGui `AppFocusLost`, `MousePos`, left `MouseDown`, `MouseClicked`, `MouseReleased`, and active/hovered window IDs.
- Immediately after the actual Add `ImGui::Button`: return value, item ID/rectangle, hovered/active/disabled state; `structure_cooking`, save-job validity, restart-required, Apply, production freeze/playing, population availability, actor count, NPC mode, and notice.

Log state only on transitions or during the bounded QA click window. Do not arm the NPC directly, relax an assertion, clear locks, change scene state, replace the button with a permissive raw hit box, or claim acceptance from diagnostic output. If an outer guard is real, the trace will identify it; if the button never sees a complete click, it will show the input boundary where it was lost.
