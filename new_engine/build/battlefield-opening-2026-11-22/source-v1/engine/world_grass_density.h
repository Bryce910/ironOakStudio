#ifndef VALWOOD_WORLD_GRASS_DENSITY_H
#define VALWOOD_WORLD_GRASS_DENSITY_H
#include "engine/world_ground_cover_quality.h"
// Only noncolliding visual grass. World-generated yaw/scale remain identical
// across cache anchors; camera, source index, species and fade never rank roots.
namespace world_grass_density {
struct Selection {bool density,blades;};
inline Selection classify(float yaw,float scale,float amount,int tier) noexcept {
    const auto seed=world_ground_cover_quality::generated_seed(yaw,scale);
    return {world_ground_cover_quality::keep(seed^0x85ebca6bu,amount),
            world_ground_cover_quality::keep(seed^0x4c957f2du,world_ground_cover_quality::profile(tier).grass_blade_keep)};
}
}
#endif
