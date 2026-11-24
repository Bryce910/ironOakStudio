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
 require(argc==3,"usage: world_unarmed_stop_response_test RIG_DIRECTORY APPEARANCE_CONFIG");
 auto owner=std::make_unique<WorldSkeletalHero>();auto&h=*owner;std::string error;
 require(h.load_cpu((std::string(argv[1])+"/sword.glb").c_str(),error),error);
 world_knight_appearance::Config config;world_animation::Asset body;
 require(world_knight_appearance::load(argv[2],config,body,error),error);
 require(h.select_appearance(body,config.first_person,error),error);
 require(h.supports_unarmed_transitions(),"authored civilian transition bank loaded");
 unsigned cases=0;double worst_drift[3]={},worst_stop[3]={},worst_anchor=0,worst_penetration=0,worst_sole_speed=0;
 // All fixtures use the same actual rig, captured root trajectories, collision
 // acceptance path, and rendered civilian boot vertices as normal gameplay.
 for(double dt:{1./23.,1./30.,1./60.,1./120.})for(unsigned gait=0;gait<3;++gait)for(unsigned scenario=0;scenario<6;++scenario){
  const double speed=gait==0?1.25:gait==1?3.4:5.78,budget=gait==0?.20:gait==1?.30:.45;
  WorldTrainingCombat fight;WorldSkeletalHero::Input in;in.equipment_stowed=true;in.ground_query=ground;in.reset=true;
  require(h.update_cpu(in,fight,error),error);in.reset=false;in.dt=dt;
  for(unsigned n=0;n<30;++n){fight.clock+=dt;require(h.update_cpu(in,fight,error),error);}
  double elapsed=0,drift=0,last_moving=0;bool resumed=false,paused=false;
  std::array<std::array<double,3>,4> previous={};bool have_previous=false;
  const double release=scenario==1?.12:scenario==2?1.18:1.5;
  const double finish=release+2;
  while(elapsed<finish-1.e-8){
   const double step=std::min(dt,finish-elapsed);double wish=elapsed<release-1.e-8?speed:0;
   if(scenario==2&&elapsed>=.9)wish=elapsed<release-1.e-8?-speed:0;
   if(scenario==3&&elapsed>=release+.08&&elapsed<release+.38){wish=speed;resumed=true;}
   const bool final_stop=elapsed>=release+(scenario==3?.38:0)-1.e-8;
   if(scenario==4&&final_stop&&!paused){
    // Pause freezes the displayed pose and captured root/source clock, then
    // a resumed release continues its same bounded physical trajectory.
    const auto pose=h.pose();const double phase=h.unarmed_motion_time_;
    auto input=in;input.paused=true;input.dx=input.dz=0;input.dt=dt;
    for(unsigned n=0;n<5;++n){WorldSkeletalHero::MotionRequest held;require(h.plan_unarmed_motion(0,0,in.yaw,dt,true,true,held,error),error);require(held.dx==0&&held.dz==0,"pause produces no root travel");require(h.update_cpu(input,fight,error),error);require(h.unarmed_motion_time_==phase,"pause freezes source phase");for(unsigned j=0;j<pose.nodes;++j)require(h.pose().global[j].m==pose.global[j].m,"pause freezes pose");}
    paused=true;
   }
   WorldSkeletalHero::MotionRequest motion;require(h.plan_unarmed_motion(0,wish,in.yaw,step,true,false,motion,error),error);
   require(motion.captured,"captured request owns the root");
   require(std::hypot(motion.dx,motion.dz)<=speed*step+1.e-6,"root never exceeds incoming speed cap");
   const bool blocked=scenario==5&&final_stop;
   in.dt=step;in.dx=blocked?0:motion.dx;in.dz=blocked?0:motion.dz;in.x+=in.dx;in.z+=in.dz;in.yaw=motion.facing_yaw;fight.clock+=step;
   require(h.update_cpu(in,fight,error),error);
   if(final_stop){drift+=std::hypot(in.dx,in.dz);if(std::hypot(in.dx,in.dz)/step>.01)last_moving=elapsed-release-(scenario==3?.38:0)+step;}
   for(unsigned side=0;side<2;++side){const auto&foot=h.feet_[side];
    if(foot.planted&&foot.weight>.99f){world_animation::Vec3 point;require(h.sole_point(h.pose(),side,foot.marker,point,error),error);const double x=in.x+std::cos(in.yaw)*point.x+std::sin(in.yaw)*point.z,z=in.z-std::sin(in.yaw)*point.x+std::cos(in.yaw)*point.z;worst_anchor=std::max(worst_anchor,std::hypot(x-foot.anchor[0],z-foot.anchor[2]));}
    for(unsigned marker=0;marker<2;++marker){world_animation::Vec3 p;require(h.sole_point(h.pose(),side,marker,p,error),error);const std::array<double,3> point={in.x+std::cos(in.yaw)*p.x+std::sin(in.yaw)*p.z,p.y,in.z-std::sin(in.yaw)*p.x+std::cos(in.yaw)*p.z};const unsigned id=side*2+marker;if(final_stop&&have_previous){const auto&old=previous[id];const double distance=std::sqrt(std::pow(point[0]-old[0],2)+std::pow(point[1]-old[1],2)+std::pow(point[2]-old[2],2));worst_sole_speed=std::max(worst_sole_speed,distance/step);if(distance>=.04+step*10)std::cerr<<"sole discontinuity dt="<<dt<<" gait="<<gait<<" scenario="<<scenario<<" elapsed="<<elapsed<<" point="<<id<<" distance="<<distance<<" mode="<<h.motion_mode_<<"\n";require(distance<.04+step*10,"sole trajectory remains continuous without teleports");}previous[id]=point;}
    // Inspect every actual boot vertex through the complete stop at 30Hz.
    if(final_stop&&unsigned(elapsed/dt)%std::max(1u,unsigned(std::round(1./(dt*30))))==0)
     for(unsigned vertex:foot.vertices){world_animation::Vec3 p,normal;require(world_animation::deform_vertex(h.asset().mesh.vertices[vertex],h.pose(),p,normal,error),error);p=world_animation::transform_point(h.pose().global[h.asset().skeleton.mesh_node],p);worst_penetration=std::min(worst_penetration,double(p.y));}
   }
   have_previous=true;elapsed+=step;
  }
  require(drift<=budget+1.e-5,"release root travel stays within its responsive stopping budget");
  require(last_moving<=.65+dt,"accepted root settles within 650ms plus one frame");
  require(h.stats().captured_motion_mode==0,"stop settles to idle");
  if(scenario==3)require(resumed,"ordinary movement resumed during stopping");
  if(scenario==5)require(drift==0&&h.stats().captured_blocked>0,"blocked stop accepts no root travel and terminates");
  worst_drift[gait]=std::max(worst_drift[gait],drift);worst_stop[gait]=std::max(worst_stop[gait],last_moving);++cases;
 }
 require(worst_anchor<.005,"strong planted sole stays within 5mm of its physical anchor");
 require(worst_penetration>-.002,"actual civilian boots stay above ground within 2mm");
 std::cout<<"{\"passed\":true,\"actual_rig_cases\":"<<cases<<",\"assertions\":"<<checks<<",\"max_stop_drift_m\":["<<worst_drift[0]<<','<<worst_drift[1]<<','<<worst_drift[2]<<"],\"max_stop_seconds\":["<<worst_stop[0]<<','<<worst_stop[1]<<','<<worst_stop[2]<<"],\"strong_anchor_error_m\":"<<worst_anchor<<",\"min_boot_clearance_m\":"<<worst_penetration<<",\"max_sole_speed_mps\":"<<worst_sole_speed<<"}\n";
}
