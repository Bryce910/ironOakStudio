#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_skeletal_hero.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
static void ck(bool good,const char*m){++checks;if(!good)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("usage: test_quick_step rig_directory");
 const char*files[]={"sword","sword_shield","great_axe","staff"};const char*prefix[]={"","Shield_","Axe_","Staff_"};unsigned cases=0;double max_grip=0,max_residual=0,min_step=100,max_step=0;
 for(unsigned w=0;w<4;++w)for(unsigned fps:{30u,60u,120u}){
  WorldSkeletalHero hero;std::string error;std::string path=std::string(argv[1])+"/"+files[w]+".glb";ck(hero.load_cpu(path.c_str(),error,prefix[w]),"Load owned rig");
  WorldTrainingCombat combat;combat.weapon=world_weapon_loadout::Weapon(w?w+1:0);combat.sword_high_windup_min_seconds=.36;combat.start_shared_arena();combat.impact_pause_enabled=false;TrainingEvents events;
  for(unsigned i=0;i<120;++i)combat.update_shared(1./60.,nullptr,0,events);
  WorldSkeletalHero::Input input;input.dt=1./fps;for(unsigned i=0;i<60;++i){input.reset=!i;ck(hero.update_cpu(input,combat,error),"Initial CPU pose");}input.reset=false;
  ck(combat.dodge(),"Ordinary quick-step accepted");double distance=0;unsigned dodge_frames=0;
  for(unsigned frame=0;frame<fps*2;++frame){
   const double dz=combat.dodge_time>0?-4*std::sin(3.141592653589793*combat.dodge_time/.26)/fps:0;distance-=dz;input.dz=dz;input.z+=dz;
   if(combat.dodge_time>0){++dodge_frames;ck(!combat.attack(),"Dodge cannot buffer an attack through invulnerability");}
   combat.update_shared(1./fps,nullptr,0,events);ck(hero.update_cpu(input,combat,error),"Owned rig remains valid during step/settle");
   for(unsigned n=0;n<hero.pose().nodes;++n)for(float f:hero.pose().global[n].m)ck(std::isfinite(f),"Finite posed skeleton through quick-step");
   const auto&s=hero.stats();max_grip=std::max(max_grip,double(s.maximum_grip_error_m));for(float f:s.foot_residual_m)max_residual=std::max(max_residual,double(f));
  }
  ck(dodge_frames>0&&distance>.60&&distance<.70&&combat.dodges==1,"Actual bounded backstep travelled and recovered once");min_step=std::min(min_step,distance);max_step=std::max(max_step,distance);
  combat.choose(TrainingDirection::Left);ck(combat.attack(),"Fresh attack accepted after backstep");input.dz=0;
  for(unsigned i=0;i<fps*2;++i){combat.update_shared(1./fps,nullptr,0,events);ck(hero.update_cpu(input,combat,error),"Step-to-cut pose continuity");}
  ck(combat.attack_start_serial==1&&combat.player_phase==TrainingPlayerPhase::Ready,"No held replay after step and fresh swing");++cases;
 }
 ck(max_grip<.0001&&max_residual<.001,"Grip and planted support remain bounded");
 std::cout<<"PASS "<<cases<<" owned-rig quick-step/fresh-attack cases, "<<checks<<" checks; travel_m="<<min_step<<".."<<max_step<<" grip_m="<<max_grip<<" support_m="<<max_residual<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
