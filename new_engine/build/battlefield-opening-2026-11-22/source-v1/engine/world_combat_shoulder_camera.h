#ifndef VALWOOD_WORLD_COMBAT_SHOULDER_CAMERA_H
#define VALWOOD_WORLD_COMBAT_SHOULDER_CAMERA_H
#include <algorithm>
#include <cmath>
struct WorldCombatShoulderCamera {double x,z;float offset;bool blocked;};
inline constexpr float world_combat_shoulder_maximum=2.30f;
// A close melee opponent needs room beside the hero and around a raised weapon.
// This is a vertical lens target, never a change to logical aim or weapon reach.
inline float world_combat_close_melee_fov(double distance){
    if(!std::isfinite(distance))return 1.08f;
    const float t=std::clamp(float((distance-1.15)/2.15),0.f,1.f);
    const float open=1-t*t*(3-2*t);
    return 1.08f+.14f*open;
}
// The existing player-centered boom owns the base position. A small additional
// render-only offset exposes the weapon/opponent beside the hero. Each segment
// uses the same floor and capsule contract, with immediate collision retraction.
template<class Height,class Clear>
WorldCombatShoulderCamera world_combat_shoulder_camera(double x,double y,double z,float yaw,float previous,float boom,float dt,bool active,bool cut,Height height,Clear clear,float requested_maximum=1.04f){
    const float maximum=std::isfinite(requested_maximum)?std::clamp(requested_maximum,0.f,world_combat_shoulder_maximum):1.04f;
    // Retain useful lateral separation when the follow boom contracts near a
    // wall. Every added centimetre still goes through the capsule sweep below.
    const float target=active?maximum*std::clamp((boom-.55f)/1.85f,0.f,1.f):0;
    const float weight=1-std::exp(-std::clamp(dt,0.f,.1f)/.14f);
    const float requested=cut?target:std::clamp(previous+(target-previous)*weight,0.f,maximum);
    const double sx=std::cos(yaw),sz=std::sin(yaw);double px=x,pz=z;float offset=0;
    const unsigned steps=std::min(29u,unsigned(std::ceil(requested/.08f)));
    for(unsigned i=1;i<=steps;++i){
        const float d=std::min(requested,.08f*i);const double nx=x+sx*d,nz=z+sz*d;
        if(height(nx,nz)+.22>y||!clear(px,pz,nx,nz,float(y-.18),.36f,.22f))return {px,pz,offset,true};
        px=nx;pz=nz;offset=d;
    }
    return {px,pz,offset,false};
}
#endif
