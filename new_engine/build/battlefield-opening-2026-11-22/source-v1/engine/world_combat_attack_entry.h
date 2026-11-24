#ifndef VALWOOD_WORLD_COMBAT_ATTACK_ENTRY_H
#define VALWOOD_WORLD_COMBAT_ATTACK_ENTRY_H
#include <algorithm>
#include <cmath>

// Presentation only. The gameplay windup still ends at its original clock.
namespace world_combat_attack_entry {
inline float leadin(double windup_seconds) {
    if(!std::isfinite(windup_seconds)||windup_seconds<=0)return 0;
    return float(std::min(.09,windup_seconds*.5));
}
inline double settle_time(double leadin_seconds) {return leadin_seconds*.2;}
inline float weight(double elapsed,double leadin_seconds) {
    const float t=float(std::clamp(elapsed/std::max(.001,leadin_seconds),0.,1.));
    return t*t*(3-2*t);
}
inline float windup_fraction(double elapsed,double duration,double leadin_seconds) {
    return float(std::clamp(std::max(0.,elapsed-leadin_seconds)/std::max(.001,duration-leadin_seconds),0.,1.));
}
}
#endif
