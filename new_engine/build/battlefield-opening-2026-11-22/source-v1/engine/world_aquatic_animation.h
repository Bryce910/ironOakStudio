#ifndef VALWOOD_WORLD_AQUATIC_ANIMATION_H
#define VALWOOD_WORLD_AQUATIC_ANIMATION_H
#include <algorithm>
#include <cmath>

// Pose-bank selection only. This never moves the player or chooses water depth.
namespace world_aquatic_animation {
constexpr unsigned modes=3,frames=16,pose_count=modes*frames;
enum class Mode:unsigned {Tread,Stroke,Dive};
struct Sample {unsigned from=0,to=1;float weight=0,phase=0;Mode mode=Mode::Tread;bool active=false,transition=false;};
// Native sample times from the CC0 Swim_Idle_Loop / Swim_Fwd_Loop clips.
inline double duration(Mode m){return m==Mode::Tread?10./3.:4./3.;}
struct State {
    Sample value={};double phase=0,elapsed=0,moving_grace=0;unsigned transition_from=0;Mode mode=Mode::Tread;bool initialized=false,transition=false;
    Sample update(bool enabled,bool underwater,bool moving,double dt,bool paused=false){
        if(!enabled){*this=State{};return value;}
        if(!std::isfinite(dt)||dt<0)return value;
        if(paused&&initialized)return value;
        dt=paused?0:std::min(dt,.1);
        // A momentary zero-travel frame (loading/collision/stick neutral) must
        // not restart a whole new stroke or flip the body upright.
        moving_grace=moving?.20:std::max(0.,moving_grace-dt);
        const Mode desired=underwater?Mode::Dive:moving_grace>0?Mode::Stroke:Mode::Tread;
        if(!initialized){initialized=true;mode=Mode::Tread;phase=0;value.active=true;value.mode=mode;}
        if(desired!=mode){
            transition_from=value.weight<.5f?value.from:value.to;
            mode=desired;phase=double(transition_from%frames)/frames;elapsed=0;transition=true;
        }
        value.active=true;value.mode=mode;
        if(transition){
            elapsed=std::min(.38,elapsed+dt);const float t=float(elapsed/.38);
            value.from=transition_from;value.to=unsigned(mode)*frames+transition_from%frames;
            value.weight=t*t*(3-2*t);value.phase=float(phase);value.transition=true;
            if(elapsed>=.38)transition=false;
        }else{
            phase+=dt/duration(mode);phase-=std::floor(phase);
            const double key=phase*frames;const unsigned at=std::min(frames-1,unsigned(key));
            value.from=unsigned(mode)*frames+at;value.to=unsigned(mode)*frames+(at+1)%frames;
            value.weight=float(key-at);value.phase=float(phase);value.transition=false;
        }
        return value;
    }
};
}
#endif
