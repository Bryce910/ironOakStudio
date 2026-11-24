#ifndef VALWOOD_WORLD_NECK_BLOOD_H
#define VALWOOD_WORLD_NECK_BLOOD_H
#include "engine/world_combat_vfx.h"
#include "world_combat_capacity.h"
#include <array>
#include <algorithm>
#include <cmath>
namespace world_neck_blood {
using Point=world_combat_vfx::Point;
constexpr unsigned capacity=world_combat_capacity::npcs,pulse_count=5;
constexpr double pulse_times[pulse_count]={0,.10,.24,.42,.64};
struct Pose {bool severed=false,valid=false;Point point,axis={0,1,0};double floor=0;};
struct Pulse {Point point,axis;double floor=0;float strength=1;unsigned actor=0,index=0,droplets=0;};
struct Stage {std::array<Pulse,16> pulses={};unsigned count=0;};
struct Stats {unsigned long long severances=0,pulses=0;unsigned active=0,peak=0;};
class System {
 struct Emitter {bool seen=false,active=false;double age=0;unsigned next=0;};
 std::array<Emitter,capacity> emitters_={};Stage stage_;Stats stats_;double clock_=0;bool have_clock_=false;
 static bool finite(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<1e8&&std::abs(p.y)<1e7&&std::abs(p.z)<1e8;}
 static bool valid(const Pose &p){return p.valid&&finite(p.point)&&finite(p.axis)&&std::hypot(p.axis.x,p.axis.y,p.axis.z)>1e-6&&std::isfinite(p.floor)&&std::abs(p.floor)<1e7;}
public:
 void reset(){emitters_={};stage_={};have_clock_=false;stats_.active=0;}
 bool update(const std::array<Pose,capacity> &poses,double dt,double clock,bool paused=false){
  stage_={};if(!std::isfinite(dt)||dt<0||!std::isfinite(clock)||clock<0)return false;if(paused)return true;
  if(have_clock_&&clock+1e-6<clock_)reset();clock_=clock;have_clock_=true;dt=std::min(dt,.1);stats_.active=0;
  for(unsigned id=0;id<capacity;++id){auto &e=emitters_[id];const auto &p=poses[id];
   if(!p.severed){e={};continue;}
   const bool fresh=!e.seen;if(fresh){e.seen=true;e.active=valid(p);e.age=0;e.next=0;if(e.active)++stats_.severances;}
   if(!valid(p)){e.active=false;continue;}if(!e.active)continue;if(!fresh)e.age+=dt;
   const double n=std::hypot(p.axis.x,p.axis.y,p.axis.z);const Point axis{p.axis.x/n,p.axis.y/n,p.axis.z/n};
   while(e.next<pulse_count&&e.age+1e-8>=pulse_times[e.next]&&stage_.count<stage_.pulses.size()){
    if(stage_.count<stage_.pulses.size()){const unsigned i=e.next;stage_.pulses[stage_.count++]={p.point,axis,p.floor,1.95f-.10f*i,id,i,i?36u:48u};++stats_.pulses;}
    ++e.next;
   }
   if(e.next==pulse_count)e.active=false;stats_.active+=e.active;
  }
  stats_.peak=std::max(stats_.peak,stats_.active);return true;
 }
 Stage stage()const{return stage_;}Stats stats()const{return stats_;}
};
}
#endif
