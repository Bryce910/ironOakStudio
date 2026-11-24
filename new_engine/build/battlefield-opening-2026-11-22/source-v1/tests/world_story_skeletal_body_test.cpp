#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
using namespace world_animation;
static unsigned checks=0;
static void require(bool value,const std::string &message){++checks;if(!value){std::cerr<<"FAIL "<<message<<"\n";std::exit(1);}}
static double difference(const Pose&a,const Pose&b){double d=0;require(a.nodes==b.nodes,"pose nodes");for(unsigned n=0;n<a.nodes;++n)for(unsigned k=0;k<16;++k)d=std::max(d,std::abs(double(a.global[n].m[k])-b.global[n].m[k]));return d;}
// Deliberately CPU-only. Unexpected GPU creation/draw/upload fails this test;
// destroy(nullptr) is the normal CPU controller destructor and is permitted.
namespace world_animation_render {
Renderer *create(const Asset&,unsigned,std::string&,std::uint64_t){std::abort();}
void destroy(Renderer *p){if(p)std::abort();}
bool warm(Renderer*,unsigned,std::string&){std::abort();}
bool upload(Renderer*,const Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}
bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}
}
int main(int argc,char**argv){
 require(argc==7,"civilian.cfg armored.cfg sword.glb shield.glb axe.glb staff.glb");
 const char*prefix[]={nullptr,"Shield_","Axe_","Staff_"};unsigned cases=0,pose_samples=0;
 for(unsigned outfit=0;outfit<2;++outfit){world_knight_appearance::Config config;Asset body;std::string error;require(world_knight_appearance::load(argv[1+outfit],config,body,error),error);
  for(unsigned weapon=0;weapon<4;++weapon){
   WorldSkeletalHero h;require(h.load_cpu(argv[3+weapon],error,prefix[weapon]),error);require(h.supports_swimming()&&h.supports_death(),"dedicated clips supported");require(h.select_appearance(body,config.first_person,error),error);
   WorldTrainingCombat fight;fight.weapon=weapon==0?world_weapon_loadout::Weapon::Sword:weapon==1?world_weapon_loadout::Weapon::SwordShield:weapon==2?world_weapon_loadout::Weapon::GreatAxe:world_weapon_loadout::Weapon::Staff;
   WorldSkeletalHero::Input in;in.dt=1./60.;in.reset=true;in.equipment_stowed=true;require(h.update_cpu(in,fight,error),error);in.reset=false;
   auto step=[&](){fight.clock+=in.paused?0:in.dt;require(h.update_cpu(in,fight,error),error);++pose_samples;for(unsigned n=0;n<h.pose().nodes;++n)for(float f:h.pose().global[n].m)require(std::isfinite(f),"finite special-body pose");};
   const auto grounded=h.pose();in.swimming=true;in.equipment_stowed=false;step();require(difference(grounded,h.pose())<1e-6,"water entry starts at displayed body");require(!h.equipment_weapon_visible()&&!h.equipment_combat_ready(),"held gear hidden and contacts disabled during entry");
   for(unsigned n=0;n<30;++n)step();require(h.stats().swimming&&!h.stats().special_transition&&!h.stats().foot_planted[0]&&!h.stats().foot_planted[1],"swim full body does not use ground support");
   double aa[3],bb[3];require(!h.blade(aa,bb),"swimming has no physical blade");
   const auto paused=h.pose();in.paused=true;for(unsigned n=0;n<12;++n)step();require(difference(paused,h.pose())==0,"true pause freezes displayed swim pose");in.paused=false;
   const auto idle=h.pose();in.dx=.7*in.dt;step();require(difference(idle,h.pose())<1e-6,"stroke change begins at displayed pose");for(unsigned n=0;n<30;++n)step();require(h.stats().special_mode==unsigned(world_story_body_pose::Mode::SwimForward),"actual forward swim selected");
   in.submerged=true;step();for(unsigned n=0;n<30;++n)step();require(h.stats().submerged&&h.stats().submerged_frames>0&&!h.equipment_weapon_visible(),"underwater keeps selected body and hidden gear");
   in.swimming=in.submerged=false;in.dx=0;const auto water=h.pose();step();require(difference(water,h.pose())<1e-6,"shore exit begins at displayed pose");
   std::vector<world_combat_anatomy::Capsule> capsules;std::vector<std::array<double,9>> triangles;
   require(!h.contact_blade(2,.36,0,0,0,0,aa,bb,error),"hidden exit bridge rejects attack contact");
   require(!h.guard_blade(2,0,0,0,0,0,aa,bb,error),"hidden exit bridge rejects guard contact");
   require(!h.guard_shield(2,0,0,0,0,0,triangles,error),"hidden exit bridge rejects shield contact");
   require(!h.guard_target_preview(2,nullptr,0,0,0,0,aa,bb,capsules,error),"hidden exit bridge rejects guard preview");
   require(!h.body_capsules(0,0,0,0,capsules,error,2,0),"hidden exit bridge rejects speculative guard anatomy");
   for(unsigned n=0;n<30;++n)step();require(!h.stats().swimming&&!h.stats().special_transition,"shore exit returns to ground");require(h.equipment_weapon_visible()&&h.equipment_combat_ready(),"settled shore exit restores equipped weapon");
   const auto standing=h.pose();in.dead=true;fight.state=TrainingState::Lost;fight.health=0;step();require(difference(standing,h.pose())<1e-6,"death begins at displayed body");for(unsigned n=0;n<240;++n)step();const auto terminal=h.pose();for(unsigned n=0;n<6;++n)step();require(h.stats().dead&&difference(terminal,h.pose())==0&&!h.equipment_weapon_visible(),"death endpoint holds on selected body with no held weapon");
   in.dead=false;in.reset=true;in.equipment_stowed=true;fight=WorldTrainingCombat{};step();require(difference(terminal,h.pose())<1e-6,"retry bridge begins at displayed corpse pose");in.reset=false;for(unsigned n=0;n<30;++n)step();require(!h.stats().dead&&!h.stats().special_transition,"retry releases corpse to normal pose");
   require(h.stats().swim_frames>60&&h.stats().death_frames>240&&h.stats().special_entries==2&&h.stats().special_exits==2,"special lifecycle counters");++cases;
  }
 }
 std::cout<<"{\"passed\":true,\"gpu_used\":false,\"outfit_weapon_cases\":"<<cases<<",\"pose_samples\":"<<pose_samples<<",\"assertions\":"<<checks<<"}\n";
}
