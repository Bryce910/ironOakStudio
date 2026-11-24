#include "engine/world_combat_vfx.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace world_combat_vfx;
namespace {
bool same(Point a,Point b){return std::abs(a.x-b.x)<1e-8&&std::abs(a.y-b.y)<1e-8&&std::abs(a.z-b.z)<1e-8;}
Input fixture(){Input input;input.feet[0]={0,24,0};input.feet[1]={0,24,2.2};
 input.blades[0]={{3,25,.5},{3,25.3,1.8},true};input.blades[1]={{.2,25,1.8},{0,25.3,.8},true};return input;}
}
int main(){
 const float right[3]={1,0,0},up[3]={0,1,0};WorldTrainingCombat combat;
 combat.state=TrainingState::Fighting;combat.enemy_phase=TrainingEnemyPhase::Recovery;combat.enemy_duration=10;
 Input input=fixture();System body;Stage stage;
 body.update(combat,input,0);combat.hits=1;body.update(combat,input,0);
 auto stats=body.stats();assert(stats.body_bursts==1&&stats.metal_bursts==0&&stats.last_material==Material::Body);
 assert(std::abs(std::hypot(stats.last_contact.x-input.feet[1].x,stats.last_contact.z-input.feet[1].z)-.34)<1e-7);
 assert(stats.last_contact.y>=24.65&&stats.last_contact.y<=25.48);
 // The blade is three metres to the side; the resolved damage burst must still
 // touch the defender's body rather than hovering at that stale weapon pose.
 assert(std::abs(stats.last_contact.x)<.35);
 body.stage({0,25,-3},right,up,stage);unsigned red=0,dust=0;
 for(unsigned i=0;i<stage.count;++i){const auto &value=stage.instances[i];if(value.style[0]>=4){++red;assert(value.color[0]<1&&value.color[0]>value.color[1]*20&&value.color[0]>value.color[2]*15);}else{++dust;assert(value.style[0]==1);}}
 assert(red==32&&dust==7&&stage.count==39);body.update(combat,input,0);assert(body.stats().body_bursts==1);
 // Physical arrow contacts own their exact point independently of sword pose.
 const Point arrow{.12,25.11,1.86};input.body_contacts[1]={arrow,true};combat.hits=2;body.update(combat,input,0);
 assert(same(body.stats().last_contact,arrow)&&body.stats().body_bursts==2&&body.stats().metal_bursts==0);
 const auto before=body.stats();input.body_contacts[1].point.x=std::numeric_limits<double>::quiet_NaN();combat.hits=3;body.update(combat,input,.1);
 assert(body.stats().body_bursts==before.body_bursts&&body.stats().active_particles==before.active_particles);
 // Incoming damage uses the player body/physical contact, never enemy blade sparks.
 System incoming;input=fixture();combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;
 input.body_contacts[0]={{.2,25.05,.1},true};combat.enemy_hits=1;incoming.update(combat,input,0);
 assert(incoming.stats().incoming==1&&incoming.stats().body_bursts==1&&incoming.stats().metal_bursts==0);
 assert(same(incoming.stats().last_contact,input.body_contacts[0].point));
 // Both directions can resolve in the same update. The player contact must
 // never overwrite the opponent's physical arrow anchor used for inspection.
 System simultaneous;combat.hits=1;input.body_contacts[1]={arrow,true};simultaneous.update(combat,input,0);
 assert(simultaneous.stats().body_bursts==2);
 assert(same(simultaneous.stats().last_body_contacts[1],arrow));
 assert(same(simultaneous.stats().last_body_contacts[0],input.body_contacts[0].point));
 // A real cut's supplied direction carries the blood across the image instead
 // of hiding every source spray along the view ray. Damage weight and a final
 // body hit strengthen its initial visual mass without inventing extra hits.
 auto centroid=[](const Stage &s){Point center={};unsigned count=0;for(unsigned i=0;i<s.count;++i)if(s.instances[i].style[0]>=4){for(const auto &c:s.instances[i].corners){center.x+=c[0];center.y+=c[1];center.z+=c[2];}count+=4;}assert(count);return Point{center.x/count,center.y/count,center.z/count};};
 auto largest=[](const Stage &s){double largest=0;for(unsigned i=0;i<s.count;++i)if(s.instances[i].style[0]>=4){const auto &v=s.instances[i];largest=std::max(largest,std::hypot(double(v.corners[1][0]-v.corners[0][0]),double(v.corners[1][1]-v.corners[0][1])));}return largest;};
 System directional;input=fixture();input.body_directions[1]={1,.1,0};combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;combat.hits=1;
 directional.update(combat,input,0);directional.stage({},right,up,stage);const auto origin=centroid(stage);const auto normal_size=largest(stage);assert(normal_size>.12&&normal_size<.23);
 directional.update(combat,input,.08);directional.stage({},right,up,stage);assert(centroid(stage).x>origin.x+.05);
 System finisher;input.body_strength[1]=1.5f;input.body_finisher[1]=true;finisher.update(combat,input,0);finisher.stage({},right,up,stage);
 assert(finisher.stats().finisher_bursts==1&&finisher.stats().body_bursts==1&&finisher.stats().active_particles==55&&largest(stage)>normal_size*1.1);
 const auto initial=finisher.stats();input.body_strength[1]=std::numeric_limits<float>::quiet_NaN();combat.hits=2;finisher.update(combat,input,.1);assert(finisher.stats().body_bursts==initial.body_bursts&&finisher.stats().active_particles==initial.active_particles);
 // The impact stays readable briefly, then drops instead of turning into a
 // long-lived red cloud. All airborne blood is gone before a second passes.
 System heavy;input=fixture();input.body_directions[1]={1,.1,0};combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;combat.hits=1;
 heavy.update(combat,input,0);heavy.stage({},right,up,stage);const auto start=centroid(stage);
 unsigned mist_count=0;for(unsigned i=0;i<stage.count;++i)if(stage.instances[i].style[0]>=4){mist_count+=stage.instances[i].style[0]==5;assert(stage.instances[i].color[0]<=.281f);}
 assert(mist_count==2);for(unsigned i=0;i<12;++i)heavy.update(combat,input,1./60.);
 heavy.stage({},right,up,stage);assert(centroid(stage).y<start.y-.08);
 for(unsigned i=0;i<stage.count;++i)assert(stage.instances[i].style[0]!=5);
 for(unsigned i=0;i<30;++i)heavy.update(combat,input,1./60.);
 heavy.stage({},right,up,stage);for(unsigned i=0;i<stage.count;++i)assert(stage.instances[i].style[0]<4);
 // Near-ground body contact cannot leave a blood card hovering below the floor.
 System landing;input.body_contacts[1]={{0,24.10,2},true};landing.update(combat,input,0);
 for(unsigned i=0;i<12;++i)landing.update(combat,input,1./60.);
 assert(landing.stats().grounded_blood_particles>0);landing.stage({},right,up,stage);
 for(unsigned i=0;i<stage.count;++i)if(stage.instances[i].style[0]>=4)for(const auto &corner:stage.instances[i].corners)assert(corner[1]>24.0);
 // A blocked/guarded sword and a timed parry produce only hot iron particles.
 System steel;combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;input=fixture();
 combat.guarded=1;steel.update(combat,input,0);steel.stage({0,25,-3},right,up,stage);
 assert(steel.stats().metal_bursts==1&&steel.stats().body_bursts==0&&steel.stats().last_material==Material::Metal);
 assert(stage.count==18);for(unsigned i=0;i<stage.count;++i)assert(stage.instances[i].style[0]==0&&stage.instances[i].color[0]>1);
 const Point metal=steel.stats().last_contact;assert(std::isfinite(metal.x)&&std::isfinite(metal.y)&&std::isfinite(metal.z));
 combat.parries=1;steel.update(combat,input,0);assert(steel.stats().metal_bursts==2&&steel.stats().body_bursts==0);
 // Animation, misses and direction changes alone cannot invent material hits.
 System miss;combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;combat.misses=1;combat.player_phase=TrainingPlayerPhase::Strike;
 miss.update(combat,input,0);assert(miss.stats().body_bursts==0&&miss.stats().metal_bursts==0);
 input.paused=true;combat.hits=1;miss.update(combat,input,1);assert(miss.stats().body_bursts==0);
 input.paused=false;miss.update(combat,input,0);assert(miss.stats().body_bursts==1);miss.update(combat,input,0);assert(miss.stats().body_bursts==1);
 // Existing particle/trail capacities remain the saturation bound.
 for(unsigned i=0;i<200;++i){++combat.hits;miss.update(combat,input,.001);}
 miss.stage({},right,up,stage);assert(miss.stats().active_particles<=particle_capacity&&stage.count<=instance_capacity&&miss.stats().dropped>0);
 // A short neutral steel ribbon shows only the outer blade and stays faint
 // enough to keep body impacts readable. Visual trimming never alters input
 // sockets used to resolve contact.
 System ribbon;combat=WorldTrainingCombat{};combat.state=TrainingState::Fighting;combat.player_phase=TrainingPlayerPhase::Strike;input=fixture();
 const auto original=input.blades[0];ribbon.update(combat,input,0);input.blades[0].tip.x+=.2;ribbon.update(combat,input,0);ribbon.stage({},right,up,stage);
 assert(stage.count==1&&stage.trails==1);const auto &trail=stage.instances[0];assert(trail.style[0]==2&&std::abs(trail.color[3]-.055f)<1e-6);
 for(unsigned i=0;i<3;++i)assert(trail.color[i]<1);
 assert(trail.color[2]-trail.color[0]<.05f);
 assert(std::abs(trail.corners[0][2]-(original.base.z+(original.tip.z-original.base.z)*.25))<1e-6);
 assert(same(input.blades[0].base,original.base));combat.player_phase=TrainingPlayerPhase::Ready;ribbon.update(combat,input,.099);assert(ribbon.stats().active_trails==1);ribbon.update(combat,input,.002);assert(ribbon.stats().active_trails==0);
 std::puts("PASS material combat VFX: body-only nonemissive blood, defender-surface anchoring, exact physical arrow contacts, iron-only guarded/parry sparks, deduplication, pause, invalid-contact transaction and fixed saturation budgets");
}
