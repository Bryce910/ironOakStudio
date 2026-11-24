#include "engine/world_rainforest_atmosphere.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    // Zero biome influence preserves actual user cloud values and all gains.
    for(float cloud:{0.f,.28f,.7f,1.f}) {
        const auto p=world_rainforest_atmosphere(cloud,0);
        assert(p.cloud==cloud&&p.blend==0&&p.sun_multiplier==1&&p.sky_multiplier==1);
        assert(p.haze_add==0&&p.mist_weight==0);
        assert(world_rainforest_atmosphere(cloud,1).cloud>=cloud);
    }
    const auto full=world_rainforest_atmosphere(.28f,1);
    const auto opening=world_rainforest_atmosphere(.28f,1,true);
    assert(std::fabs(opening.cloud-.68f)<1e-6f&&std::fabs(opening.sun_multiplier-.75f)<1e-6f);
    assert(std::fabs(opening.sky_multiplier-.94f)<1e-6f&&opening.haze_add<full.haze_add&&opening.mist_weight<full.mist_weight);
    assert(world_rainforest_atmosphere(.28f,0,true).sun_multiplier==1);
    assert(std::fabs(full.cloud-.96f)<1e-6f&&std::fabs(full.sun_multiplier-.36f)<1e-6f);
    assert(std::fabs(full.sky_multiplier-.74f)<1e-6f&&std::fabs(full.haze_add-.0008f)<1e-8f);
    assert(full.blend==1&&full.mist_weight==2&&std::fabs(.62f*full.mist_weight-1.24f)<1e-6f);
    // Traversal in either direction must follow the same bounded smooth curve,
    // without state, allocations, overshoot or a weather threshold at entry.
    auto before=world_rainforest_atmosphere(.28f,0);
    for(int i=1;i<=10000;i++) {
        float w=float(i)/10000;const auto p=world_rainforest_atmosphere(.28f,w);
        assert(p.blend>=before.blend&&p.cloud>=before.cloud&&p.mist_weight>=before.mist_weight);
        assert(p.sun_multiplier<=before.sun_multiplier&&p.sky_multiplier<=before.sky_multiplier);
        assert(p.cloud-before.cloud<.0002f&&p.mist_weight-before.mist_weight<.0005f);
        assert(p.cloud<=.960001f&&p.sun_multiplier>=.359999f&&p.sky_multiplier>=.739999f);
        assert(p.haze_add>=0&&p.haze_add<=.00080001f&&p.mist_weight<=2);
        before=p;
    }
    const auto edge=world_rainforest_atmosphere(.28f,.0001f);
    const auto near_full=world_rainforest_atmosphere(.28f,.9999f);
    assert(edge.blend<4e-8f&&1-near_full.blend<1e-6f);
    assert(world_rainforest_atmosphere(.28f,-1).blend==0&&world_rainforest_atmosphere(.28f,2).blend==1);
    assert(world_rainforest_atmosphere(.28f,std::numeric_limits<float>::quiet_NaN()).blend==0);
    std::puts("PASS rainforest atmosphere: exact exterior controls, full overcast/dim canopy/mist targets, 10000 monotone bounded traversal samples, smooth endpoints and input bounds");
}
