#ifndef VALWOOD_WORLD_COMBAT_LOCOMOTION_H
#define VALWOOD_WORLD_COMBAT_LOCOMOTION_H
#include <array>
#include <algorithm>
#include <cmath>
namespace world_combat_locomotion {
inline float wrapped(float phase){return phase-std::floor(phase);}
// Upper-body/viewmodel loops converge to the distance-driven leg phase without
// replacing a completed support-key handoff with an unrelated walk sample.
inline float follow_phase(float phase,float target,float advance,float dt){
    if(!(dt>0)||!std::isfinite(dt))return wrapped(phase);
    phase+=advance;
    phase+=std::remainder(target-phase,1.f)*(1-std::exp(-std::clamp(dt,0.f,.05f)/.12f));
    return wrapped(phase);
}
// Actor +Z is forward and +X is the authored left side. Distances come from
// committed collision-resolved movement, never requested controller input.
struct State {
    float phase=0,weight=0,speed=0,forward=0,lateral=0,phase_step=0;
    float support_phase=0,support_from=0,support_elapsed=.10f;
    std::array<float,4> direction={1,0,0,0};
    bool moving=false;
    void update(double dx,double dz,float yaw,float dt,bool cut,bool enabled,const std::array<float,4>&stride){
        if(cut){*this=State{};return;}
        phase_step=0;
        if(!(dt>0)||!std::isfinite(dt))return;
        dt=std::min(dt,.05f);
        const double distance=std::hypot(dx,dz);
        if(!std::isfinite(distance)||distance>1||!std::isfinite(yaw)){*this=State{};return;}
        const double c=std::cos(yaw),s=std::sin(yaw);
        forward=float(s*dx+c*dz);lateral=float(c*dx-s*dz);
        speed=float(distance/dt);
        // Speed, rather than distance-per-frame, keeps crawl/stop behavior
        // consistent at30/60/144Hz and avoids chattering around contact jitter.
        moving=enabled&&speed>(moving?.020f:.035f);
        if(moving){
            const float denominator=std::max(.000001f,std::abs(forward)+std::abs(lateral));
            const std::array<float,4> desired={std::max(forward,0.f)/denominator,std::max(-forward,0.f)/denominator,
                std::max(lateral,0.f)/denominator,std::max(-lateral,0.f)/denominator};
            const float turn=1-std::exp(-dt/.085f);
            for(unsigned i=0;i<4;++i)direction[i]+=(desired[i]-direction[i])*turn;
            float length=0;for(unsigned i=0;i<4;++i)length+=direction[i]*stride[i];
            phase_step=float(distance)/std::max(.25f,length);phase=wrapped(phase+phase_step);
            support_from=phase;support_phase=wrapped(std::round(phase*2)*.5f);support_elapsed=0;
            weight=std::min(1.f,weight+dt/.12f);
        }else{
            // Settle toward the nearest double-support key before relaxing.
            // No gait distance accumulates against a wall or during a pause.
            support_elapsed=std::min(.10f,support_elapsed+dt);
            const float t=support_elapsed/.10f,w=t*t*(3-2*t);
            phase=wrapped(support_from+std::remainder(support_phase-support_from,1.f)*w);
            weight=std::max(0.f,weight-dt/(enabled?.16f:.075f));
        }
    }
};
}
#endif
