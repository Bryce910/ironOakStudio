#ifndef VALWOOD_WORLD_ROOTED_GRASS_H
#define VALWOOD_WORLD_ROOTED_GRASS_H
#include <cmath>
#include <cstring>

namespace world_rooted_grass {
// A source-family capability, not a substring guess on a user-facing alias.
// These families were inspected as soil-rooted masked grass. Shrubs, ferns,
// clover, reeds, crops and arbitrary newly imported plants keep their response.
inline bool family(const char *stem) {
    if(!stem)return false;
    if(std::strcmp(stem,"bep_nature_grass_long")==0||
       std::strcmp(stem,"bep_nature_grass_broad")==0||
       std::strcmp(stem,"visual_upgrade/grass_patch")==0||
       std::strcmp(stem,"cinematic_grass_0")==0||
       std::strcmp(stem,"cinematic_grass_1")==0||
       std::strcmp(stem,"cinematic_grass_2")==0)return true;
    constexpr const char *prefixes[]={"imports/bep_nature_grass_long/",
                                      "imports/bep_nature_grass_broad/"};
    for(const char *prefix:prefixes) {
        const auto length=std::strlen(prefix);
        if(std::strncmp(stem,prefix,length)!=0)continue;
        const char *version=stem+length;
        // Imported v1 content-addressed source directories have 24 hex digits.
        if(std::strlen(version)!=30||std::strcmp(version+24,"/asset")!=0)return false;
        for(unsigned i=0;i<24;i++)
            if(!((version[i]>='0'&&version[i]<='9')||(version[i]>='a'&&version[i]<='f')))return false;
        return true;
    }
    return false;
}

inline bool capability(const char *stem,int vegetation,bool root_grounded,
                       bool collision_free,const float minimum[3],const float maximum[3]) {
    if((vegetation!=3&&vegetation!=5)||!root_grounded||!collision_free||
       !minimum||!maximum||!family(stem))return false;
    for(unsigned i=0;i<3;i++)
        if(!std::isfinite(minimum[i])||!std::isfinite(maximum[i])||minimum[i]>maximum[i])return false;
    // Root-local Y is the wind weight. Reject floating, centre-pivot or enlarged
    // replacement geometry rather than bending it using an unvalidated datum.
    return minimum[1]>=-.02f&&minimum[1]<=.08f&&maximum[1]>.05f&&maximum[1]<=1.2f&&
           minimum[0]>=-3.f&&maximum[0]<=3.f&&minimum[2]>=-3.f&&maximum[2]<=3.f;
}
}
#endif
