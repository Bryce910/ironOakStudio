#include "engine/world_combat_vfx.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace world_combat_vfx;
int main(){
 WorldTrainingCombat c;c.state=TrainingState::Fighting;c.enemy_phase=TrainingEnemyPhase::Recovery;c.enemy_duration=10;
 Input in;in.feet[0]={0,24,0};in.feet[1]={0,24,2.2};in.blades[0]={{0,25,.5},{0,25.3,1.8},true};in.blades[1]={{.2,25,1.8},{0,25.3,.8},true};
 System system;Stage stage;const float right[3]={1,0,0},up[3]={0,1,0};
 system.update(c,in,1./60.);system.stage({0,25,-3},right,up,stage);assert(stage.count==0);
 c.hits=1;system.update(c,in,1./60.);assert(system.stats().hits==1&&system.stats().active_particles==39);system.update(c,in,1./60.);assert(system.stats().hits==1);
 c.parries=1;system.update(c,in,1./60.);assert(system.stats().parries==1&&system.stats().active_particles>40);c.blocks=1;c.guarded=1;c.dodges=1;c.enemy_hits=1;system.update(c,in,1./60.);assert(system.stats().blocks==2&&system.stats().incoming==1&&system.stats().dust_bursts==2);
 auto before=system.stats().active_particles;in.paused=true;system.update(c,in,10);assert(system.stats().active_particles==before&&system.camera_impulse()==0);in.paused=false;
 c.player_phase=TrainingPlayerPhase::Strike;system.update(c,in,.01);in.blades[0].tip.x+=.15;system.update(c,in,.01);assert(system.stats().active_trails==1);
 system.stage({0,25,-3},right,up,stage);assert(stage.count>40&&stage.trails==1&&stage.count<=instance_capacity);for(unsigned n=0;n<stage.count;++n)for(const auto &p:stage.instances[n].corners)for(float x:p)assert(std::isfinite(x));
 // First-person ribbons use the matching viewmodel socket and overlay style.
 c.clock+=1;in.first_person=true;in.trail_blades[0]=in.blades[0];in.trail_blades[0].tip.y+=.15;system.update(c,in,.01);system.stage({0,25,-3},right,up,stage);bool overlay=false;for(unsigned i=0;i<stage.count;++i)overlay|=stage.instances[i].style[0]==3;assert(overlay);
 for(unsigned i=0;i<200;++i){c.hits++;system.update(c,in,.001);}assert(system.stats().active_particles<=particle_capacity&&system.stats().dropped>0);
 for(unsigned i=0;i<200;++i){c.player_phase=TrainingPlayerPhase::Ready;c.clock+=.01;system.update(c,in,.01);}system.stage({0,25,-3},right,up,stage);assert(stage.count==0);
 // Contact events come from range/guard rules, including real hit-stop.
 WorldTrainingCombat fight;assert(fight.start_threat());for(unsigned i=0;i<200;++i)fight.update(1./60.,fight.enemy_x-2.2,fight.enemy_z);fight.enemy_phase=TrainingEnemyPhase::Recovery;fight.enemy_duration=10;fight.choose(TrainingDirection::Left);assert(fight.attack());for(unsigned i=0;i<20&&fight.hits==0;++i)fight.update(1./60.,fight.enemy_x-2.2,fight.enemy_z);assert(fight.hits==1&&fight.player_swings==1&&fight.impact_pause>0);const double held=fight.clock;fight.update(.008,fight.enemy_x-2.2,fight.enemy_z);assert(fight.clock==held);
 auto miss=fight;miss.impact_pause=0;miss.player_phase=TrainingPlayerPhase::Ready;miss.player_time=0;miss.hits=miss.misses=0;assert(miss.attack());for(unsigned i=0;i<30;++i)miss.update(1./60.,miss.enemy_x-2.5,miss.enemy_z);assert(miss.hits==0&&miss.misses==1);
 auto bad=right; (void)bad;float invalid[3]={std::numeric_limits<float>::quiet_NaN(),0,0};system.stage({},invalid,up,stage);assert(stage.count==0);
 std::puts("PASS combat VFX: gameplay contacts, one-shot bursts, parry/block/dodge, pause, expiry, bounded saturation, blade ribbons, first-person projection mode, range and hit-stop");
}
