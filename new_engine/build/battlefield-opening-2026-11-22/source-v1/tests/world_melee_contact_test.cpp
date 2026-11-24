#include "engine/world_melee_contact.h"
#include "engine/world_training_combat.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using W=world_weapon_loadout::Weapon;
using D=TrainingDirection;
namespace contact=world_melee_contact;
static WorldTrainingCombat ready(W w,D d){
 WorldTrainingCombat f;assert(f.start_shared_arena());f.state=TrainingState::Fighting;f.impact_pause_enabled=false;f.weapon=w;f.choose(d);return f;
}
static TrainingOpponent defender(bool guarding,D d){
 TrainingOpponent o;o.health=o.max_health=500;o.phase=guarding?TrainingEnemyPhase::Measure:TrainingEnemyPhase::Recovery;o.duration=100;o.guard=d;return o;
}
static TrainingTarget target(TrainingOpponent &o,unsigned id,double x,double z){return {&o,id,std::hypot(x,z),true,true,x,z,true};}
static void strike(WorldTrainingCombat &f,TrainingTarget *t,std::size_t n,TrainingEvents &e){assert(f.attack());for(unsigned i=0;i<90;++i)assert(f.update_shared(1./240.,t,n,e));}
int main(){
 unsigned boundary_checks=0;
 for(W w:{W::GreatAxe,W::Staff})for(unsigned d=0;d<5;++d){
  const auto poly=contact::polygon(w,d);assert(poly.points&&poly.count>=3);
  auto front=poly.points[0];for(std::size_t i=1;i<poly.count;++i)if(poly.points[i].z>front.z)front=poly.points[i];
  for(bool guarding:{false,true}){
   const double radius=guarding?contact::guard_radius:contact::body_radius;
   assert(contact::intersects(w,d,front.x,front.z+radius-1e-5,guarding));
   assert(!contact::intersects(w,d,front.x,front.z+radius+1e-5,guarding));boundary_checks+=2;
  }
  assert(!contact::intersects(w,d,1.3,1.1));assert(!contact::intersects(w,d,0,-1.3));
  assert(!contact::intersects(w,d,0,world_weapon_loadout::profile(w).reach-.01));
  auto inside=defender(false,D(d));auto f=ready(w,D(d));auto t=target(inside,0,front.x,front.z+contact::body_radius-1e-4);TrainingEvents events;
  strike(f,&t,1,events);assert(f.hits==1&&inside.health<500&&f.guarded==0);
  auto outside=defender(false,D(d));f=ready(w,D(d));t=target(outside,0,front.x,front.z+contact::body_radius+1e-4);events.clear();
  strike(f,&t,1,events);assert(f.hits==0&&f.misses==1&&outside.health==500);
  // A defender's weapon can meet the swing before its body reaches the blade.
  auto guard=defender(true,D(d));f=ready(w,D(d));t=target(guard,0,front.x,front.z+.75);events.clear();
  strike(f,&t,1,events);assert(f.hits==0&&f.guarded==1&&guard.health==500&&guard.blocks_made==1);
  for(unsigned i=0;i<events.count;++i)assert(events.values[i].kind!=TrainingContactKind::PlayerBody);
  auto wrong_guard=defender(true,D((d+1)%5));f=ready(w,D(d));t=target(wrong_guard,0,front.x,front.z+.75);events.clear();
  strike(f,&t,1,events);assert(f.hits==0&&f.guarded==0&&wrong_guard.health==500);
  auto flank=defender(false,D(d));f=ready(w,D(d));t=target(flank,0,1.3,1.1);events.clear();strike(f,&t,1,events);assert(f.hits==0&&flank.health==500);
 }
 // Reject malformed spatial inputs before touching the simulation transaction.
 auto f=ready(W::GreatAxe,D::High);auto o=defender(false,D::High);auto t=target(o,0,0,1.7);TrainingEvents e;
 const auto clock=f.clock;const auto stamina=f.stamina;
 t.local_x=std::numeric_limits<double>::quiet_NaN();assert(!f.update_shared(.1,&t,1,e)&&f.clock==clock&&f.stamina==stamina);
 t.local_x=0;t.local_z=std::numeric_limits<double>::infinity();assert(!f.update_shared(.1,&t,1,e)&&f.clock==clock);
 t.local_z=1.7;t.distance=1.3;assert(!f.update_shared(.1,&t,1,e)&&f.clock==clock);
 assert(!contact::intersects(W::GreatAxe,99,0,1.4));assert(!contact::intersects(W(99),0,0,1.4));
 assert(!contact::intersects(W::Staff,0,NAN,1.4));assert(!contact::intersects(W::Staff,0,0,INFINITY));assert(!contact::intersects(W::Staff,0,1e300,1e300));
 // The closest actual reachable defender wins; a closer off-axis actor is skipped.
 for(W w:{W::GreatAxe,W::Staff}){
  TrainingOpponent a=defender(false,D::High),b=defender(false,D::High),c=defender(false,D::High);
  TrainingTarget ts[]={target(a,0,1.3,.2),target(b,5,0,1.8),target(c,1,0,1.7)};
  f=ready(w,D::High);e.clear();strike(f,ts,3,e);assert(a.health==500&&b.health==500&&c.health<500&&f.hits==1);
  unsigned body=0;for(unsigned i=0;i<e.count;++i)if(e.values[i].kind==TrainingContactKind::PlayerBody){++body;assert(e.values[i].opponent_id==1);}assert(body==1);
 }
 // Existing aggregate callers are conservative forward-distance queries.
 for(W w:{W::GreatAxe,W::Staff}){
  o=defender(false,D::High);f=ready(w,D::High);TrainingTarget legacy{&o,0,1.7,true,true};e.clear();strike(f,&legacy,1,e);assert(f.hits==1);
  o=defender(false,D::High);f=ready(w,D::High);legacy.distance=world_weapon_loadout::profile(w).reach-.01;e.clear();strike(f,&legacy,1,e);assert(f.hits==0&&o.health==500);
 }
 // Swords retain their old profile reach even outside the new axe/staff hulls.
 for(W w:{W::Sword,W::SwordShield}){o=defender(false,D::High);f=ready(w,D::High);TrainingTarget legacy{&o,0,world_weapon_loadout::profile(w).reach-.01,true,true};e.clear();strike(f,&legacy,1,e);assert(f.hits==1);}
 std::printf("PASS contact gate: %u inside/outside body+guard boundaries, all10 calibrated cuts, guard-only steel contact, no remote/off-axis blood, invalid transactions, nearest reachable target, conservative story fallback, swords unchanged\n",boundary_checks);
}
