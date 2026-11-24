#include "engine/world_arena_combat.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
using namespace world_arena_combat;
static std::size_t allocations=0;
void *operator new(std::size_t n){++allocations;if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete(void *p,std::size_t)noexcept{std::free(p);}
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static void tick(Arena &a,WorldTrainingCombat &f,Vec3 p,double dt=1./120.,bool paused=false){a.update(dt,f,p,paused);}
static bool ready(Arena &a,WorldTrainingCombat &f,Vec3 p,unsigned count=1){for(unsigned n=0;n<1200;++n){if(a.active_count()==count&&f.state==TrainingState::Fighting)return true;tick(a,f,p);}return false;}
static unsigned flying(const Arena &a){unsigned n=0;for(const auto &v:a.arrows())n+=v.state==ArrowState::Flying;return n;}
int main(){
 Arena a;WorldTrainingCombat f;auto p=a.player_spawn();
 CHECK(a.state()==State::Idle&&a.config().total==8&&a.config().simultaneous==1);
 CHECK(!valid({0,1})&&!valid({9,1})&&!valid({8,0})&&!valid({8,9})&&!valid({1,2}));
 CHECK(!a.start(f,{0,1})&&a.state()==State::Idle&&f.state==TrainingState::Idle);
 // Every supported pair starts exactly its participants, then enters truly
 // concurrent independently phased opponents. Hidden backing slots are inert.
 unsigned configurations=0;
 for(unsigned total=1;total<=8;++total)for(unsigned concurrent=1;concurrent<=total;++concurrent){
  CHECK(a.start(f,{total,concurrent},true));++configurations;
  CHECK(a.entering_count()==concurrent&&a.active_count()==0&&a.defeated()==0);
  CHECK(f.shared_arena&&f.health==100&&f.stamina==100);
  for(unsigned id=0;id<8;++id){CHECK((id<total)==(a.opponent(id)!=nullptr));CHECK((a.challengers()[id].state==ChallengerState::Unused)==(id>=total));}
  unsigned peak=0;
  for(unsigned n=0;n<1200&&peak<concurrent&&a.state()!=State::Lost;++n){
   tick(a,f,p);peak=std::max(peak,a.active_count());
   CHECK(a.active_count()+a.entering_count()<=concurrent);
   CHECK(a.defeated()==0&&f.health<=100&&f.stamina<=100&&f.stamina>=0);
   for(const auto &v:a.challengers())CHECK(a.bounds.contains(v.position,.1));
  }
  CHECK(peak==concurrent);
  for(unsigned i=0;i<total;++i)for(unsigned j=0;j<i;++j){
   const auto &x=a.challengers()[i],&y=a.challengers()[j];
   if(x.state==ChallengerState::Active&&y.state==ChallengerState::Active)CHECK(std::hypot(x.position.x-y.position.x,x.position.z-y.position.z)>.70);
  }
 }
 CHECK(configurations==36);
 // Actual eight sequential duels: no HP/stamina restoration at challenger
 // handoff, each enemy genuinely defeated by ordinary guard/attack actions.
 CHECK(a.start(f,{8,1}));bool took_damage=false;unsigned last_defeated=0;
 float health_at_win=100;std::uint64_t last_serial=0;unsigned body_events=0,defeat_events=0;
 for(unsigned n=0;n<120*240&&a.state()!=State::Won&&a.state()!=State::Lost;++n){
  if(f.state==TrainingState::Fighting&&a.active_count()){
   const auto *o=a.opponent(unsigned(a.current_index()));
   if(!took_damage){if(f.enemy_hits){took_damage=true;CHECK(f.health<100);}}
   else if(o->phase==TrainingEnemyPhase::Windup){f.choose(o->direction);f.set_guard(o->duration-o->time<.17);}
   else{f.set_guard(false);if(f.player_phase==TrainingPlayerPhase::Ready&&f.stamina>=35){f.choose(TrainingDirection((unsigned(o->guard)+1)%5));f.attack();}}
  }
  tick(a,f,p);
  for(unsigned e=0;e<a.events().count;++e){const auto &v=a.events().values[e];CHECK(v.serial>last_serial);last_serial=v.serial;
   if(v.kind==TrainingContactKind::PlayerBody){++body_events;CHECK(v.opponent_id<8&&v.damage>0);}
   if(v.kind==TrainingContactKind::EnemyDefeated){++defeat_events;CHECK(v.opponent_id<8);}
  }
  CHECK(a.events().overflow==0&&a.active_count()<=1&&a.entering_count()<=1);
  if(a.defeated()!=last_defeated){CHECK(a.defeated()==last_defeated+1);health_at_win=f.health;last_defeated=a.defeated();}
  if(a.entering_count()&&last_defeated)CHECK(f.health==health_at_win);
 }
 CHECK(took_damage&&a.state()==State::Won&&a.defeated()==8&&f.health>0&&f.health<100&&defeat_events==8&&body_events>=24);
 CHECK(f.state==TrainingState::Won&&std::strstr(f.feedback,"all challengers"));
 // Corpse pose/location ages only unpaused, no duplicate completion after win.
 const auto ended=f.health;const auto serial_after=f.contact_serial;
 for(unsigned i=0;i<240;++i)tick(a,f,p);
 CHECK(f.health==ended&&f.contact_serial==serial_after&&a.defeated()==8&&f.impact_pause==0);
 CHECK(a.challengers()[7].defeat_age>1.8);
 Arena corpses;corpses.keep_defeated=true;WorldTrainingCombat c;CHECK(corpses.start(c,{2,1}));
 CHECK(ready(corpses,c,p));
 for(unsigned n=0;n<120*90&&corpses.defeated()==0&&corpses.state()!=State::Lost;++n){
  const auto *o=corpses.opponent(unsigned(corpses.current_index()));
  if(o->phase==TrainingEnemyPhase::Windup){c.choose(o->direction);c.set_guard(o->duration-o->time<.17);}
  else{c.set_guard(false);if(c.player_phase==TrainingPlayerPhase::Ready&&c.stamina>=35){c.choose(TrainingDirection((unsigned(o->guard)+1)%5));c.attack();}}
  tick(corpses,c,p);
 }
 CHECK(corpses.defeated()==1);const auto fallen=corpses.challengers()[0];
 for(unsigned i=0;i<240;++i)tick(corpses,c,p,1./120.,true);
 CHECK(corpses.challengers()[0].defeat_age==fallen.defeat_age);
 for(unsigned i=0;i<300;++i)tick(corpses,c,p);
 const auto after=corpses.challengers()[0];CHECK(after.position.x==fallen.position.x&&after.position.z==fallen.position.z&&after.defeat_yaw==fallen.defeat_yaw&&after.defeat_age>2.3);
 CHECK(corpses.opponent(0)->health==0&&corpses.view(c,0).state==TrainingState::Won&&corpses.active_count()+corpses.entering_count()==1);
 // Maximum concurrency has genuinely separate AI/attacks, not a disguised
 // sequential queue. Eight unguarded attackers cause actual shared HP defeat.
 CHECK(a.start(f,{8,8}));CHECK(ready(a,f,p,8));
 unsigned different=0;const auto *first=a.opponent(0);for(unsigned i=1;i<8;++i)different+=a.opponent(i)->duration!=first->duration||a.opponent(i)->time!=first->time;
 CHECK(different>=4);
 for(unsigned n=0;n<120*30&&a.state()!=State::Lost;++n)tick(a,f,p);
 CHECK(a.state()==State::Lost&&f.health==0&&f.enemy_hits>=6);
 unsigned attackers=0;for(unsigned i=0;i<8;++i)attackers+=a.opponent(i)->hits_dealt>0;CHECK(attackers>=3);
 CHECK(a.start(f,{3,3},true));CHECK(ready(a,f,p,3));
 // Real ballistic shot to an active non-focus actor. Its exact swept point
 // and ID are delivered in the same-frame event, independent of focus/HUD.
 const unsigned victim=(unsigned(a.current_index())+1)%3;
 a.set_weapon(Weapon::Bow);f.clear_buffer();CHECK(a.begin_draw(f));
 for(unsigned i=0;i<36;++i)tick(a,f,p);
 const auto target=a.challengers()[victim].position+Vec3{0,1.15,0};const Vec3 origin=p+Vec3{0,1.4,0};
 const float old_hp=a.opponent(victim)->health;
 CHECK(a.release_draw(f,origin,target-origin)&&a.shots()==1&&flying(a)==1);
 bool exact_contact=false;
 for(unsigned n=0;n<120&&a.arrow_hits()==0;++n){tick(a,f,p);for(unsigned e=0;e<a.events().count;++e){const auto &v=a.events().values[e];if(v.kind==TrainingContactKind::PlayerBody&&v.projectile){
  CHECK(v.opponent_id==victim);for(const auto &arrow:a.arrows())if(arrow.state==ArrowState::HitEnemy)exact_contact|=arrow.opponent_id==victim&&std::abs(v.point[0]-arrow.position.x)<1e-12&&std::abs(v.point[1]-arrow.position.y)<1e-12&&std::abs(v.point[2]-arrow.position.z)<1e-12;
 }}}
 CHECK(a.arrow_hits()==1&&a.opponent(victim)->health<old_hp&&exact_contact);
 // Paused draw is cancelled and player clock/enemies freeze. Handoff restart
 // resets per-enemy health, arrows, buffers and corpses without story state.
 CHECK(a.begin_draw(f));const auto clock=f.clock;const auto where=a.challengers()[0].position;
 tick(a,f,p,.2,true);CHECK(!a.drawing()&&f.clock==clock&&a.challengers()[0].position.x==where.x&&a.challengers()[0].position.z==where.z);
 CHECK(a.start(f,{8,1}));CHECK(ready(a,f,p));a.set_weapon(Weapon::Bow);CHECK(a.begin_draw(f));
 for(unsigned i=0;i<36;++i)tick(a,f,p);
 CHECK(a.release_draw(f,{-8.4,1.5,-5},{0,.15,1}));
 for(unsigned i=0;i<150;++i)tick(a,f,p);
 CHECK(a.arrow_hits()==0&&a.wall_hits()==1&&a.opponent(1)->health==90);
 CHECK(a.begin_draw(f));for(unsigned i=0;i<30;++i)tick(a,f,p);CHECK(a.release_draw(f,origin,{0,-1,0}));
 for(unsigned i=0;i<60;++i)tick(a,f,p);
 CHECK(a.ground_hits()==1&&a.arrow_hits()==0);
 CHECK(a.begin_draw(f));for(unsigned i=0;i<30;++i)tick(a,f,p);CHECK(a.release_draw(f,origin,{0,1,0}));
 for(unsigned i=0;i<510;++i)tick(a,f,p);
 CHECK(flying(a)==0&&a.arrow_hits()==0);
 a.cancel(f);CHECK(a.state()==State::Idle&&flying(a)==0&&!f.pending_attack);
 const auto invalid_clock=f.clock;a.update(NAN,f,p);a.update(.1,f,{NAN,0,0});CHECK(f.clock==invalid_clock);
 CHECK(swept_capsule({0,1,-2},{0,1,2},{0,0,0})<.5);
 CHECK(!std::isfinite(swept_capsule({2,1,-2},{2,1,2},{0,0,0})));
 auto limited=a.bounds.constrain_player({99,0,-99});CHECK(a.bounds.contains(limited)&&limited.x<=a.bounds.half_extent-.38);
 CHECK(a.start(f,{8,8},true));const auto allocations_before=allocations;
 for(unsigned i=0;i<10000;++i)tick(a,f,p);
 CHECK(allocations==allocations_before);
 std::cout<<"world_arena_combat: "<<checks<<" checks passed; all36 config pairs, true8-way attacks/sharedHP,8 real sequential wins, carry/corpses/pause, moving physical non-focus arrow + exact event, immunity/ground/wall/TTL/bounds\n";
}
