#include "engine/world_training_combat.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
static void check(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);}
struct Query {bool steel=false;};
static bool contact(const TrainingAnatomicalQuery&q,TrainingAnatomicalHit&h,void*ctx){h={};h.valid=true;h.weapon_contact=!q.incoming&&static_cast<Query*>(ctx)->steel;h.part=h.weapon_contact?TrainingBodyPart::None:TrainingBodyPart::Torso;h.sweep_t=0;return true;}
int main(){try{
 unsigned cases=0;
 for(unsigned difficulty=0;difficulty<3;++difficulty)for(unsigned fps:{30u,60u,120u})for(unsigned dir=0;dir<5;++dir)for(unsigned mode=0;mode<3;++mode){
  WorldTrainingCombat f;f.weapon=world_weapon_loadout::Weapon::Staff;f.difficulty=world_combat_difficulty::Difficulty(difficulty);f.start_shared_arena();f.impact_pause_enabled=false;
  TrainingEvents ev;for(unsigned i=0;i<120;++i)f.update_shared(1./60.,nullptr,0,ev);
  TrainingOpponent o;o.phase=mode==2?TrainingEnemyPhase::Stagger:TrainingEnemyPhase::Windup;o.time=mode==2?.12:.05;o.duration=.40;o.attack_windup_seconds=.4;o.direction=TrainingDirection::High;
  TrainingTarget target;target.opponent=&o;target.id=0;target.distance=1.4;target.in_player_arc=true;
  Query q;q.steel=mode==1;f.contact_query=contact;f.contact_query_context=&q;f.choose(TrainingDirection(dir));check(f.attack(),"Ordinary staff commit");
  for(unsigned i=0;i<60&&!f.resolved_attack_serial;++i)f.update_shared(1./fps,&target,1,ev);
  check(f.resolved_attack_serial==f.attack_start_serial,"One committed physical contact resolved");
  if(mode==1){check(f.resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock&&o.health==90,"Steel stays steel: no blunt damage/stagger");check(o.phase!=TrainingEnemyPhase::Stagger,"Steel does not create a body flinch");}
  else{
   check(f.resolved_attack_outcome==TrainingAttackOutcome::Body&&o.health<90&&o.health>0,"Blunt body contact damages without a sever");check(o.phase==TrainingEnemyPhase::Stagger,"Physical body interruption owns reaction");check(!o.severed_parts,"Staff cannot sever");
   const double remaining=o.duration-o.time;
   if(mode==2)check(remaining<.18,"Repeat contact does not restart the existing stagger");
   else{check(remaining>.35&&remaining<=.40,"Brief fresh stagger has bounded duration");const unsigned old_hits=f.enemy_hits;
    for(unsigned i=0;i<unsigned(.32*fps);++i){f.update_shared(1./fps,&target,1,ev);check(f.enemy_hits==old_hits,"Interrupted old attack cannot damage through visible reaction");}
   }
   for(unsigned i=0;i<fps*2;++i)f.update_shared(1./fps,&target,1,ev);
   check(o.phase!=TrainingEnemyPhase::Stagger,"Opponent returns to live AI after recovery");
  }
  ++cases;
 }
 std::cout<<"PASS "<<cases<<" body/steel/repeated-hit cases, "<<checks<<" checks at 30/60/120 Hz across all difficulties\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
