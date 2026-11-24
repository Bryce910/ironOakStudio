#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
using namespace world_animation;
static unsigned checks=0,frames=0;
static void require(bool yes,const std::string&why){++checks;if(!yes){std::cerr<<"FAIL "<<why<<"\n";std::exit(1);}}
namespace world_animation_render {
Renderer*create(const Asset&,unsigned,std::string&,std::uint64_t){std::abort();}
void destroy(Renderer*p){if(p)std::abort();}
bool warm(Renderer*,unsigned,std::string&){std::abort();}
bool upload(Renderer*,const Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}
bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}
}
static double difference(const Pose&a,const Pose&b){double d=0;require(a.nodes==b.nodes,"same nodes");for(unsigned i=0;i<a.nodes;++i)for(unsigned k=0;k<16;++k)d=std::max(d,std::abs(double(a.global[i].m[k])-b.global[i].m[k]));return d;}
int main(int argc,char**argv){
 require(argc==5,"civilian.cfg armored.cfg new_rig_dir old_rig_dir");
 const char*names[]={"sword","sword_shield","great_axe","staff"};const char*prefix[]={nullptr,"Shield_","Axe_","Staff_"};unsigned cases=0;double max_lower_error=0,max_armed_error=0,min_swing=1e9,max_hand_difference=0,max_relaxed_hand_step=0,max_old_relaxed_hand_step=0,worst_inherited_step=0,worst_run_weight=0;unsigned worst_frame=0,worst_weapon=0,worst_outfit=0;
 for(unsigned outfit=0;outfit<2;++outfit){world_knight_appearance::Config config;Asset body;std::string error;require(world_knight_appearance::load(argv[1+outfit],config,body,error),error);
 for(unsigned weapon=0;weapon<4;++weapon){
  auto next=std::make_unique<WorldSkeletalHero>(),old=std::make_unique<WorldSkeletalHero>();auto&n=*next;auto&o=*old;
  require(n.load_cpu((std::string(argv[3])+"/"+names[weapon]+".glb").c_str(),error,prefix[weapon]),error);require(o.load_cpu((std::string(argv[4])+"/"+names[weapon]+".glb").c_str(),error,prefix[weapon]),error);
  require(n.supports_relaxed_walk()&&!o.supports_relaxed_walk(),"optional source clip selected only on new rig");require(n.select_appearance(body,config.first_person,error)&&o.select_appearance(body,config.first_person,error),error);
  unsigned spine=0,left=0,right=0;const auto&s=n.asset().skeleton;for(unsigned i=0;i<s.joints.size();++i){if(s.joints[i].name=="spine_01")spine=i;if(s.joints[i].name=="hand_l")left=i;if(s.joints[i].name=="hand_r")right=i;}
  std::vector<unsigned>lower;for(unsigned i=0;i<s.joints.size();++i){int a=int(i);while(a>=0&&unsigned(a)!=spine)a=s.joints[unsigned(a)].parent;if(a<0)lower.push_back(i);}
  auto upper_difference=[&](const Pose&a,const Pose&b){double d=0;for(unsigned i=0;i<s.joints.size();++i){int ancestor=int(i);while(ancestor>=0&&unsigned(ancestor)!=spine)ancestor=s.joints[unsigned(ancestor)].parent;if(ancestor<0)continue;const auto&x=a.local[i];const auto&y=b.local[i];for(double v:{double(x.translation.x-y.translation.x),double(x.translation.y-y.translation.y),double(x.translation.z-y.translation.z),double(x.rotation.x-y.rotation.x),double(x.rotation.y-y.rotation.y),double(x.rotation.z-y.rotation.z),double(x.rotation.w-y.rotation.w)})d=std::max(d,std::abs(v));}return d;};
  bool had_relaxed=false;std::array<float,3> previous_hand={},previous_old_hand={};
  WorldTrainingCombat fight;fight.weapon=weapon==0?world_weapon_loadout::Weapon::Sword:weapon==1?world_weapon_loadout::Weapon::SwordShield:weapon==2?world_weapon_loadout::Weapon::GreatAxe:world_weapon_loadout::Weapon::Staff;
  WorldSkeletalHero::Input in;in.dt=1./60.;in.reset=true;
  auto step=[&](bool armed_exact=false){fight.clock+=in.paused?0:in.dt;in.x+=in.dx;in.z+=in.dz;require(n.update_cpu(in,fight,error),error);require(o.update_cpu(in,fight,error),error);++frames;
   if(n.equipment_stowed_settled()&&!n.stats().swimming&&!n.stats().dead&&!n.stats().special_transition){const auto&m=n.pose().global[right].m;const auto&om=o.pose().global[right].m;if(had_relaxed){const double step_m=std::sqrt(std::pow(double(m[12])-previous_hand[0],2)+std::pow(double(m[13])-previous_hand[1],2)+std::pow(double(m[14])-previous_hand[2],2)),inherited_m=std::sqrt(std::pow(double(om[12])-previous_old_hand[0],2)+std::pow(double(om[13])-previous_old_hand[1],2)+std::pow(double(om[14])-previous_old_hand[2],2));if(step_m>max_relaxed_hand_step){max_relaxed_hand_step=step_m;worst_inherited_step=inherited_m;worst_frame=frames;worst_weapon=weapon;worst_outfit=outfit;worst_run_weight=n.stats().run;}max_old_relaxed_hand_step=std::max(max_old_relaxed_hand_step,inherited_m);}previous_hand={m[12],m[13],m[14]};previous_old_hand={om[12],om[13],om[14]};had_relaxed=true;}else had_relaxed=false;
   for(unsigned i=0;i<n.pose().nodes;++i)for(float v:n.pose().global[i].m)require(std::isfinite(v),"finite human pose");
   if(!in.swimming&&!in.dead&&!n.stats().special_transition)for(unsigned i:lower)for(unsigned k=0;k<16;++k)max_lower_error=std::max(max_lower_error,std::abs(double(n.pose().global[i].m[k])-o.pose().global[i].m[k]));
   if(armed_exact){max_armed_error=std::max(max_armed_error,difference(n.pose(),o.pose()));require(n.stats().relaxed_walk_weight==0,"armed does not use free-arm overlay");}
  };
  step(true);in.reset=false;for(unsigned k=0;k<60;++k)step(true);
  // The new source must leave ordinary armed locomotion exactly unchanged.
  for(double speed:{.8,1.6,3.8}){in.dz=speed*in.dt;for(unsigned k=0;k<90;++k)step(true);}in.dz=0;
  for(unsigned k=0;k<120;++k)step(true);
  const auto before=n.pose();in.equipment_stowed=true;step();require(upper_difference(before,n.pose())<1e-6,"stow bridge starts at displayed upper pose while feet remain live");for(unsigned k=0;k<30;++k)step();require(n.equipment_stowed_settled()&&!n.equipment_weapon_visible(),"stowed held gear stays hidden");
  double aa[3],bb[3];require(!n.blade(aa,bb)&&!n.contact_blade(2,.46,in.x,0,in.z,0,aa,bb,error),"stowed rejects physical contacts");
  // Forward, diagonal and running use the same resolved distance phase, with
  // measured hand travel. Their pelvis/legs/foot support remain exact.
  for(unsigned direction=0;direction<3;++direction){const double speed=direction==2?3.8:1.6;in.dx=direction==1?speed*in.dt*.70710678118:0;in.dz=speed*in.dt*(direction==1?.70710678118:1);double low=1e9,high=-1e9;
   for(unsigned k=0;k<150;++k){step();if(k>=45){low=std::min(low,double(n.pose().global[right].m[14]));high=std::max(high,double(n.pose().global[right].m[14]));max_hand_difference=std::max(max_hand_difference,std::abs(double(n.pose().global[right].m[14])-o.pose().global[right].m[14]));}}
   min_swing=std::min(min_swing,high-low);require(n.stats().relaxed_walk_weight>.999,"actual moving body uses source walk");require(high-low>.10,"free right hand has visible captured swing");
  }
  in.dx=in.dz=0;const double cycle=n.stats().gait_cycles;for(unsigned k=0;k<180;++k)step();require(n.stats().gait_cycles==cycle,"blocked root never advances gait");require(n.stats().relaxed_walk_weight==0,"settled idle removes walking overlay");require(difference(n.pose(),o.pose())<1e-5,"existing quiet relaxed idle preserved");
  const auto paused=n.pose();const auto count=n.stats().relaxed_walk_frames;in.paused=true;for(unsigned k=0;k<12;++k)step();require(difference(paused,n.pose())==0&&n.stats().relaxed_walk_frames==count,"pause freezes displayed pose and free-arm clock");in.paused=false;
  const auto stowed=n.pose();in.equipment_stowed=false;step();require(upper_difference(stowed,n.pose())<1e-6,"draw bridge starts at displayed upper pose while feet remain live");for(unsigned k=0;k<30;++k)step();require(n.equipment_combat_ready()&&n.equipment_weapon_visible(),"draw restores gameplay eligibility");require(difference(n.pose(),o.pose())<1e-5,"draw settles at original armed pose");
  // Existing five authored guard and contact directions remain byte-equivalent.
  for(unsigned direction=0;direction<5;++direction){fight.guard=true;fight.selected=TrainingDirection(direction);for(unsigned k=0;k<24;++k)step(true);double oa[3],ob[3];require(n.contact_blade(direction,.46,in.x,0,in.z,0,aa,bb,error)&&o.contact_blade(direction,.46,in.x,0,in.z,0,oa,ob,error),error);for(unsigned c=0;c<3;++c)require(std::abs(aa[c]-oa[c])<1e-7&&std::abs(bb[c]-ob[c])<1e-7,"armed contact geometry exact");}fight.guard=false;
  // This optional walking overlay may never leak into full-body story modes.
  in.equipment_stowed=true;in.swimming=true;step();for(unsigned k=0;k<30;++k)step();require(n.stats().relaxed_walk_weight==0&&difference(n.pose(),o.pose())<1e-5,"swim idle unchanged");in.dz=.6*in.dt;for(unsigned k=0;k<30;++k)step();in.submerged=true;for(unsigned k=0;k<30;++k)step();require(n.stats().relaxed_walk_weight==0&&difference(n.pose(),o.pose())<1e-5&&!n.equipment_weapon_visible(),"dive full body unchanged");
  in.swimming=in.submerged=false;in.dz=0;for(unsigned k=0;k<45;++k)step();in.dead=true;for(unsigned k=0;k<210;++k)step();require(n.stats().dead&&difference(n.pose(),o.pose())<1e-5,"death unchanged");in.dead=false;in.reset=true;step();in.reset=false;for(unsigned k=0;k<45;++k)step();require(!n.stats().dead&&difference(n.pose(),o.pose())<1e-5,"reset returns to quiet original idle");require(n.stats().relaxed_walk_frames>300,"real free-arm frames counted");++cases;
 }}
 require(max_lower_error<1e-6,"all accepted lower-body poses remain exact");require(max_armed_error<1e-6,"all armed animation poses remain exact");require(max_hand_difference>.10,"walking no longer pins hands to relaxed idle");require(max_relaxed_hand_step<=max_old_relaxed_hand_step+.00001,"free arms add no hand-step pop beyond the unchanged inherited pelvis gait: "+std::to_string(max_relaxed_hand_step)+" inherited_baseline="+std::to_string(max_old_relaxed_hand_step));
 std::cout<<"{\"passed\":true,\"gpu_used\":false,\"outfit_weapon_cases\":"<<cases<<",\"frames\":"<<frames<<",\"assertions\":"<<checks<<",\"max_lower_body_error_m\":"<<max_lower_error<<",\"max_armed_pose_component_error\":"<<max_armed_error<<",\"minimum_measured_right_hand_swing_m\":"<<min_swing<<",\"max_right_hand_change_from_old_idle_m\":"<<max_hand_difference<<",\"max_relaxed_hand_step_m\":"<<max_relaxed_hand_step<<",\"max_inherited_relaxed_hand_step_m\":"<<max_old_relaxed_hand_step<<",\"worst_frame\":"<<worst_frame<<",\"worst_weapon_index\":"<<worst_weapon<<",\"worst_outfit_index\":"<<worst_outfit<<",\"worst_run_weight\":"<<worst_run_weight<<",\"inherited_hand_step_at_worst_frame_m\":"<<worst_inherited_step<<"}\n";
}
