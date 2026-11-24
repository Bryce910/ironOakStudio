#ifndef VALWOOD_WORLD_COMBAT_DEATH_TRANSITION_H
#define VALWOOD_WORLD_COMBAT_DEATH_TRANSITION_H
#include <algorithm>
#include <cmath>

// Render-only transition. The authoritative death clock always advances at
// its original rate; this blend never delays an injury, a mask or a corpse.
namespace world_combat_death_transition {
inline constexpr float bridge_seconds=.20f;
inline constexpr float death_seconds=1.8f;
struct Sample {unsigned slot=148;float weight=0,phase=0,alpha=0;bool bridging=true;};
inline Sample sample(float seconds){
    seconds=std::isfinite(seconds)?std::clamp(seconds,0.f,death_seconds):0.f;
    Sample s;s.phase=seconds/death_seconds;
    const float frame=s.phase*23;const unsigned key=std::min(23u,unsigned(frame));
    s.slot=148+key;s.weight=frame-key;
    if(seconds>=bridge_seconds){s.alpha=1;s.bridging=false;}
    else if(seconds>0){const float t=seconds/bridge_seconds;s.alpha=t*t*(3-2*t);}
    return s;
}
// Both cut rims and the blood attachment use exactly the body blend. At each
// end return the input bit-for-bit; the caller normalizes an interpolated axis.
template<class Point> inline Point blend(Point frozen,Point current,float alpha){
    if(alpha<=0)return frozen;if(alpha>=1)return current;
    return frozen+(current-frozen)*double(alpha);
}
}
#endif
