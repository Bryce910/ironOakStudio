#include "engine/world_combat_vfx.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace world_combat_vfx;
namespace {
const float right[3]={1,0,0},up[3]={0,1,0};
Point center(const Instance &i){Point p;for(const auto &c:i.corners){p.x+=c[0]/4.;p.y+=c[1]/4.;p.z+=c[2]/4.;}return p;}
Input contact(unsigned kind,Point axis={1,0,0}){
 Input in;in.resolved_events=true;in.contact_count=1;
 in.contacts[0]={1,kind,{2,1.5,3},{2,0,3},axis,1,false};return in;
}
void tick(System &s,WorldTrainingCombat &c,Input &in,double dt){c.clock+=dt;s.update(c,in,dt);}
}
int main(){
 // All three steel outcomes have a small immediate contact core. Blocking is
 // weaker than parrying, and each event is emitted exactly once.
 for(unsigned kind:{1u,2u,4u}){
  System s;WorldTrainingCombat c;auto in=contact(kind);Stage stage;
  s.update(c,in,0);const unsigned expected=kind==1?31:19;
  assert(s.stats().metal_bursts==1&&s.stats().body_bursts==0&&s.stats().active_particles==expected);
  s.stage({},right,up,stage);assert(stage.count==expected);unsigned cores=0;
  for(unsigned i=0;i<stage.count;++i){const auto p=center(stage.instances[i]);assert(std::abs(p.x-2)<1e-6&&std::abs(p.y-1.5)<1e-6&&std::abs(p.z-3)<1e-6);cores+=stage.instances[i].color[0]>7.5f;}
  assert(cores==1);
  tick(s,c,in,0);assert(s.stats().metal_bursts==1&&s.stats().active_particles==expected);
  // At40ms the thin contact streak remains anchored; every genuine spark
  // has traveled into the supplied impact-away hemisphere.
  tick(s,c,in,.04);s.stage({},right,up,stage);assert(stage.count==expected);unsigned anchored=0;
  for(unsigned i=0;i<stage.count;++i){const auto p=center(stage.instances[i]);if(std::abs(p.x-2)<1e-6){++anchored;assert(std::abs(p.y-1.5)<1e-6&&std::abs(p.z-3)<1e-6);}else assert(p.x>2);}
  assert(anchored==1);
  tick(s,c,in,.09);tick(s,c,in,.05);assert(s.stats().active_particles==0);
 }
 // Mirrored impact direction changes the actual particle trajectory.
 {
  System s;WorldTrainingCombat c;auto in=contact(2,{-1,0,0});Stage stage;s.update(c,in,0);tick(s,c,in,.04);s.stage({},right,up,stage);
  unsigned anchored=0;for(unsigned i=0;i<stage.count;++i){const auto x=center(stage.instances[i]).x;if(std::abs(x-2)<1e-6)++anchored;else assert(x<2);}assert(anchored==1);
 }
 // A pause preserves particles and suppresses the impulse; no resume replay.
 {
  System s;WorldTrainingCombat c;auto in=contact(1);s.update(c,in,0);in.paused=true;tick(s,c,in,.1);assert(s.stats().active_particles==31&&s.camera_impulse()==0);in.paused=false;tick(s,c,in,.04);assert(s.stats().metal_bursts==1);
 }
 // The resolved-event path is authoritative. An air miss, swing counters, and
 // stale aggregate body/steel counters cannot synthesize impact feedback.
 {
  System s;WorldTrainingCombat c;Input in;in.resolved_events=true;c.misses=2;c.player_swings=3;c.enemy_swings=4;c.hits=7;c.parries=8;c.guarded=9;c.blocks=10;c.enemy_hits=11;s.update(c,in,.016);
  assert(s.stats().metal_bursts==0&&s.stats().body_bursts==0&&s.stats().active_particles==0&&s.camera_impulse()==0);
  in=contact(5);s.update(c,in,.016);assert(s.stats().active_particles==0);
 }
 // Body blood, dust and clot persistence retain their existing behavior.
 {
  System s;WorldTrainingCombat c;auto in=contact(0);s.update(c,in,0);assert(s.stats().body_bursts==1&&s.stats().metal_bursts==0&&s.stats().active_particles==39);
  tick(s,c,in,.09);tick(s,c,in,.09);assert(s.stats().active_particles>0&&s.stats().last_material==Material::Body);
 }
 // Peak multi-opponent event load remains bounded by the existing pool.
 {
  System s;WorldTrainingCombat c;auto in=contact(1);in.contact_count=64;for(unsigned n=0;n<64;++n){in.contacts[n]=in.contacts[0];in.contacts[n].serial=n+1;}
  s.update(c,in,0);assert(s.stats().active_particles==particle_capacity&&s.stats().dropped>0&&s.stats().metal_bursts==64);
 }
 std::puts("PASS real-contact VFX: anchored core, directional block/parry sparks, <180ms expiry, serial dedup, pause, no miss impact, preserved body blood, bounded64-event saturation");
}
