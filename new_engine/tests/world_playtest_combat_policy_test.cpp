#include "engine/world_training_combat.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

// Preflight the actual input policy used by the opt-in SDL runtime workflow.
// No combat phase, health, damage, result or attack-counter assignments.
int main(){
    for(bool assist:{false,true})for(double dt:{1./30.,1./45.,1./60.,1./144.}){
        WorldTrainingCombat fight;
        assert(fight.start_threat(assist,false));
        double elapsed=0;
        while(fight.engaged()&&elapsed<180){
            fight.update(dt,fight.enemy_x-2.15,fight.enemy_z);elapsed+=dt;
        }
        assert(fight.state==TrainingState::Lost&&fight.health==0&&fight.enemy_hits>=5);
        const unsigned defeat_hits=fight.enemy_hits;
        assert(fight.start_threat(assist,false));
        assert(fight.health==100&&fight.stamina==100&&!fight.hits&&!fight.enemy_hits&&!fight.parries);
        elapsed=0;
        while(fight.engaged()&&elapsed<180){
            if(fight.state==TrainingState::Fighting){
                if(fight.enemy_phase==TrainingEnemyPhase::Windup){
                    fight.choose(fight.enemy_direction);
                    const bool defend=fight.enemy_duration-fight.enemy_time<.17;
                    if(defend&&!fight.guard)fight.set_guard(true);
                    if(!defend&&fight.guard)fight.set_guard(false);
                }else{
                    if(fight.guard)fight.set_guard(false);
                    if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=23){
                        fight.choose(TrainingDirection((unsigned(fight.enemy_guard)+1)%5));fight.attack();
                    }
                }
            }
            fight.update(dt,fight.enemy_x-2.15,fight.enemy_z);elapsed+=dt;
        }
        assert(fight.state==TrainingState::Won&&fight.health>0&&fight.hits>=3&&fight.parries>0&&fight.player_swings>=fight.hits);
        std::printf("PASS %3.0fHz assist%d: unprotected defeat %u hits; duel won in %.2fs with %u hits/%u parries/%.1f health\n",
                    1/dt,int(assist),defeat_hits,elapsed,fight.hits,fight.parries,fight.health);
    }
}
