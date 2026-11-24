#ifndef VALWOOD_WORLD_CASTLE_LIGHTS_H
#define VALWOOD_WORLD_CASTLE_LIGHTS_H

#include <array>
#include <cmath>
#include <cstddef>

// Startup-only configuration for the four actual lamps in the keep's ground
// hall. Positions use absolute atlas XZ and world Y, matching the foliage
// shader after its camera-origin reconstruction. No runtime allocations.
struct WorldCastleLights {
    static constexpr std::size_t count = 4;
    std::array<std::array<float,4>,count> position_radius = {};
    std::array<float,3> color = {{24.f,14.88f,6.72f}}; // linear warm source intensity
    std::array<float,4> bounds = {}; // outer shell xmin,zmin,xmax,zmax
    std::array<float,2> heights = {{23.8f,31.2f}};
};

inline bool world_castle_lights_build(double origin_x,double origin_z,WorldCastleLights &out) {
    // Atlas extents are only 100.8km. Reject invalid origins before mutation;
    // this also keeps conversion to shader float coordinates well conditioned.
    if(!std::isfinite(origin_x)||!std::isfinite(origin_z)||
       std::abs(origin_x)>100800.||std::abs(origin_z)>100800.)return false;
    WorldCastleLights result;
    constexpr float local[4][2]={{-9.8f,-138.5f},{9.8f,-138.5f},
                                  {-9.8f,-151.5f},{9.8f,-151.5f}};
    for(std::size_t i=0;i<result.count;++i)
        result.position_radius[i]={{float(origin_x+local[i][0]),26.895f,float(origin_z+local[i][1]),20.f}};
    // The inward 25cm/15cm shader feather is inside the solid shell. Interior
    // wall faces at X±11.6, Z[-153.6,-136], floor24.015 and ceiling31 retain
    // full response; positions on/outside the outer shell receive zero.
    result.bounds={{float(origin_x-12.),float(origin_z-154.),float(origin_x+12.),float(origin_z-135.6)}};
    out=result;return true;
}

#endif
