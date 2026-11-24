#ifndef VALWOOD_OPENING_DIALOGUE_CAMERA_H
#define VALWOOD_OPENING_DIALOGUE_CAMERA_H
#include <algorithm>
#include <cmath>
#include <cstdint>

// Render-only conversation framing. The playable capsule and its look angles
// remain authoritative; all spatial movement must pass the host's camera cast.
namespace world_opening_dialogue_camera {
struct Pose {double x=0,y=0,z=0,yaw=0,pitch=0,fov=1.0471975512,letterbox=0;};
inline bool finite(const Pose &p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.yaw)&&std::isfinite(p.pitch)&&std::isfinite(p.fov)&&std::isfinite(p.letterbox);}
inline Pose mix(const Pose &a,const Pose &b,double t){
    auto lerp=[&](double x,double y){return x+(y-x)*t;};
    return {lerp(a.x,b.x),lerp(a.y,b.y),lerp(a.z,b.z),a.yaw+std::remainder(b.yaw-a.yaw,6.283185307179586)*t,lerp(a.pitch,b.pitch),lerp(a.fov,b.fov),lerp(a.letterbox,b.letterbox)};
}
// A cinematic eye may occupy the first-person head position because its
// player is hidden. Once the visible third-person body returns, neither that
// endpoint nor a chord through the body is a valid camera interpolation.
// Reuse the already swept follow position in that case; retain the authored
// orientation/lens/letterbox for the short render-only handoff.
inline Pose body_safe_handoff_start(const Pose &from,const Pose &base,double x,double feet,double z){
    if(!finite(from)||!finite(base)||!std::isfinite(x)||!std::isfinite(feet)||!std::isfinite(z))return base;
    if(std::max(from.y,base.y)<feet-.15||std::min(from.y,base.y)>feet+2.15)return from;
    const double dx=base.x-from.x,dz=base.z-from.z,length=dx*dx+dz*dz;
    const double t=length>1.e-12?std::clamp(((x-from.x)*dx+(z-from.z)*dz)/length,0.,1.):0;
    const double clearance=std::hypot(from.x+dx*t-x,from.z+dz*t-z);
    if(clearance>=1.10)return from;
    Pose safe=from;safe.x=base.x;safe.y=base.y;safe.z=base.z;return safe;
}
struct State {
    Pose current{},start{},last_base{};
    bool initialized=false,dialogue=false,blending=false,intro_handoff=false,handoff_follows_base=false;
    double age=0,duration=.32;
    std::uint64_t entries=0,exits=0,blend_frames=0,blocked_frames=0;
    void forget(){initialized=false;dialogue=blending=intro_handoff=handoff_follows_base=false;age=0;}
    bool handoff(const Pose &from,const Pose &base,bool position_follows_base=false){
        if(!finite(from)||!finite(base)||std::hypot(from.x-base.x,from.y-base.y,from.z-base.z)>8)return false;
        current=start=from;last_base=base;initialized=true;dialogue=false;blending=intro_handoff=true;handoff_follows_base=position_follows_base;age=0;duration=.45;return true;
    }
    bool transitioning()const{return blending;}
    bool conversation_transitioning()const{return blending&&!intro_handoff;}
    template<class Clear> Pose update(const Pose &base,const Pose &goal,bool active,double dt,bool paused,bool reduced,Clear clear){
        if(!finite(base)||!finite(goal))return initialized?current:Pose{};
        // Loading/transport/restore never interpolate across distant worlds.
        if(!initialized||std::hypot(base.x-last_base.x,base.y-last_base.y,base.z-last_base.z)>20){
            current=start=last_base=base;initialized=true;dialogue=blending=intro_handoff=handoff_follows_base=false;age=0;
        }
        if(active!=dialogue){start=current;age=0;duration=.32;blending=true;intro_handoff=handoff_follows_base=false;dialogue=active;if(active)++entries;else ++exits;}
        const Pose target=active?goal:base;
        if(paused)return current;
        dt=std::clamp(std::isfinite(dt)?dt:0.,0.,.05);
        Pose next=target;
        if(blending&&!reduced){
            age=std::min(duration,age+dt);const double x=age/duration,t=x*x*(3.-2.*x);
            next=mix(start,target,t);++blend_frames;if(age>=duration)blending=false;
        }else blending=false;
        // An unsafe authored eye hands off lens/orientation only. Keep its
        // spatial component on this frame's swept follow camera even while
        // the player walks or rotates immediately after taking control.
        if(intro_handoff&&handoff_follows_base){next.x=base.x;next.y=base.y;next.z=base.z;}
        if(!clear(base,next)){next=base;++blocked_frames;}
        current=next;last_base=base;return current;
    }
};
}
#endif
