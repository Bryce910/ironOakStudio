#include "engine/world_rooted_grass.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <limits>

int main() {
    using world_rooted_grass::capability;
    const float minimum[]={-1.58f,0.f,-1.44f},maximum[]={1.63f,.71f,1.28f};
    unsigned accepted=0,rejected=0;
    for(const char *stem:{"bep_nature_grass_long","bep_nature_grass_broad",
        "imports/bep_nature_grass_long/1d0b8738ded932866f3b3a83/asset",
        "imports/bep_nature_grass_broad/c7a6b2b766e39b3f882e061f/asset",
        "visual_upgrade/grass_patch","cinematic_grass_0","cinematic_grass_1","cinematic_grass_2"}) {
        assert(capability(stem,3,true,true,minimum,maximum));accepted++;
    }
    for(const char *stem:{"bep_nature_bush_a","bep_nature_reed","bep_nature_fern",
        "bep_nature_clover","village_crop_wheat","shire/grass/grass_long_patch",
        "my_grass_replacement","grass","cinematic_grass_3",
        "imports/bep_nature_grass_long/not-a-version/asset",
        "imports/bep_nature_grass_long/1d0b8738ded932866f3b3a83/asset_extra",
        "imports/bep_nature_grass_long/1d0b8738ded932866f3b3a83/other"}) {
        assert(!capability(stem,3,true,true,minimum,maximum));rejected++;
    }
    for(int vegetation:{0,1,2,4,6}) {
        assert(!capability("bep_nature_grass_long",vegetation,true,true,minimum,maximum));rejected++;
    }
    assert(!capability("bep_nature_grass_long",3,false,true,minimum,maximum));
    assert(!capability("bep_nature_grass_long",3,true,false,minimum,maximum));
    // Source replacements must retain validated geometry and root datum. The
    // alias, renderer availability and a previous successful family never waive it.
    for(float bad:{-.2f,.2f,std::numeric_limits<float>::quiet_NaN()}) {
        float changed[]={minimum[0],bad,minimum[2]};
        assert(!capability("bep_nature_grass_long",3,true,true,changed,maximum));rejected++;
    }
    float huge[]={1.63f,12.f,1.28f};
    assert(!capability("bep_nature_grass_long",3,true,true,minimum,huge));
    assert(!capability(nullptr,3,true,true,minimum,maximum));
    assert(!capability("bep_nature_grass_long",3,true,true,nullptr,maximum));
    std::printf("PASS rooted grass family capability: %u source families/import versions, %u rejected unrelated/category/datum cases; root/collision/finite-bounds guards\n",accepted,rejected);
}
