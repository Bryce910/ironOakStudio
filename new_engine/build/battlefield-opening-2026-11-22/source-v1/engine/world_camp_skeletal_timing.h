#ifndef VALWOOD_WORLD_CAMP_SKELETAL_TIMING_H
#define VALWOOD_WORLD_CAMP_SKELETAL_TIMING_H
#include <algorithm>
#include <array>
#include <cmath>

namespace world_camp_skeletal_timing {
// The retained clips were authored as slow presentation loops: one talking
// gesture took eight seconds and a listening nod took five. Keep the breathing
// pace, but let social actions read at conversational speed. These are distinct
// clip rates, rather than accelerating the entire person (including breathing).
constexpr std::array<double,4> clip_rates={1.0,1.7,2.2,1.6};
inline double seconds(unsigned clip,unsigned actor,double now){
    return now*clip_rates[clip]*(.98+.02*double(actor))+double(actor)*2.17;
}
inline std::array<float,4> target(bool speaking,bool listening,bool warm_hands){
    return speaking?std::array<float,4>{.10f,0,.90f,0}:
        listening?std::array<float,4>{.18f,.82f,0,0}:
        warm_hands?std::array<float,4>{.22f,0,0,.78f}:
        std::array<float,4>{1,0,0,0};
}
inline void approach(std::array<float,4> &weights,const std::array<float,4> &goal,double dt){
    // A full response is visible within a third of a second. The exponential
    // remains invariant to render frequency; an interrupted render may settle
    // a transition without slowing the sampled skeletal action itself.
    const float blend=float(-std::expm1(-9.*std::clamp(dt,0.,.25)));
    for(unsigned k=0;k<weights.size();++k)weights[k]+=(goal[k]-weights[k])*blend;
}
}
#endif
