#include "engine/world_training_combat.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
using D=TrainingDirection;using S=TrainingState;using E=TrainingEnemyPhase;using P=TrainingPlayerPhase;
static void tick(WorldTrainingCombat&c,double seconds,double distance=2.2){for(int i=0;i<int(std::ceil(seconds*120));i++)c.update(1./120.,c.enemy_x-distance,c.enemy_z);}
static WorldTrainingCombat ready(){WorldTrainingCombat c;assert(c.start(c.instructor_x,25.75,c.instructor_z));tick(c,3.1);assert(c.state==S::Fighting);return c;}
int main(){
 WorldTrainingCombat c;assert(!c.start(0,25.75,0)&&!c.start(c.instructor_x,99,c.instructor_z)&&!c.start(c.instructor_x,25.75,c.instructor_z,true));
 assert(c.start(c.instructor_x,25.75,c.instructor_z));const auto h=c.health;c.update(100,c.start_x,c.start_z,true);assert(c.health==h&&c.clock==0);c.update(std::numeric_limits<double>::quiet_NaN(),0,0);assert(c.clock==0);
 c.leave();assert(c.state==S::Cancelled);c=ready();
 for(unsigned d=0;d<5;d++){
  auto a=ready();a.enemy_phase=E::Recovery;a.enemy_duration=10;a.choose(D(d));assert(a.attack());tick(a,.46);assert(a.hits==1&&a.enemy_health==33&&a.direction_mask==(1u<<d));assert(a.stamina<100&&a.stamina>=0);
 }
 // Misses use real distance, not a successful button sequence.
 c=ready();c.enemy_phase=E::Recovery;c.enemy_duration=10;assert(c.attack());tick(c,.5,5);assert(c.hits==0&&c.misses==1);
 for(unsigned d=0;d<5;d++){
  c=ready();c.enemy_phase=E::Windup;c.enemy_time=0;c.enemy_duration=.12;c.enemy_direction=D(d);c.choose(D(d));c.set_guard(true);tick(c,.14);assert(c.parries==1&&c.health==100&&c.enemy_phase==E::Stagger);
  c.choose(D((d+1)%5));assert(c.attack());tick(c,.46);assert(c.enemy_health==23&&c.hits==1);
  c=ready();c.enemy_phase=E::Windup;c.enemy_duration=.6;c.enemy_direction=D(d);c.choose(D(d));c.set_guard(true);tick(c,.62);assert(c.blocks==1&&c.parries==0&&c.health==100);
  c=ready();c.enemy_phase=E::Windup;c.enemy_duration=.12;c.enemy_direction=D(d);c.choose(D((d+1)%5));c.set_guard(true);tick(c,.14);assert(c.health<100&&c.parries==0);
 }
 c=ready();c.enemy_phase=E::Windup;c.enemy_duration=.12;c.enemy_direction=D::Left;c.choose(D::Left);c.stamina=15;c.set_guard(true);c.guard_age=1;tick(c,.14);assert(c.health<100&&c.stamina==0&&!c.guard);
 c=ready();c.enemy_phase=E::Windup;c.enemy_duration=.12;assert(c.dodge());assert(!c.dodge());tick(c,.14);assert(c.health==100&&c.dodge_time>0);
 // Three committed swings share one resource pool. A fresh edge buffers a
 // direction, with no cost before the actual link and no fourth combo swing.
 c=ready();c.impact_pause_enabled=false;c.enemy_phase=E::Recovery;c.enemy_duration=20;
 c.choose(D::Left);assert(c.attack()&&c.attack_start_serial==1&&c.stamina==84);
 tick(c,.12);c.choose(D::Right);const auto after_first=c.stamina;
 assert(c.attack()&&c.pending_attack&&c.stamina==after_first);assert(!c.attack());
 for(int i=0;i<120&&c.attack_start_serial<2;++i)c.update(1./120.,c.enemy_x-2.2,c.enemy_z);
 assert(c.attack_start_serial==2&&c.combo_count==2&&c.combo_links==1&&c.attack_direction==D::Right&&c.stamina==68);
 tick(c,.08);c.choose(D::Low);assert(c.attack()&&c.stamina==68);
 for(int i=0;i<120&&c.attack_start_serial<3;++i)c.update(1./120.,c.enemy_x-2.2,c.enemy_z);
 assert(c.attack_start_serial==3&&c.combo_count==3&&c.combo_links==2&&c.stamina==52);
 assert(!c.attack()&&!c.pending_attack);tick(c,.5);
 assert(c.attack_start_serial==3&&c.hits==3&&c.enemy_health==3&&c.player_phase==P::Ready);
 // An edge queued too early expires; it cannot fire seconds later or pay energy.
 c=ready();c.impact_pause_enabled=false;c.enemy_phase=E::Recovery;c.enemy_duration=20;
 assert(c.attack());assert(c.attack());tick(c,.7);
 assert(c.attack_start_serial==1&&c.expired_buffers==1&&!c.pending_attack&&c.hits==1);
 // Insufficient energy at commitment drops a link without negative stamina.
 c=ready();c.impact_pause_enabled=false;c.enemy_phase=E::Recovery;c.enemy_duration=20;c.stamina=25;
 assert(c.attack());tick(c,.12);assert(c.attack());tick(c,.5);
 assert(c.attack_start_serial==1&&c.stamina>=9&&!c.pending_attack);
 // Pause/focus host, damage, weapon host and leave explicitly cancel the buffer.
 c=ready();assert(c.attack()&&c.attack());const auto pause_clock=c.clock;
 c.update(.2,c.enemy_x-2.2,c.enemy_z,true);assert(c.clock==pause_clock&&!c.pending_attack);
 c=ready();assert(c.attack()&&c.attack());c.clear_buffer();assert(!c.pending_attack);
 c=ready();assert(c.attack());tick(c,.12);assert(c.attack());const auto guard_serial=c.attack_start_serial;
 c.set_guard(true);assert(!c.pending_attack);tick(c,.7);assert(c.attack_start_serial==guard_serial);
 c=ready();assert(c.attack()&&c.attack());c.enemy_phase=E::Windup;c.enemy_time=0;c.enemy_duration=.01;
 tick(c,.02);assert(c.enemy_hits==1&&!c.pending_attack);
 c=ready();assert(c.attack()&&c.attack());c.leave();assert(!c.pending_attack&&c.state==S::Cancelled);
 // The knight anticipates repeated directions; attack spamming costs stamina.
 c=ready();c.enemy_phase=E::Measure;c.enemy_duration=10;c.enemy_guard=D::Right;c.choose(D::Left);assert(c.attack());tick(c,.9);assert(c.hits==1);c.choose(D::Left);assert(c.attack());tick(c,.9);assert(c.misses==1&&c.enemy_guard==D::Left);
 // A timing/spacing-aware policy can win all three rounds without cheating HP.
 c=ready();unsigned steps=0;
 for(;steps<120*180&&c.engaged();steps++){
  if(c.state==S::Fighting){
   if(c.enemy_phase==E::Windup){c.choose(c.enemy_direction);if(c.enemy_duration-c.enemy_time<.17)c.set_guard(true);else c.set_guard(false);}
   else{c.set_guard(false);if(c.player_phase==P::Ready&&c.stamina>=23){c.choose(D((unsigned(c.enemy_guard)+1)%5));c.attack();}}
  }
  c.update(1./120.,c.enemy_x-2.2,c.enemy_z);
 }
 assert(c.state==S::Won&&c.round==2&&c.health>0&&c.parries>0&&c.hits>=9);
 std::printf("PASS strategic policy: won3 rounds in%.2fs, health%.1f, hits%u, parries%u\n",steps/120.,c.health,c.hits,c.parries);
 // Unprotected, repeated-button play is punishable and cannot stunlock knights.
 c=ready();for(int i=0;i<120*180&&c.engaged();i++){c.choose(D::High);c.attack();c.update(1./120.,c.enemy_x-2.2,c.enemy_z);}assert(c.state==S::Lost&&c.enemy_hits>=5&&c.rejected_attacks>100);
 c=ready();c.update(.016,c.enemy_x-10,c.enemy_z);assert(c.state==S::Cancelled);assert(c.start(c.instructor_x,25.75,c.instructor_z)&&c.health==100&&c.stamina==100&&c.round==0);
 // Coarse frame rates cannot skip damage or consume an entire round instantly.
 for(double dt:{1./30.,1./45.,1./60.,1./144.}){c=ready();c.enemy_phase=E::Windup;c.enemy_duration=.2;for(double t=0;t<.5;t+=dt)c.update(dt,c.enemy_x-2.2,c.enemy_z);assert(c.enemy_hits==1&&c.health==82);}
 // Shared-clock proof: eight independent opponents cannot multiply player
 // regeneration or phase time. The legacy entry is inert in shared mode.
 WorldTrainingCombat one,many;assert(one.start_shared_arena()&&many.start_shared_arena());
 TrainingEvents oe,me;std::array<TrainingOpponent,8> opponents{};std::array<TrainingTarget,8> targets{};
 for(unsigned i=0;i<8;++i){opponents[i].duration=10;targets[i]={&opponents[i],i,8,true,true};}
 for(int i=0;i<140;++i){assert(one.update_shared(1./120.,nullptr,0,oe));assert(many.update_shared(1./120.,targets.data(),8,me));}
 one.stamina=many.stamina=40;one.regen_delay=many.regen_delay=0;
 for(int i=0;i<60;++i){assert(one.update_shared(1./120.,nullptr,0,oe));assert(many.update_shared(1./120.,targets.data(),8,me));}
 assert(std::abs(one.stamina-many.stamina)<1e-5&&std::abs(many.stamina-53)<.002&&std::abs(one.clock-many.clock)<1e-12);
 const double shared_clock=many.clock;many.update(.2,0,0);assert(many.clock==shared_clock);
 // Nearest reachable front defender wins, independent of array order. One
 // swing never damages all eight, and a closer rear defender is not selected.
 many.impact_pause_enabled=false;many.stamina=100;
 for(unsigned i=0;i<8;++i){opponents[i].phase=E::Recovery;opponents[i].duration=10;targets[i].distance=2.3;}
 targets[0].distance=1.2;targets[0].in_player_arc=false;targets[5].distance=1.8;
 many.choose(D::Low);assert(many.attack());me.clear();
 for(int i=0;i<30;++i)assert(many.update_shared(1./120.,targets.data(),8,me));
 assert(many.hits==1&&opponents[5].health==75&&opponents[0].health==90);
 unsigned body=0;for(unsigned i=0;i<me.count;++i)if(me.values[i].kind==TrainingContactKind::PlayerBody){++body;assert(me.values[i].opponent_id==5&&me.values[i].damage==15);}
 assert(body==1&&me.overflow==0);
 // Independent simultaneous windups resolve their actual IDs. A matching rear
 // attack bypasses a front-only guard, while a matching front one parries.
 WorldTrainingCombat guards;assert(guards.start_shared_arena());TrainingEvents ge;
 for(int i=0;i<130;++i)assert(guards.update_shared(1./120.,nullptr,0,ge));
 guards.impact_pause_enabled=false;guards.choose(D::Left);guards.set_guard(true);
 TrainingOpponent front,rear;front.phase=rear.phase=E::Windup;front.direction=rear.direction=D::Left;front.duration=rear.duration=.01;
 TrainingTarget pair[]={{&front,2,2.1,true,true},{&rear,6,2.1,false,false}};ge.clear();assert(guards.update_shared(.02,pair,2,ge));
 assert(guards.parries==1&&guards.enemy_hits==1&&guards.health==82&&front.phase==E::Stagger&&rear.phase==E::Strike);
 assert(front.parries_received==1&&rear.hits_dealt==1&&ge.overflow==0);
 unsigned actual_contacts=0;std::uint64_t serial=0;
 for(unsigned i=0;i<ge.count;++i){const auto &e=ge.values[i];assert(e.serial>serial);serial=e.serial;
  if(e.kind==TrainingContactKind::PlayerParry){++actual_contacts;assert(e.opponent_id==2);}
  if(e.kind==TrainingContactKind::EnemyBody){++actual_contacts;assert(e.opponent_id==6);}
 }assert(actual_contacts==2);
 // Invalid/duplicate targets reject transactionally; no clock or energy work.
 const auto before_invalid=guards.clock;pair[1].id=2;assert(!guards.update_shared(.2,pair,2,ge)&&guards.clock==before_invalid);
 pair[1].id=6;pair[1].distance=std::numeric_limits<double>::quiet_NaN();assert(!guards.update_shared(.2,pair,2,ge)&&guards.clock==before_invalid);
 // Physical bow adapter shares the exact enemy HP and emits one death receipt.
 TrainingOpponent arrow;arrow.health=12;TrainingEvents ae;assert(guards.damage_opponent(arrow,7,36,D::Thrust,ae));
 assert(arrow.health==0&&ae.count==2&&ae.values[0].damage==12&&ae.values[1].kind==TrainingContactKind::EnemyDefeated);
 assert(!guards.damage_opponent(arrow,7,36,D::Thrust,ae)&&ae.count==2);
 auto view=guards.opponent_view(arrow);assert(view.state==S::Won&&view.enemy_health==0&&view.hits==1&&guards.state==S::Fighting);
 assert(ae.values[0].finisher);
 // Link timing and three-swing energy remain bounded at practical frame rates.
 for(double dt:{1./30.,1./60.,1./144.}){
  auto chain=ready();chain.impact_pause_enabled=false;chain.enemy_phase=E::Recovery;chain.enemy_duration=20;
  chain.enemy_health=chain.enemy_max_health=1000;chain.choose(D::Left);assert(chain.attack());
  for(unsigned n=0;n<1000&&!(chain.player_phase==P::Ready&&chain.attack_start_serial==3);++n){
   if(chain.player_phase==P::Windup&&chain.player_time>=.06&&chain.combo_count<3&&!chain.pending_attack){chain.choose(chain.attack_direction==D::Left?D::Right:D::Left);assert(chain.attack());}
   chain.update(dt,chain.enemy_x-2.2,chain.enemy_z);
   assert(chain.stamina>=0&&chain.stamina<=100&&chain.attack_start_serial<=3&&std::isfinite(chain.player_time));
   if(chain.attack_start_serial==3&&chain.player_phase!=P::Ready)assert(chain.stamina==52);
  }
  assert(chain.attack_start_serial==3&&chain.combo_links==2&&chain.hits==3&&chain.player_phase==P::Ready);
 }
 std::puts("PASS shared clock/resources, nearest/front target, independent simultaneous hits/guard, per-actor events, arrow death once, combo3/TTL/energy/cancellation");
 std::puts("PASS all5 angles, matching/timed/wrong/low-stamina guards, riposte, reach, dodge cooldown, anti-mash,3-round win/loss/retry, pause/invalid-time/leave and30..144Hz strikes");
}
