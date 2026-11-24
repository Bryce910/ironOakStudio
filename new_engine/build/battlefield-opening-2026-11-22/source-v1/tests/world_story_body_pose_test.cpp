#include "engine/world_story_body_pose.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace world_story_body_pose;
int main(){
 const std::array<double,3>d={2,4./3.,1.8};State s;Input i;i.dt=1./60.;assert(s.update(i,d));assert(!s.sample().active&&!s.sample().hidden);
 i.swimming=true;assert(s.update(i,d));assert(s.sample().mode==Mode::SwimIdle&&s.sample().changed&&s.sample().blend==0&&s.sample().hidden);
 for(int n=0;n<30;++n){assert(s.update(i,d));}assert(!s.sample().transition&&s.sample().seconds>.49&&s.sample().seconds<.51);
 const auto pause=s.sample();i.paused=true;i.moving=true;i.submerged=true;for(int n=0;n<20;++n){assert(s.update(i,d));}assert(s.sample().mode==pause.mode&&s.sample().seconds==pause.seconds&&s.sample().blend==pause.blend);
 i.paused=false;assert(s.update(i,d));assert(s.sample().mode==Mode::SwimDive&&s.sample().changed&&s.sample().blend==0);
 i.submerged=false;assert(s.update(i,d));assert(s.sample().mode==Mode::SwimForward);i.moving=false;for(int n=0;n<10;++n){assert(s.update(i,d));}assert(s.sample().mode==Mode::SwimForward);for(int n=0;n<4;++n){assert(s.update(i,d));}assert(s.sample().mode==Mode::SwimIdle);
 i.swimming=false;assert(s.update(i,d));assert(!s.sample().active&&s.sample().transition&&s.sample().hidden&&s.sample().blend==0);for(int n=0;n<30;++n){assert(s.update(i,d));}assert(!s.sample().hidden);
 i.dead=true;assert(s.update(i,d));assert(s.sample().mode==Mode::Death&&s.sample().changed&&s.sample().seconds==0);for(int n=0;n<300;++n){assert(s.update(i,d));}assert(s.sample().seconds==d[2]&&s.sample().hidden);assert(s.update(i,d)&&s.sample().seconds==d[2]);
 i.dead=false;i.reset=true;assert(s.update(i,d));assert(s.sample().mode==Mode::Ground&&s.sample().transition&&s.sample().changed&&s.sample().hidden);i.reset=false;for(int n=0;n<30;++n){assert(s.update(i,d));}assert(!s.sample().hidden);
 // The displayed bridge completes in authored seconds across common rates.
 for(double hz:{30.,60.,120.}){State rate;Input x;x.dt=1/hz;assert(rate.update(x,d));x.swimming=true;assert(rate.update(x,d));unsigned frames=0;while(rate.sample().transition){assert(rate.update(x,d));assert(++frames<100);}assert(frames/hz>=bridge_seconds-1e-10&&frames/hz<bridge_seconds+1/hz+1e-10);}
 // Long runs use the authored duration, never a fabricated16-frame bank clock.
 i.swimming=true;assert(s.update(i,d));for(int n=0;n<120;++n){assert(s.update(i,d));}assert(std::abs(s.sample().seconds)<1e-12||std::abs(s.sample().seconds-2)<1e-12);
 i.dt=-1;assert(!s.update(i,d));i.dt=NAN;assert(!s.update(i,d));i.dt=.016;assert(!s.update(i,{0,1,1}));
 std::cout<<"story body clock/entry/mode/grace/pause/exit/death/reset checks passed\n";
}
