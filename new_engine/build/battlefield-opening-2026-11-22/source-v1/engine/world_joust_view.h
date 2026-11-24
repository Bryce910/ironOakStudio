#ifndef VALWOOD_WORLD_JOUST_VIEW_H
#define VALWOOD_WORLD_JOUST_VIEW_H
#include "engine/world_joust.h"

// Arena-local view only. The camera samples the same rider transform used by
// contacts and the rendered body; an unhorsed eye never follows an empty saddle.
namespace world_joust_view {
struct View { world_joust::Vec3 eye; float yaw=0,pitch=0,fov=1.03f; bool fall=false; };
inline View sample(const world_joust::Game &g,bool third_person,bool reduced_motion,float feedback_age){
    using namespace world_joust;
    const auto &r=g.player;const float c=std::cos(r.horse.yaw),s=std::sin(r.horse.yaw);
    View v;v.yaw=pi-r.horse.yaw;
    if(r.unhorsed||g.opponent.unhorsed){
        const auto &fallen=r.unhorsed?r:g.opponent;
        const Vec3 target=rider_eye(fallen)+Vec3{0,-.4f,0};
        // A readable exterior view avoids looking through the rider's own
        // helmet/ground during a fall, including when first person was selected.
        // Stay over the track. A spectator-side camera can enter an awning
        // as an ejected rider travels toward the outer fence.
        v.eye={std::clamp(target.x*.15f,-1.f,1.f),std::max(4.8f,target.y+3.3f),target.z-6.5f*c};
        const Vec3 direction=target-v.eye;
        v.yaw=std::atan2(direction.x,-direction.z);
        v.pitch=std::atan2(direction.y,std::hypot(direction.x,direction.z));
        v.fov=.98f;v.fall=true;
    }else if(third_person){
        const Vec3 movement=rider_eye(r)-(r.horse.position+Vec3{0,2.88f,.04f});
        v.eye=r.horse.position+Vec3{-s*7.5f-c,4.6f,-c*7.5f+s}+movement*.45f;
        v.pitch=-.16f;
        if(g.phase==Phase::Setup){
            v.eye=r.horse.position+Vec3{-11.f,7.8f,-10.f};
            const Vec3 target={0,2.f,-16.f},direction=target-v.eye;
            v.yaw=std::atan2(direction.x,-direction.z);
            v.pitch=std::atan2(direction.y,std::hypot(direction.x,direction.z));
            v.fov=1.03f;
        }
    }else{
        v.eye=rider_eye(r)+Vec3{-s*.45f,0,-c*.45f};v.pitch=-.07f;
    }
    if(!reduced_motion&&feedback_age>=0&&feedback_age<.22f)
        v.pitch+=.033f*std::sin(feedback_age*39.f)*std::exp(-feedback_age*12.f);
    return v;
}
}
#endif
