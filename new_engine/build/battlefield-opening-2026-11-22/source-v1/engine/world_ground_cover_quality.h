#ifndef VALWOOD_WORLD_GROUND_COVER_QUALITY_H
#define VALWOOD_WORLD_GROUND_COVER_QUALITY_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "engine/world_graphics_settings.h"

// Shared CPU generation and render policy. Solid trees and rocks retain the
// same roots at every tier; only noncolliding grass and fern budgets vary.
namespace world_ground_cover_quality {
inline constexpr float grass_lod_begin=14.f;
inline constexpr float grass_lod_end=22.f;
struct Profile {
    std::size_t grass,trees,rocks,ferns;
    float grass_blade_keep;
};
inline constexpr Profile profile(int tier) noexcept {
    switch(tier){
        case 0:return {23000,7000,180,360,.35f};
        case 1:return {46000,7000,180,900,.50f};
        case 2:return {69000,7000,180,1800,.75f};
        case 3:return {92000,7000,180,3600,.95f};
    }
    return {46000,7000,180,900,.50f};
}
inline constexpr Profile profile(WorldFoliageDensity tier) noexcept {
    return profile(static_cast<int>(tier));
}

// Preserve Dressing's established species/tuft classification bit for bit.
inline std::uint32_t generated_seed(float yaw,float scale) noexcept {
    std::uint32_t yaw_bits,scale_bits;
    std::memcpy(&yaw_bits,&yaw,sizeof yaw_bits);
    std::memcpy(&scale_bits,&scale,sizeof scale_bits);
    std::uint64_t h=std::uint64_t(yaw_bits)*0x9e3779b97f4a7c15ull^
                    std::uint64_t(scale_bits)*0xbf58476d1ce4e5b9ull;
    h^=h>>30;h*=0xbf58476d1ce4e5b9ull;h^=h>>27;
    return std::uint32_t(h^(h>>32));
}

// One stable world-independent rank makes every lower density a subset of
// every higher density. Endpoints bypass hashing; invalid input fails open.
inline bool keep(std::uint64_t seed,float amount) noexcept {
    if(!std::isfinite(amount)||amount>=1.f)return true;
    if(amount<=0.f)return false;
    seed+=0x9e3779b97f4a7c15ull;
    seed=(seed^(seed>>30))*0xbf58476d1ce4e5b9ull;
    seed=(seed^(seed>>27))*0x94d049bb133111ebull;
    seed^=seed>>31;
    const double rank=double(seed>>11)*(1.0/9007199254740992.0);
    return rank<double(amount);
}
}
#endif
