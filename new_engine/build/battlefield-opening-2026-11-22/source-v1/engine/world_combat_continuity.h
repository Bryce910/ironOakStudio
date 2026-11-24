#ifndef VALWOOD_WORLD_COMBAT_CONTINUITY_H
#define VALWOOD_WORLD_COMBAT_CONTINUITY_H
#include <algorithm>
#include <cmath>

// Render sampling only. Simulation durations, serials and contact times are
// never advanced, delayed or reconstructed by this policy.
namespace world_combat_continuity {
inline float smooth(double value){const float t=float(std::clamp(value,0.,1.));return t*t*(3-2*t);}
inline float fraction(double elapsed,double duration){return float(std::clamp(elapsed/std::max(.000001,duration),0.,1.));}
constexpr float combo_key=8.f/11.f;
enum class Combo {Bridge,Finish,Unwind};
struct ComboSample {float phase=0,weight=0;bool bridge=false,settle=false;};
inline ComboSample combo(Combo kind,float from,double elapsed,double duration){
    ComboSample q;from=std::clamp(from,0.f,1.f);
    if(kind==Combo::Unwind){q.settle=true;q.weight=from*(1-smooth(fraction(elapsed,duration)));}
    else if(kind==Combo::Finish)q.phase=from+(1-from)*smooth(fraction(elapsed,duration));
    else{const double catchup=duration*.2;if(elapsed<catchup)q.phase=from+(combo_key-from)*smooth(fraction(elapsed,catchup));else{q.bridge=true;q.weight=smooth(fraction(elapsed-catchup,duration-catchup));}}
    return q;
}
struct ReturnSample {float phase=0,weight=0;bool settle=false;};
inline ReturnSample returning(double elapsed,double duration,bool eased){
    ReturnSample q;const double tail=std::min(.08,std::max(0.,duration)*.25),main=duration-tail;
    if(elapsed<main||tail<=0){const float t=fraction(elapsed,main);q.phase=eased?smooth(t):t;}
    else{q.phase=1;q.settle=true;q.weight=smooth(fraction(elapsed-main,tail));}return q;
}
struct GuardSample {float phase=0,weight=0;bool entry=false;};
inline GuardSample guarding(float from,double elapsed,double duration){
    GuardSample q;const double release=duration*.6;
    if(elapsed<release)q.phase=std::clamp(from,0.f,1.f)*(1-smooth(fraction(elapsed,release)));
    else{q.entry=true;q.weight=smooth(fraction(elapsed-release,duration-release));}return q;
}
inline float idle(double elapsed){return smooth(fraction(elapsed,.14));}
}
#endif
