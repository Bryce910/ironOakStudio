#ifndef VALWOOD_WORLD_CITY_SCENE_H
#define VALWOOD_WORLD_CITY_SCENE_H
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

// Optional, project-owned source look. Values are linear RGB irradiance/radiance;
// the existing editable sun/sky strengths still scale them. Parsing commits only
// a complete bounded document, so a bad authored look cannot partly replace it.
namespace world_city_scene {
struct Look {
    std::array<float,3> sun={1,1,1},sky={.19f,.245f,.325f},ground={.066f,.070f,.047f};
    std::array<float,3> fog={.363f,.430f,.505f},zenith={.04f,.116f,.245f};
    float haze=.000025f;
};
inline bool load(const char *path,Look &out,std::string &error) {
    std::ifstream file(path);if(!file){error="Cannot open city.scene";return false;}
    std::string line;if(!std::getline(file,line)||line!="VALWOOD_CITY_LOOK 1"){error="Unsupported city look";return false;}
    Look next;unsigned seen=0,count=0;
    while(std::getline(file,line)) {
        if(++count>32||line.size()>512){error="City look exceeds bounded record budget";return false;}
        std::istringstream row(line);std::string kind,extra;row>>kind;
        if(kind.empty()||kind[0]=='#')continue;
        std::array<float,3> *rgb=nullptr;unsigned bit=0;
        if(kind=="sun"){rgb=&next.sun;bit=1;}else if(kind=="sky"){rgb=&next.sky;bit=2;}
        else if(kind=="ground"){rgb=&next.ground;bit=4;}else if(kind=="fog"){rgb=&next.fog;bit=8;}
        else if(kind=="zenith"){rgb=&next.zenith;bit=16;}else if(kind=="haze")bit=32;
        bool valid=bit&&!(seen&bit);
        if(rgb){valid=valid&&bool(row>>(*rgb)[0]>>(*rgb)[1]>>(*rgb)[2]);for(float v:*rgb)valid=valid&&std::isfinite(v)&&v>=0&&v<=16;}
        else if(bit==32)valid=valid&&bool(row>>next.haze)&&std::isfinite(next.haze)&&next.haze>=0&&next.haze<=.02f;
        if(!valid||(row>>extra)){error="Invalid/duplicate city look record: "+kind;return false;}
        seen|=bit;
    }
    if(!file.eof()||seen!=63){error="Incomplete city look";return false;}
    out=next;error.clear();return true;
}
}
#endif
