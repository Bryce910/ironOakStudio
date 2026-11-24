#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#define private public
#include "engine/world_skeletal_hero.h"
#undef private
#include "engine/world_knight_appearance.h"

namespace world_animation_render {
Renderer*create(const world_animation::Asset&,unsigned,std::string&,std::uint64_t){std::abort();}
void destroy(Renderer*p){if(p)std::abort();}
bool warm(Renderer*,unsigned,std::string&){std::abort();}
bool upload(Renderer*,const world_animation::Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}
bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}
}
static unsigned checks=0;
static void require(bool value,const std::string&why){++checks;if(!value){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
static bool ground(double,double,double,WorldSkeletalHero::Support&out,void*){out={};return true;}
int main(int argc,char**argv){
 require(argc==3,"usage: world_unarmed_turn_response_test RIG_DIRECTORY APPEARANCE_CONFIG");
 auto owned=std::make_unique<WorldSkeletalHero>();auto&h=*owned;std::string error;
 require(h.load_cpu((std::string(argv[1])+"/sword.glb").c_str(),error),error);
 world_knight_appearance::Config config;world_animation::Asset body;
 require(world_knight_appearance::load(argv[2],config,body,error),error);
 require(h.select_appearance(body,config.first_person,error),error);
 require(h.supports_unarmed_transitions(),"real authored transition bank loaded");
 unsigned cases=0;double worst_loop=0,worst_half_speed=0,worst_anchor=0,worst_penetration=0;
 for(double dt:{1./60.,1./23.,1./15.})for(double direction:{0.,1.5707963267948966,-1.5707963267948966,3.141592653589793})for(bool interrupt:{false,true}){
  WorldTrainingCombat fight;WorldSkeletalHero::Input in;in.equipment_stowed=true;in.ground_query=ground;in.reset=true;
  require(h.update_cpu(in,fight,error),error);in.reset=false;in.dt=dt;
  for(unsigned n=0;n<30;++n){fight.clock+=dt;require(h.update_cpu(in,fight,error),error);}
  if(interrupt){WorldSkeletalHero::MotionRequest turn;require(h.plan_unarmed_motion(.1,0,in.yaw,dt,true,false,turn,error),error);in.yaw=turn.facing_yaw;in.dx=turn.dx;in.dz=turn.dz;fight.clock+=dt;require(h.update_cpu(in,fight,error),error);require(h.stats().captured_motion_mode==4,"idle turning take started");}
  double elapsed=0,loop=-1,half=-1;
  for(unsigned n=0;elapsed<2.;++n){
   WorldSkeletalHero::MotionRequest motion;require(h.plan_unarmed_motion(1.25*std::sin(direction),1.25*std::cos(direction),in.yaw,dt,true,false,motion,error),error);
   require(motion.captured,"captured movement request used");
   require(std::hypot(motion.dx,motion.dz)<=1.25*dt+1.e-7,"turning never exceeds apartment walking speed");
   require(h.motion_mode_!=4,"forward input immediately interrupts stationary turn");
   in.dx=motion.dx;in.dz=motion.dz;in.x+=in.dx;in.z+=in.dz;in.yaw=motion.facing_yaw;fight.clock+=dt;
   require(h.update_cpu(in,fight,error),error);elapsed+=dt;
   if(std::hypot(in.dx,in.dz)/dt>=.625&&half<0)half=elapsed;
   if(h.stats().captured_motion_mode==2&&loop<0)loop=elapsed;
   for(unsigned side=0;side<2;++side){const auto&foot=h.feet_[side];
    for(unsigned vertex:foot.vertices){world_animation::Vec3 p,normal;require(world_animation::deform_vertex(h.asset().mesh.vertices[vertex],h.pose(),p,normal,error),error);p=world_animation::transform_point(h.pose().global[h.asset().skeleton.mesh_node],p);worst_penetration=std::min(worst_penetration,double(p.y));}
    if(foot.planted&&foot.weight>.99f){world_animation::Vec3 p;require(h.sole_point(h.pose(),side,foot.marker,p,error),error);const double x=in.x+std::cos(in.yaw)*p.x+std::sin(in.yaw)*p.z,z=in.z-std::sin(in.yaw)*p.x+std::cos(in.yaw)*p.z;worst_anchor=std::max(worst_anchor,std::hypot(x-foot.anchor[0],z-foot.anchor[2]));}
   }
   for(unsigned j=0;j<h.pose().nodes;++j){for(float v:h.pose().global[j].m)require(std::isfinite(v),"finite pose");const auto&q=h.pose().local[j].rotation;require(std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<.005,"unit quaternion");}
  }
  require(half>=0&&half<=.36+dt,"walking responds within 360ms plus one renderer frame");
  require(loop>=0&&loop<=.90+dt,"turn transitions to forward loop within 900ms plus one renderer frame");
  worst_loop=std::max(worst_loop,loop);worst_half_speed=std::max(worst_half_speed,half);
  const auto frozen=h.pose();const double phase=h.unarmed_motion_time_;in.paused=true;in.dx=in.dz=0;
  for(unsigned n=0;n<5;++n){WorldSkeletalHero::MotionRequest motion;require(h.plan_unarmed_motion(-1.25,0,in.yaw,dt,true,true,motion,error),error);require(motion.dx==0&&motion.dz==0,"paused request stationary");require(h.update_cpu(in,fight,error),error);require(h.unarmed_motion_time_==phase,"pause freezes source clock");for(unsigned j=0;j<frozen.nodes;++j)require(h.pose().global[j].m==frozen.global[j].m,"pause freezes skeleton");}
  ++cases;
 }
 unsigned release_cases=0;double maximum_release_drift=0,maximum_released_speed=0;
 // The faster shortened start must not cross an early-tap speed threshold
 // and enqueue the original full-distance stop after the input is released.
 for(double frame_dt:{1./60.,1./23.,1./15.})for(double direction:{1.5707963267948966,-1.5707963267948966,3.141592653589793})for(double release_at:{.1,.2,.35,.5}){
  WorldTrainingCombat fight;WorldSkeletalHero::Input in;in.equipment_stowed=true;in.ground_query=ground;in.reset=true;
  require(h.update_cpu(in,fight,error),error);in.reset=false;in.dt=frame_dt;
  for(unsigned n=0;n<30;++n){fight.clock+=in.dt;require(h.update_cpu(in,fight,error),error);}
  double elapsed=0,drift=0;
  while(elapsed<1.5-1.e-8){
   double dt=std::min(frame_dt,1.5-elapsed);if(elapsed<release_at-1.e-8&&elapsed+dt>release_at)dt=release_at-elapsed;
   const bool released=elapsed>=release_at-1.e-8;const double wish=released?0:1.25;
   WorldSkeletalHero::MotionRequest motion;require(h.plan_unarmed_motion(wish*std::sin(direction),wish*std::cos(direction),in.yaw,dt,true,false,motion,error),error);
   require(std::hypot(motion.dx,motion.dz)<=1.25*dt+1.e-7,"release fixture keeps apartment speed cap");
   if(released){drift+=std::hypot(motion.dx,motion.dz);maximum_released_speed=std::max(maximum_released_speed,std::hypot(motion.dx,motion.dz)/dt);}
   in.dt=dt;in.dx=motion.dx;in.dz=motion.dz;in.x+=in.dx;in.z+=in.dz;in.yaw=motion.facing_yaw;fight.clock+=dt;
   require(h.update_cpu(in,fight,error),error);elapsed+=dt;
   for(unsigned side=0;side<2;++side){const auto&foot=h.feet_[side];
    for(unsigned vertex:foot.vertices){world_animation::Vec3 p,normal;require(world_animation::deform_vertex(h.asset().mesh.vertices[vertex],h.pose(),p,normal,error),error);p=world_animation::transform_point(h.pose().global[h.asset().skeleton.mesh_node],p);worst_penetration=std::min(worst_penetration,double(p.y));}
    if(foot.planted&&foot.weight>.99f){world_animation::Vec3 p;require(h.sole_point(h.pose(),side,foot.marker,p,error),error);const double x=in.x+std::cos(in.yaw)*p.x+std::sin(in.yaw)*p.z,z=in.z-std::sin(in.yaw)*p.x+std::cos(in.yaw)*p.z;worst_anchor=std::max(worst_anchor,std::hypot(x-foot.anchor[0],z-foot.anchor[2]));}
   }
  }
  require(drift<1.e-7,"shortened turn start does not queue root travel after release");
  require(h.stats().captured_motion_mode==0,"released shortened start settles to idle");
  maximum_release_drift=std::max(maximum_release_drift,drift);++release_cases;
 }
 require(worst_penetration>-.002,"actual civilian boots stay above flat ground within 2mm");
 require(worst_anchor<.005,"planted sole remains within 5mm of physical anchor");
 // Hash a disabled/civilian-to-armed path for a baseline-vs-candidate check.
 // The turn-response policy must never alter ordinary combat locomotion.
 auto combat_owned=std::make_unique<WorldSkeletalHero>();auto&combat=*combat_owned;require(combat.load_cpu((std::string(argv[1])+"/sword.glb").c_str(),error),error);require(combat.select_appearance(body,config.first_person,error),error);
 std::uint64_t hash=1469598103934665603ull;WorldTrainingCombat fight;WorldSkeletalHero::Input in;in.reset=true;in.ground_query=ground;in.dt=1./60.;
 for(unsigned n=0;n<240;++n){WorldSkeletalHero::MotionRequest disabled;require(combat.plan_unarmed_motion(0,0,in.yaw,in.dt,false,false,disabled,error),error);in.dx=n>=60&&n<180?.025:0;in.dz=n>=60&&n<180?.025:0;in.x+=in.dx;in.z+=in.dz;fight.clock+=in.dt;fight.guard=n>=180;fight.selected=TrainingDirection((n/12)%5);require(combat.update_cpu(in,fight,error),error);in.reset=false;for(unsigned j=0;j<combat.pose().nodes;++j)for(float value:combat.pose().global[j].m){std::uint32_t bits;std::memcpy(&bits,&value,4);hash^=bits;hash*=1099511628211ull;}}
 std::cout<<"{\"passed\":true,\"held_start_cases\":"<<cases<<",\"release_cases\":"<<release_cases<<",\"assertions\":"<<checks<<",\"maximum_loop_entry_seconds\":"<<worst_loop<<",\"maximum_half_walk_speed_seconds\":"<<worst_half_speed<<",\"maximum_release_drift_m\":"<<maximum_release_drift<<",\"maximum_released_speed_mps\":"<<maximum_released_speed<<",\"minimum_boot_clearance_m\":"<<worst_penetration<<",\"maximum_strong_anchor_error_m\":"<<worst_anchor<<",\"disabled_combat_pose_hash\":\""<<std::hex<<hash<<"\"}\n";
}
