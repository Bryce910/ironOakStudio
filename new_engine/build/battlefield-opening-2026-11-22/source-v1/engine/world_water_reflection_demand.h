#ifndef VALWOOD_WORLD_WATER_REFLECTION_DEMAND_H
#define VALWOOD_WORLD_WATER_REFLECTION_DEMAND_H
#include "engine/world_studio.h"

// Use the same first override and inherited visibility for drawing and mirror
// demand. A missing override keeps the legacy global ocean enabled.
inline const world_studio::Water *world_water_source_style(
        const world_studio::Document &document,unsigned source,
        const world_studio::Water *custom=nullptr) {
    if(custom)return custom;
    for(const auto &water:document.waters)if(water.source==source)return &water;
    return nullptr;
}
inline bool world_water_style_enabled(const world_studio::Document &document,
        const world_studio::Water *style) {
    return !style||(style->enabled&&world_entity::visible(document.entities,
        {world_entity::Kind::Water,unsigned(style-document.waters.data())}));
}

struct WorldWaterReflectionDemand {
    bool render=false,skipped_unused_ocean=false;
};
// River surfaces explicitly do not sample the planar mirror. Local pool,
// creek and authored-pond consumers must keep it even with an inland override.
// The bounded QA variant restores only the old, otherwise unused demand; it
// does not re-enable an ocean surface or change any water shader/material.
inline WorldWaterReflectionDemand world_water_reflection_demand(bool graphics,
        bool local_consumer,bool legacy_ocean_candidate,bool ocean_enabled,
        bool force_legacy=false) {
    const bool legacy=graphics&&(local_consumer||legacy_ocean_candidate);
    const bool render=graphics&&(local_consumer||
        (legacy_ocean_candidate&&(ocean_enabled||force_legacy)));
    return {render,legacy&&!render};
}
#endif
