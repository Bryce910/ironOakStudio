#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "engine/world_skeletal_hero.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using Clock=std::chrono::steady_clock;
// Controller isolation only: a deterministic steel-contact fixture drives the
// real combat latch. Native acceptance separately requires owned NPC geometry.
static bool steel(const TrainingAnatomicalQuery&q,TrainingAnatomicalHit&h,void*){
 if(q.incoming)return false;h={};h.valid=true;h.weapon_contact=true;h.sweep_t=0;h.attack_phase=q.phase_begin;return true;
}
int main(int argc,char**argv){try{
 if(argc!=4)throw std::runtime_error("usage: audit_steel rig_directory fps output.csv");
 const unsigned fps=std::stoul(argv[2]);const double dt=1./fps;std::ofstream out(argv[3]);out<<std::setprecision(12)<<"weapon,direction,frame,phase,recoil,source_phase,contact_phase,retreat_phase,base_x,base_y,base_z,tip_x,tip_y,tip_z,grip_error,update_ms,ready\n";
 const char*files[]={"sword","sword_shield","great_axe","staff"};const char*prefixes[]={"","Shield_","Axe_","Staff_"};
 for(unsigned w=0;w<4;++w)for(unsigned d=0;d<5;++d){
  WorldSkeletalHero hero;std::string e;const std::string rig=std::string(argv[1])+"/"+files[w]+".glb";
  if(!hero.load_cpu(rig.c_str(),e,prefixes[w]))throw std::runtime_error(e);
  WorldTrainingCombat f;f.weapon=world_weapon_loadout::Weapon(w?w+1:0);f.sword_high_windup_min_seconds=.36;f.start_shared_arena();f.impact_pause_enabled=false;
  for(unsigned i=0;i<120;++i){TrainingEvents ev;f.update_shared(1./60.,nullptr,0,ev);}
  WorldSkeletalHero::Input input;input.dt=dt;
  for(unsigned i=0;i<60;++i){input.reset=!i;if(!hero.update_cpu(input,f,e))throw std::runtime_error(e);}input.reset=false;
  TrainingOpponent enemy;enemy.guard=TrainingDirection(d);TrainingTarget target;target.opponent=&enemy;target.distance=1.2;target.advance_ai=false;
  f.contact_query=steel;f.choose(TrainingDirection(d));if(!f.attack())throw std::runtime_error("Attack rejected");
  for(unsigned i=0;i<fps*2;++i){TrainingEvents ev;f.update_shared(dt,&target,1,ev);const auto start=Clock::now();if(!hero.update_cpu(input,f,e))throw std::runtime_error(e);const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   double a[3],b[3];if(!hero.blade(a,b))throw std::runtime_error("No blade");const auto&s=hero.stats();
   out<<files[w]<<','<<d<<','<<i<<','<<unsigned(f.player_phase)<<','<<s.steel_recoil<<','<<s.steel_source_phase<<','<<s.steel_contact_phase<<','<<s.steel_retreat_phase;
   for(double v:a)out<<','<<v;for(double v:b)out<<','<<v;out<<','<<s.maximum_grip_error_m<<','<<ms<<','<<(f.player_phase==TrainingPlayerPhase::Ready)<<'\n';
  }
  if(hero.stats().steel_rebounds!=1||f.resolved_attack_outcome!=TrainingAttackOutcome::EnemyBlock)throw std::runtime_error("Steel latch/recovery failed");
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
