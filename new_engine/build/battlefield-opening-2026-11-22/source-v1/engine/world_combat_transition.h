#ifndef VALWOOD_WORLD_COMBAT_TRANSITION_H
#define VALWOOD_WORLD_COMBAT_TRANSITION_H
#include <algorithm>
#include <cmath>

// Render-only bridges over pose pairs that are already allocated by the bank.
// No simulation clock, damage window, root position or weapon socket is changed.
namespace world_combat_transition {
enum class OriginKind {None,Walk,WalkStart,WalkReturn,Clip,Return,Entry};
struct Origin {
    OriginKind kind=OriginKind::None;
    unsigned clip=0,index=0;
    float phase=0;
    bool moving()const{return kind==OriginKind::Walk||kind==OriginKind::WalkStart||kind==OriginKind::WalkReturn;}
    explicit operator bool()const{return kind!=OriginKind::None;}
};
enum class SampleKind {Clip,WalkStart,WalkReturn,Return,Entry};
struct Sample {SampleKind kind=SampleKind::Entry;unsigned index=0;float phase=0;};
inline float smooth(double x){const float t=float(std::clamp(x,0.,1.));return t*t*(3-2*t);}
inline float inverse_smooth(float phase){
    phase=std::clamp(phase,0.f,1.f);if(phase==0||phase==1)return phase;
    float lo=0,hi=1;
    for(unsigned i=0;i<24;++i){const float mid=(lo+hi)*.5f;if(smooth(mid)<phase)lo=mid;else hi=mid;}
    return (lo+hi)*.5f;
}
inline Sample entry(const Origin &from,double elapsed,double duration,unsigned direction){
    const double t=std::clamp(elapsed/std::max(.000001,duration),0.,1.);
    const float phase=std::clamp(from.phase,0.f,1.f);
    // A walk/reaction first reaches an existing return endpoint, then uses that
    // endpoint's certified idle pair. Other origins already lie on such a pair.
    if(from.kind==OriginKind::Walk||from.kind==OriginKind::Clip){
        const float end=from.kind==OriginKind::Walk?std::round(phase*2)*.5f:1.f;
        if(t<.2)return {SampleKind::Clip,from.clip,phase+(end-phase)*smooth(t/.2)};
        if(t<.6)return {from.kind==OriginKind::Walk?SampleKind::WalkReturn:SampleKind::Return,
            from.kind==OriginKind::Walk?(unsigned(std::lround(end*2))&1u):from.index,smooth((t-.2)/.4)};
    }else if(t<.6){
        if(from.kind==OriginKind::WalkStart)return {SampleKind::WalkStart,0,phase*(1-smooth(t/.6))};
        if(from.kind==OriginKind::WalkReturn)return {SampleKind::WalkReturn,from.index,phase+(1-phase)*smooth(t/.6)};
        if(from.kind==OriginKind::Return)return {SampleKind::Return,from.index,phase+(1-phase)*smooth(t/.6)};
        if(from.kind==OriginKind::Entry)return {SampleKind::Entry,from.index,phase*(1-smooth(t/.6))};
    }
    return {SampleKind::Entry,direction,smooth((t-.6)/.4)};
}
inline float guard_recoil(float guard_phase,double elapsed,double duration){
    const float t=float(std::clamp(elapsed/std::max(.000001,duration),0.,1.));
    const float pulse=16*t*t*(1-t)*(1-t);
    // Keep the authored directional guard. A small C1 relaxation communicates
    // the impact without replacing a high guard with the low generic block.
    return std::clamp(guard_phase,0.f,1.f)*(1-.015f*pulse);
}
}
#endif
