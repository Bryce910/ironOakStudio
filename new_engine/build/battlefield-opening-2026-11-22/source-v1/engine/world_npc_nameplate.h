#ifndef VALWOOD_WORLD_NPC_NAMEPLATE_H
#define VALWOOD_WORLD_NPC_NAMEPLATE_H
#include <algorithm>
#include <cmath>
namespace world_npc_nameplate {
constexpr double range=12,talk_range=3;
inline float opacity(double distance){return std::isfinite(distance)&&distance>=0?float(std::clamp((range-distance)/3.,0.,1.)):0;}
struct Projection {float x=0,y=0;bool visible=false;};
inline Projection project(double right,double up,double depth,double fov,float w,float h){
 if(!std::isfinite(right)||!std::isfinite(up)||!std::isfinite(depth)||!std::isfinite(fov)||depth<=.1||fov<=0||fov>=3||w<=0||h<=0)return {};
 const double lens=std::tan(fov*.5);const float x=float(w*.5+right/depth/lens*h*.5),y=float(h*.5-up/depth/lens*h*.5);
 return {x,y,x>=24&&x<=w-24&&y>=24&&y<=h-40};
}
}
#endif
