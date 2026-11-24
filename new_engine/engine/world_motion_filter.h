#ifndef VALWOOD_WORLD_MOTION_FILTER_H
#define VALWOOD_WORLD_MOTION_FILTER_H
#include <algorithm>
#include <cmath>
namespace world_motion {
struct State {double speed=0,turn_rate=0;};
struct Step {double yaw=0,distance=0,speed=0;};
// Swept planar capsules. An already-overlapping capsule may separate, so a
// loaded contact or teleport cannot trap the player inside the other actor.
inline bool sweep_clear(double ax,double az,double bx,double bz,double px,double pz,double radius){
    const double dx=bx-ax,dz=bz-az,length2=dx*dx+dz*dz;
    const double old2=(ax-px)*(ax-px)+(az-pz)*(az-pz);
    const double new2=(bx-px)*(bx-px)+(bz-pz)*(bz-pz);
    if(old2<radius*radius)return new2>old2+1e-12&&(ax-px)*dx+(az-pz)*dz>=-1e-12;
    const double t=length2>1e-18?std::clamp(((px-ax)*dx+(pz-az)*dz)/length2,0.,1.):0;
    const double x=ax+dx*t-px,z=az+dz*t-pz;
    return x*x+z*z>=radius*radius;
}
// Bounded local steering: decelerate before arrival, plant through reversals,
// then accelerate into the new heading. Actual translation drives gait speed.
inline Step advance(State &state,double yaw,double desired_yaw,double distance,
                    double arrival,double requested_speed,double dt){
    Step out;out.yaw=yaw;
    if(dt<=0)return out;
    const double delta=std::remainder(desired_yaw-yaw,6.283185307179586);
    const double target_rate=std::clamp(delta*6.,-3.4,3.4);
    state.turn_rate+=(target_rate-state.turn_rate)*(-std::expm1(-dt/.12));
    double turn=state.turn_rate*dt;
    if(turn*delta<0)turn=0;
    if(std::abs(turn)>=std::abs(delta)){turn=delta;state.turn_rate=0;}
    out.yaw=yaw+turn;
    const double remaining=std::max(0.,distance-arrival);
    const double desired=std::abs(delta)>.85?0:std::min(requested_speed*std::max(0.,std::cos(delta)),std::sqrt(2*2.8*remaining));
    const double rate=desired>state.speed?1.8:2.8;
    state.speed+=std::clamp(desired-state.speed,-rate*dt,rate*dt);
    out.distance=std::min(remaining,state.speed*dt);
    out.speed=out.distance/dt;
    if(remaining<=1e-5){state.speed=0;out.distance=out.speed=0;}
    return out;
}
}
#endif
