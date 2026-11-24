#ifndef VALWOOD_WORLD_STUDIO_HISTORY_REBUILD_H
#define VALWOOD_WORLD_STUDIO_HISTORY_REBUILD_H
#include "engine/world_studio.h"
#include <algorithm>

// Pure classification for an explicit Undo/Redo command, never passive UI
// frames. Persistence compares every authored field, including new serialized
// fields, instead of maintaining a whitelist of geometry records. Unknown or
// invalid differences keep the existing conservative scenery rebuild path.
namespace world_studio_history {
inline bool requires_scenery_rebuild(const ::world_studio::Document &before,
                                     const ::world_studio::Document &restored,
                                     bool pending_rebuild=false){
    if(pending_rebuild)return true;
    if(::world_studio::environment_only_change(before,restored))return false;
    // Derived terrain is intentionally absent from persistence. Different
    // immutable owners are conservative even if their visible result matches.
    if(before.surface_field!=restored.surface_field)return true;
    // Persistence omits these dormant values. Preserve their differences too,
    // so the fast path remains limited to ecology and the issued-ID counter.
    const auto &a=before.environment,&b=restored.environment;
    if(a.path!=b.path||a.enabled!=b.enabled||a.rotation!=b.rotation||
       a.intensity!=b.intensity||a.strength!=b.strength)return true;
    if(before.navigation.size()!=restored.navigation.size())return true;
    for(size_t i=0;i<before.navigation.size();++i)
        if(before.navigation[i].floor_y!=restored.navigation[i].floor_y)return true;
    // Comparing the large numeric terrain payload is much cheaper than
    // formatting it twice. Structured bindings deliberately require a review
    // if TerrainTile gains a field; an added field cannot silently be omitted.
    if(before.terrain.size()!=restored.terrain.size())return true;
    for(size_t i=0;i<before.terrain.size();++i){
        const auto &[ax,az,ad]=before.terrain[i];
        const auto &[bx,bz,bd]=restored.terrain[i];
        if(ax!=bx||az!=bz||ad!=bd)return true;
    }

    std::string error,left,right;
    if(!::world_studio::valid(before,error)||!::world_studio::valid(restored,error))return true;
    auto comparison=before;
    comparison.ecology.clear();comparison.terrain.clear();
    // History::undo/redo preserves every ID already issued by the current
    // document. Compare the snapshot as it will actually be restored.
    const auto highwater=std::max(before.entities.next_id,restored.entities.next_id);
    comparison.entities.next_id=highwater;comparison.next_structure_id=std::max(before.next_structure_id,restored.next_structure_id);
    if(!::world_studio::encode(comparison,left,error))return true;
    comparison=restored;comparison.ecology.clear();comparison.terrain.clear();comparison.entities.next_id=highwater;comparison.next_structure_id=std::max(before.next_structure_id,restored.next_structure_id);
    return !::world_studio::encode(comparison,right,error)||left!=right;
}
}
#endif
