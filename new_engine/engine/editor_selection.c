#include "engine/editor_selection.h"

EditorSelectionState g_editor_selection={EDITOR_SELECTION_NONE,-1,0};
bool g_editor_ui_request_sculpt_undo=false;
bool g_editor_ui_request_forest_undo=false;

void editor_selection_clear_for_scene(int scene){
    g_editor_selection.kind=EDITOR_SELECTION_NONE;
    g_editor_selection.index=-1;
    g_editor_selection.scene=scene;
}

void editor_selection_set_for_scene(EditorSelectionKind kind,int index,int scene){
    g_editor_selection.kind=kind;
    g_editor_selection.index=index;
    g_editor_selection.scene=scene;
}

bool editor_selection_is_active(const EditorSelectionState *selection,
                                EditorSelectionKind kind,int index){
    return selection&&selection->kind==kind&&selection->index==index;
}

void editor_selection_after_compact_delete_in_state(EditorSelectionState *selection,
                                                    EditorSelectionKind kind,
                                                    int deleted_idx,
                                                    int new_count){
    if(!selection||selection->kind!=kind) return;
    if(selection->index==deleted_idx){
        if(new_count<=0){
            selection->kind=EDITOR_SELECTION_NONE;
            selection->index=-1;
        }else if(deleted_idx>=new_count){
            selection->index=new_count-1;
        }
    }else if(selection->index>deleted_idx){
        selection->index--;
    }
}
