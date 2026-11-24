#ifndef ENGINE_EDITOR_SELECTION_H
#define ENGINE_EDITOR_SELECTION_H

#include <stdbool.h>

#include "engine/editor_ui.h"

extern EditorSelectionState g_editor_selection;
extern bool g_editor_ui_request_sculpt_undo;
extern bool g_editor_ui_request_forest_undo;

void editor_selection_clear_for_scene(int scene);
void editor_selection_set_for_scene(EditorSelectionKind kind,int index,int scene);
bool editor_selection_is_active(const EditorSelectionState *selection,
                                EditorSelectionKind kind,int index);
void editor_selection_after_compact_delete_in_state(EditorSelectionState *selection,
                                                    EditorSelectionKind kind,
                                                    int deleted_idx,
                                                    int new_count);

#endif
