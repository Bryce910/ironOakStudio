#ifndef VALWOOD_EDITOR_UI_IMGUI_SUBMISSION_H
#define VALWOOD_EDITOR_UI_IMGUI_SUBMISSION_H

#include "third_party/imgui/imgui.h"

// The renderer services texture requests even when there are no draw lists.
// Treat every non-OK status (and malformed null entry) as required backend work.
inline unsigned world_imgui_pending_texture_work(const ImDrawData *draw){
    unsigned pending=0;
    if(draw&&draw->Textures)for(const auto *texture:*draw->Textures)
        if(!texture||texture->Status!=ImTextureStatus_OK)pending++;
    return pending;
}

// Callback-only lists can have zero vertices/indices. Require both the public
// list count and the actual list array to be empty, and preserve the complete
// backend for visible UI, pending textures, and invalid/inconsistent draw data.
inline bool world_imgui_can_skip_empty_backend(bool enabled,bool passive,const ImDrawData *draw){
    return enabled&&passive&&draw&&draw->Valid&&draw->CmdListsCount==0&&draw->CmdLists.empty()&&
        draw->TotalVtxCount==0&&draw->TotalIdxCount==0&&world_imgui_pending_texture_work(draw)==0;
}

#endif
