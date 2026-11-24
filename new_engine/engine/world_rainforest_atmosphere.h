#ifndef VALWOOD_WORLD_RAINFOREST_ATMOSPHERE_H
#define VALWOOD_WORLD_RAINFOREST_ATMOSPHERE_H

#include <algorithm>
#include <cmath>

// Regional weather is separate from the player's global lighting/quality
// controls. Pure evaluation lets walking and teleports use the same look in
// geometry, sky, water, particles and loading-only local radiance captures.
struct WorldRainforestAtmosphere {
    float blend=0,cloud=.28f,sun_multiplier=1,sky_multiplier=1;
    float haze_add=0,mist_weight=0;
};

inline WorldRainforestAtmosphere world_rainforest_atmosphere(float cloud,float weight,bool open_canopy=false) {
    const float c=std::isfinite(cloud)?std::clamp(cloud,0.f,1.f):.28f;
    const float w=std::isfinite(weight)?std::clamp(weight,0.f,1.f):0.f;
    const float blend=w*w*(3.f-2.f*w);
    WorldRainforestAtmosphere result;
    result.blend=blend;
    result.cloud=c+(std::max(c,open_canopy?.68f:.96f)-c)*blend;
    result.sun_multiplier=1.f-(open_canopy?.25f:.64f)*blend;
    // Brighter openings balance the separate source-crown shelter field.
    result.sky_multiplier=1.f-(open_canopy?.06f:.26f)*blend;
    result.haze_add=(open_canopy?.00038f:.00080f)*blend;
    // Default mist .62 becomes1.24 at the heart; the renderer retains the
    // existing1.5 bound and respects a zero mist slider/disabled volume setting.
    result.mist_weight=blend*(1.f+(open_canopy?.3f:1.f)*blend);
    return result;
}
#endif
