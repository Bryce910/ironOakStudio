#ifndef VALWOOD_WORLD_STORY_BODY_POSE_H
#define VALWOOD_WORLD_STORY_BODY_POSE_H
#include <algorithm>
#include <array>
#include <cmath>
// Clip clocks only: root position, collision, damage and camera remain with the
// gameplay host. The renderer captures its displayed pose whenever changed=true.
namespace world_story_body_pose {
enum class Mode:unsigned {Ground,SwimIdle,SwimForward,SwimDive,Death};
constexpr double bridge_seconds=.38;
struct Input {bool swimming=false,submerged=false,dead=false,moving=false,paused=false,reset=false;double dt=0;};
struct Sample {Mode mode=Mode::Ground;double seconds=0;float blend=1;bool changed=false,transition=false,active=false,hidden=false;};
class State {
public:
 bool update(const Input &in,const std::array<double,3>&duration){
  if(!std::isfinite(in.dt)||in.dt<0)return false;
  for(double d:duration)if(!std::isfinite(d)||d<=0)return false;
  value_.changed=false;
  if(in.paused&&initialized_&&!in.reset)return true;
  const double dt=in.paused?0:std::min(.1,in.dt);
  grace_=in.moving?.20:std::max(0.,grace_-dt);
  const Mode desired=in.dead?Mode::Death:!in.swimming?Mode::Ground:in.submerged?Mode::SwimDive:grace_>0?Mode::SwimForward:Mode::SwimIdle;
  const bool changed=initialized_?(desired!=value_.mode||(in.reset&&desired!=Mode::Ground)):desired!=Mode::Ground;
  if(changed){
   const double phase=value_.mode==Mode::SwimIdle?value_.seconds/duration[0]:value_.mode==Mode::SwimForward||value_.mode==Mode::SwimDive?value_.seconds/duration[1]:0;
   value_.mode=desired;value_.seconds=(in.reset||desired==Mode::Death||desired==Mode::Ground)?0:phase*(desired==Mode::SwimIdle?duration[0]:duration[1]);
   elapsed_=0;value_.changed=true;
  }else if(in.reset){value_={};elapsed_=bridge_seconds;grace_=0;}
  else if(initialized_){
   const double d=value_.mode==Mode::SwimIdle?duration[0]:value_.mode==Mode::Death?duration[2]:duration[1];
   if(value_.mode!=Mode::Ground){value_.seconds+=dt;if(value_.mode==Mode::Death)value_.seconds=std::min(d,value_.seconds);else value_.seconds=std::fmod(value_.seconds,d);}
   elapsed_=std::min(bridge_seconds,elapsed_+dt);
  }
  initialized_=true;value_.active=value_.mode!=Mode::Ground;value_.transition=elapsed_<bridge_seconds;
  const double t=std::clamp(elapsed_/bridge_seconds,0.,1.);value_.blend=float(t*t*(3-2*t));value_.hidden=value_.active||value_.transition;return true;
 }
 const Sample &sample()const{return value_;}
 void reset(){*this=State{};}
private:Sample value_;bool initialized_=false;double elapsed_=bridge_seconds,grace_=0;
};
}
#endif
