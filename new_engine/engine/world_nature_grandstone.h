#ifndef VALWOOD_WORLD_NATURE_GRANDSTONE_H
#define VALWOOD_WORLD_NATURE_GRANDSTONE_H
#include <cmath>
#include <initializer_list>
// Local coordinates for the supplied construction assemblies. These caps
// suppress weather and powder beneath the occupied roofs, not on open streets.
inline float world_nature_grandstone_roof(double x,double z){
    if(std::abs(x)<75&&z>-236&&z<12)return 47;
    if(std::abs(x)<10&&z>=12&&z<40)return 38;
    for(double side:{-1.,1.})for(double cz:{129.,168.})
        if(std::abs(x-side*32)<12&&std::abs(z-cz)<8)return 21;
    if(std::abs(x+69)<21&&std::abs(z-83)<8)return 27;
    for(double cz:{73.,96.})if(std::abs(x-72)<8&&std::abs(z-cz)<8)return 13;
    return 0;
}
#endif
