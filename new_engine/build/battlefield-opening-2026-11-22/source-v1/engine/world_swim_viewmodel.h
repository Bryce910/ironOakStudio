#ifndef VALWOOD_WORLD_SWIM_VIEWMODEL_H
#define VALWOOD_WORLD_SWIM_VIEWMODEL_H
#include "engine/world_story_body_pose.h"
#include <algorithm>
#include <cmath>

// The armed viewmodel deliberately reaches toward the lens. Swimming uses
// the captured head as their registration point. The moving swim capture
// translates the head forward, so a fixed standing-eye origin would expose
// detached shoulder cuts. World body/camera/contacts are not moved here.
namespace world_swim_viewmodel {
class State {
public:
 void update(bool swimming,double dt,bool paused,bool reset){
  if(!std::isfinite(dt)||dt<0)return;
  const float goal=swimming?1.f:0.f;
  if(reset){weight_=goal;return;}
  if(paused)return;
  const float step=float(std::min(dt,.1)/world_story_body_pose::bridge_seconds);
  weight_=goal>weight_?std::min(goal,weight_+step):std::max(goal,weight_-step);
 }
 float weight()const{return weight_;}
 template<class Instance,class Head>
 Instance apply(Instance armed,const Head&head)const{
  if(weight_<=0)return armed;
  const float blend=weight_*weight_*(3-2*weight_);
  constexpr float pi=3.14159265358979323846f;
  armed.x+=(head.x-armed.x)*blend;
  armed.y+=(-head.y-armed.y)*blend;
  armed.z+=(head.z-armed.z)*blend;
  armed.yaw=pi+std::remainder(armed.yaw-pi,2*pi)*(1-blend);
  return armed;
 }
 void reset(){weight_=0;}
private:
 float weight_=0;
};
}
#endif
