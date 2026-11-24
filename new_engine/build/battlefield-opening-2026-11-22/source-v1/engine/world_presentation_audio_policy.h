#ifndef VALWOOD_PRESENTATION_AUDIO_POLICY_H
#define VALWOOD_PRESENTATION_AUDIO_POLICY_H

#include <algorithm>
#include <array>
#include <cmath>

namespace world_presentation_audio_policy {
enum class CueDecision { Invalid, Muted, Duplicate, Play };

// No mixer or world state is needed to decide whether a story beat may sound.
// The host supplies current permission before dispatch; muted beats are dropped,
// never queued for a surprise replay when a menu closes or focus returns.
struct State {
    static constexpr double duplicate_window=.22;
    std::array<double,5> last_cue={{-1.,-1.,-1.,-1.,-1.}};
    double clock=0;
    float gain=0,tension=0,level=0;
    bool context_set=false,allowed=false;

    bool audible()const{return context_set&&allowed&&gain>0;}
    void set_context(bool permission,float volume){
        context_set=true;allowed=permission;
        gain=std::isfinite(volume)?std::clamp(volume,0.f,1.f):0.f;
        // Volume reductions, mute and focus/menu gates take effect immediately.
        // Returning to an audible context fades the looping score back in.
        level=audible()?std::min(level,gain):0.f;
    }
    void advance(float dt,bool combat){
        const float seconds=std::isfinite(dt)?std::clamp(dt,0.f,.1f):0.f;
        clock+=seconds;
        const float blend=1-std::exp(-seconds*1.8f);
        tension+=(float(combat)-tension)*blend;
        level+=((audible()?gain:0.f)-level)*blend;
        level=std::clamp(level,0.f,gain);
    }
    CueDecision decision(unsigned cue)const{
        if(cue<2||cue>=last_cue.size())return CueDecision::Invalid;
        if(!audible())return CueDecision::Muted;
        if(last_cue[cue]>=0&&clock-last_cue[cue]<duplicate_window)
            return CueDecision::Duplicate;
        return CueDecision::Play;
    }
    void started(unsigned cue){if(cue>=2&&cue<last_cue.size())last_cue[cue]=clock;}
    std::array<int,3> volumes()const{
        if(!audible())return {{0,0,0}};
        return {{int(55*level*(1-tension)),int(63*level*tension),int(std::lround(65*gain))}};
    }
};
}
#endif
