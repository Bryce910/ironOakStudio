#include "engine/world_training_combat.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(false)
void tick(WorldTrainingCombat &combat,double dt,bool paused=false){
    combat.update(dt,combat.enemy_x+1.55,combat.enemy_z,paused);
}
void ready(WorldTrainingCombat &combat,double dt){
    for(unsigned frame=0;frame<2000&&combat.state==TrainingState::Countdown;++frame)tick(combat,dt);
    CHECK(combat.state==TrainingState::Fighting);
}
void begin_without_recovery(WorldTrainingCombat &combat,float health,float stamina=47){
    CHECK(combat.start_threat(false,false,false));
    // The host restores these values after starting each consecutive opponent.
    combat.health=health;combat.stamina=stamina;
    CHECK(!combat.round_health_recovery);
}
}

int main(){try{
    using Difficulty=world_combat_difficulty::Difficulty;
    // Reproduce the native 91.9 -> 100 countdown boundary at every difficulty
    // and supported render rate; only explicitly disabled sessions carry HP.
    for(auto difficulty:{Difficulty::Story,Difficulty::Normal,Difficulty::Hard,Difficulty::Expert})
    for(double dt:{1./144.,1./60.,1./23.,1./15.,.25})
    for(float carried:{91.9f,57.f,7.f}){
        WorldTrainingCombat combat;combat.difficulty=difficulty;
        begin_without_recovery(combat,carried);
        const auto countdown=combat.phase_time;
        for(unsigned i=0;i<12;++i)tick(combat,dt,true);
        CHECK(combat.phase_time==countdown&&combat.health==carried&&combat.stamina==47);
        for(unsigned i=0;i<2000&&combat.state==TrainingState::Countdown;++i){
            tick(combat,dt);CHECK(combat.health==carried);
        }
        CHECK(combat.state==TrainingState::Fighting&&combat.health==carried);
        CHECK(combat.stamina>=82&&combat.stamina<=89);
        CHECK(combat.difficulty==difficulty);
    }
    // The two-argument legacy API keeps the original +15 recovery and cap.
    for(float before:{7.f,57.f,91.9f}){
        WorldTrainingCombat combat;CHECK(combat.start_threat(false,true));
        combat.health=before;combat.stamina=47;
        ready(combat,1./60.);
        CHECK(combat.round_health_recovery&&combat.health==std::min(100.f,before+15));
        CHECK(combat.stamina>=82&&combat.watch_reward);
    }
    // A refused start cannot alter the live session's recovery policy.
    {
        WorldTrainingCombat combat;begin_without_recovery(combat,57);
        CHECK(!combat.start_threat(false,false,true));
        CHECK(!combat.start_arena());
        CHECK(!combat.start(combat.instructor_x,25.75,combat.instructor_z));
        CHECK(!combat.round_health_recovery);
        ready(combat,1./60.);CHECK(combat.health==57);
    }
    // Mode changes reset the policy instead of leaking Battlefield behavior
    // into later training, camp threats, or either arena implementation.
    for(unsigned mode=0;mode<5;++mode){
        WorldTrainingCombat combat;begin_without_recovery(combat,57);
        combat.leave();CHECK(combat.state==TrainingState::Cancelled);
        if(mode==0){
            CHECK(combat.start_threat());combat.health=57;combat.stamina=47;
        }else if(mode==1){
            CHECK(combat.start(combat.instructor_x,25.75,combat.instructor_z));
            combat.health=57;combat.stamina=47;
        }else if(mode==2){
            CHECK(combat.start_arena(57,47,90));
        }else if(mode==3){
            CHECK(combat.start_shared_arena());combat.health=57;combat.stamina=47;
        }else{
            combat.reset_session();CHECK(combat.state==TrainingState::Idle);
            CHECK(combat.round_health_recovery);continue;
        }
        CHECK(combat.round_health_recovery);
        if(mode==3){
            TrainingEvents events;
            for(unsigned frame=0;frame<200&&combat.state==TrainingState::Countdown;++frame)
                CHECK(combat.update_shared(1./60.,nullptr,0,events));
            CHECK(combat.state==TrainingState::Fighting&&combat.health==57);
        }else{
            ready(combat,1./60.);CHECK(combat.health==(mode==2?57:72));
        }
    }
    // Carry real simulator-inflicted damage through all three escape threats.
    // No health delta, win, or damage receipt is fabricated by this fixture.
    {
        WorldTrainingCombat combat;float carried=91.9f;
        for(unsigned enemy=0;enemy<3;++enemy){
            begin_without_recovery(combat,carried);
            ready(combat,1./60.);CHECK(combat.health==carried);
            const auto old_hits=combat.enemy_hits;
            for(unsigned frame=0;frame<1200&&combat.enemy_hits==old_hits;++frame)tick(combat,1./60.);
            CHECK(combat.enemy_hits>old_hits&&combat.health>0&&combat.health<carried);
            carried=combat.health;combat.leave();
        }
    }
    std::printf("PASS %u per-session combat recovery/countdown/pause/mode-reset/real-damage checks\n",checks);
    return 0;
}catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
