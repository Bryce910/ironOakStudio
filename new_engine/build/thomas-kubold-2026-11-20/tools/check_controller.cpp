#include "engine/world_skeletal_hero.h"
#include "engine/world_knight_appearance.h"
#include "engine/world_skeletal_hero_viewmodel.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

// This CPU rehearsal must never allocate or exercise an alternate renderer.
namespace world_animation_render {
Renderer *create(const world_animation::Asset&,unsigned,std::string&,std::uint64_t){std::abort();}
void destroy(Renderer *p){if(p)std::abort();}
bool warm(Renderer*,unsigned,std::string&){std::abort();}
bool upload(Renderer*,const world_animation::Pose*,const Instance*,std::size_t,const Bindings&,std::string&){std::abort();}
bool draw(Renderer*,unsigned,const Bindings&,std::string&){std::abort();}
}
unsigned checks=0,frames=0;
void need(bool ok,const std::string&message){++checks;if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
bool ground(double,double,double,WorldSkeletalHero::Support&support,void*){support={};return true;}
int main(int argc,char **argv){
 need(argc==2||argc==3,"usage: check_controller RIG_DIRECTORY [APPEARANCE.cfg]");
 const char *names[]={"sword","sword_shield","great_axe","staff"};
 const char *prefixes[]={nullptr,"Shield_","Axe_","Staff_"};
 std::cout<<"{\"schema\":\"valwood.thomas-kubold.cpu-controller.v1\",\"gpu_used\":false,\"cases\":[";
 for(unsigned weapon=0;weapon<4;++weapon){
  auto holder=std::make_unique<WorldSkeletalHero>();auto &hero=*holder;std::string error;
  const bool loaded=hero.load_cpu((std::string(argv[1])+"/"+names[weapon]+".glb").c_str(),error,prefixes[weapon]);
  need(loaded,"rig load "+std::string(names[weapon])+": "+error);
  need(hero.supports_full_unarmed_locomotion()&&hero.supports_full_unarmed_run()&&hero.supports_equipment()&&hero.supports_swimming()&&hero.supports_death(),"retained complete story lifecycle");
  unsigned fp_vertices=0,fp_generated_primitives=0;
  if(argc==3){
   world_knight_appearance::Config config;world_animation::Asset body;
   const bool body_loaded=world_knight_appearance::load(argv[2],config,body,error);
   need(body_loaded,"appearance load: "+error);
   const bool composed=hero.select_appearance(body,config.first_person,error);
   need(composed,"appearance composition / sole calibration for "+std::string(names[weapon])+": "+error);
   auto fp=world_skeletal_hero_viewmodel::arms_subset(hero.asset(),hero.first_person_primitives(),hero.first_person_body_parts());
   need(world_animation::validate(fp,error)&&!fp.mesh.vertices.empty(),"actual first-person arms subset: "+error);
   fp_vertices=fp.mesh.vertices.size();
   fp_generated_primitives=fp.mesh.primitives.size()-hero.first_person_primitives().size();
  }
  WorldTrainingCombat fight;
  fight.weapon=weapon==0?world_weapon_loadout::Weapon::Sword:weapon==1?world_weapon_loadout::Weapon::SwordShield:weapon==2?world_weapon_loadout::Weapon::GreatAxe:world_weapon_loadout::Weapon::Staff;
  WorldSkeletalHero::Input in;in.dt=1./60;in.equipment_stowed=true;in.reset=true;in.ground_query=ground;
  auto step=[&](unsigned count){for(unsigned frame=0;frame<count;++frame){in.x+=in.dx;in.z+=in.dz;fight.clock+=in.dt;
   need(hero.update_cpu(in,fight,error),"actual CPU update: "+error);in.reset=false;++frames;
   for(unsigned n=0;n<hero.pose().nodes;++n)for(float value:hero.pose().global[n].m)need(std::isfinite(value),"finite final pose");
  }};
  step(120);need(hero.full_unarmed_settled()&&hero.stats().full_unarmed,"settled captured idle");
  need(!hero.equipment_weapon_visible()&&!hero.equipment_combat_ready(),"stowed excludes held weapon and damage");
  const double walk_stride=hero.stats().unarmed_walk_stride_m,run_stride=hero.stats().unarmed_run_stride_m;
  need(walk_stride>.1&&run_stride>.1&&std::isfinite(walk_stride)&&std::isfinite(run_stride),"finite actual sole-calibrated gait strides");
  in.dz=1.5*in.dt;step(180);need(hero.stats().full_unarmed_walk_frames>30,"resolved travel uses captured walk");
  in.dx=2.8*in.dt;in.dz=2.8*in.dt;step(180);need(hero.stats().full_unarmed_run_frames>30,"resolved diagonal travel uses captured run");
  in.dx=in.dz=0;step(120);const double stopped=hero.stats().full_unarmed_cycles;step(60);
  need(hero.stats().full_unarmed_cycles==stopped,"stopped character does not advance gait");
  in.equipment_stowed=false;step(90);need(hero.equipment_combat_ready()&&hero.equipment_weapon_visible(),"draw restores held weapon / contact");
  in.equipment_stowed=true;step(90);need(hero.full_unarmed_settled(),"ordinary stow restores captured body");
  in.swimming=true;step(60);need(hero.stats().swimming&&!hero.stats().full_unarmed,"swimming owns whole body");
  in.submerged=true;step(40);need(hero.stats().submerged,"submerged body remains valid");
  in.swimming=in.submerged=false;step(90);in.dead=true;step(190);need(hero.stats().dead&&!hero.stats().full_unarmed,"defeat owns whole body");
  in.dead=false;in.reset=true;step(120);need(hero.full_unarmed_settled(),"retry restores captured idle");
  if(weapon)std::cout<<',';
  std::cout<<"{\"weapon\":\""<<names[weapon]<<"\",\"walk_stride_m\":"<<walk_stride<<",\"run_stride_m\":"<<run_stride<<",\"first_person_vertices\":"<<fp_vertices<<",\"first_person_generated_primitives\":"<<fp_generated_primitives<<",\"updates\":"<<hero.stats().updates<<'}';
 }
 std::cout<<"],\"frames\":"<<frames<<",\"assertions\":"<<checks<<",\"appearance_tested\":"<<(argc==3?"true":"false")<<",\"passed\":true}\n";
}
