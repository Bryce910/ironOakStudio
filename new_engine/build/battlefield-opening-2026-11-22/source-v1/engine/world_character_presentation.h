#ifndef VALWOOD_WORLD_CHARACTER_PRESENTATION_H
#define VALWOOD_WORLD_CHARACTER_PRESENTATION_H
#include <algorithm>
#include <cmath>

namespace world_character_presentation {
// Visual free-walk heading only. Combat continues to use authoritative aim.
inline float walking_yaw(float current,double target,double dt) {
    if(!std::isfinite(current)||!std::isfinite(target)||!std::isfinite(dt)||dt<=0)return current;
    constexpr double tau=6.283185307179586;
    const double delta=std::remainder(target-current,tau),step=std::min(dt,.1);
    const double desired=delta*(-std::expm1(-14*step));
    return float(std::remainder(current+std::clamp(desired,-tau*step,tau*step),tau));
}

// Dampen small grounded step heights before the existing camera collision
// sweeps. Airborne movement and genuine world/view changes remain immediate.
struct StepHeight {
    double y=0,x=0,z=0;
    bool valid=false;
    template<class Clear>
    double update(double px,double physical_y,double pz,double dt,bool grounded,bool enabled,Clear clear) {
        if(!std::isfinite(px)||!std::isfinite(physical_y)||!std::isfinite(pz)||!std::isfinite(dt))return physical_y;
        const bool reset=!valid||!enabled||!grounded||std::hypot(px-x,pz-z)>=1||std::abs(physical_y-y)>.65;
        double next=physical_y;
        if(!reset) {
            next=y+(physical_y-y)*(-std::expm1(-std::clamp(dt,0.,.1)/.10));
            next=std::clamp(next,physical_y-.35,physical_y+.35);
            if(!clear(physical_y,next))next=physical_y;
        }
        x=px;z=pz;y=next;valid=enabled;
        return y;
    }
};
}
#endif
