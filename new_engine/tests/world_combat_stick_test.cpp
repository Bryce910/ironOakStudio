#include "engine/world_combat_stick.h"
#include <array>
#include <iostream>
#include <limits>

static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)

int main(){
    using world_controller::Stick;
    using world_controller::SwordStick;
    const std::array<Stick,4> vectors={Stick{-1,0},Stick{1,0},Stick{0,-1},Stick{0,1}};
    // Attack direction describes the animation's source: Low is the rising
    // cut, High is the downward overhead cut. Guard direction is its position.
    const std::array<TrainingDirection,4> attack_directions={TrainingDirection::Left,TrainingDirection::Right,TrainingDirection::Low,TrainingDirection::High};
    const std::array<TrainingDirection,4> guard_directions={TrainingDirection::Left,TrainingDirection::Right,TrainingDirection::High,TrainingDirection::Low};
    SwordStick sword;

    // A held stick at startup cannot attack. Every cardinal gesture needs only
    // a neutral return and a deliberate stick motion; RT is not involved.
    CHECK(!sword.sample({1,0},true,false).attack);
    for(unsigned i=0;i<vectors.size();++i){
        const auto neutral=sword.sample({},true,false);
        CHECK(!neutral.attack&&!neutral.select_direction);
        const auto swing=sword.sample(vectors[i],true,false);
        CHECK(swing.attack&&swing.select_direction&&swing.direction==attack_directions[i]);
        for(unsigned frame=0;frame<240;++frame){
            const auto held=sword.sample(vectors[i],true,false);
            CHECK(!held.attack&&!held.select_direction);
        }
        CHECK(!sword.sample(vectors[(i+1)%4],true,false).attack);
    }

    // Noise and the hysteresis band cannot trigger a swing or rearm a hold.
    sword.reset();sword.sample({},true,false);
    for(const Stick noise:{Stick{.1f,-.1f},Stick{.24f,0},Stick{.26f,.1f},Stick{.49f,0}}){
        const auto result=sword.sample(noise,true,false);
        CHECK(!result.attack&&!result.select_direction);
    }
    CHECK(sword.sample({.5f,0},true,false).attack);
    CHECK(!sword.sample({.3f,0},true,false).attack);
    CHECK(!sword.sample({-.8f,0},true,false).attack);
    sword.sample({.25f,0},true,false);
    CHECK(sword.sample({-.5f,0},true,false).attack);

    // Dominant-axis diagonals are stable, including exact diagonal ties.
    const std::array<Stick,8> diagonals={Stick{.7f,.6f},Stick{-.7f,.6f},Stick{.6f,-.7f},Stick{.6f,.7f},Stick{.7f,.7f},Stick{-.7f,-.7f},Stick{0,-.5f},Stick{0,.5f}};
    const std::array<TrainingDirection,8> diagonal_directions={TrainingDirection::Right,TrainingDirection::Left,TrainingDirection::Low,TrainingDirection::High,TrainingDirection::Right,TrainingDirection::Left,TrainingDirection::Low,TrainingDirection::High};
    for(unsigned i=0;i<diagonals.size();++i){
        sword.sample({},true,false);
        const auto result=sword.sample(diagonals[i],true,false);
        CHECK(result.attack&&result.direction==diagonal_directions[i]);
    }

    // While LT is held, direction changes do not need a neutral return and
    // select all four guard directions without ever requesting an attack.
    sword.reset();
    for(unsigned cycle=0;cycle<3;++cycle)for(unsigned i=0;i<vectors.size();++i){
        const auto block=sword.sample(vectors[i],true,true);
        CHECK(block.select_direction&&!block.attack&&block.direction==guard_directions[i]);
    }
    CHECK(!sword.sample({0,.9f},true,false).attack); // LT release while deflected.
    CHECK(!sword.sample({-.9f,0},true,false).attack);
    sword.sample({},true,false);
    CHECK(sword.sample({-.9f,0},true,false).attack);

    // Even a simultaneous LT release and new deflection is not an attack.
    sword.sample({},true,true);
    CHECK(!sword.sample({1,0},true,false).attack);
    sword.sample({},true,false);
    CHECK(sword.sample({1,0},true,false).attack);
    // Returning to neutral on release is sufficient for the next gesture.
    sword.sample({0,-1},true,true);
    sword.sample({},true,false);
    CHECK(sword.sample({0,-1},true,false).attack);

    // The caller disables the helper for UI ownership, focus loss, disconnected
    // controllers and other contexts. Held motion must not survive any gate.
    for(unsigned context=0;context<4;++context){
        sword.sample({},true,false);
        CHECK(!sword.sample({1,0},false,false).attack);
        CHECK(!sword.sample({1,0},true,false).attack);
        CHECK(!sword.sample({-1,0},true,false).attack);
        sword.sample({},true,false);
        CHECK(sword.sample({1,0},true,false).attack);
    }
    sword.sample({},true,false);sword.reset();
    CHECK(!sword.sample({1,0},true,false).attack);
    sword.sample({},true,false);
    CHECK(sword.sample({1,0},true,false).attack);

    // Countdown or non-ready motion is consumed. Neither a held full flick nor
    // a partial motion during countdown can attack on the first fighting frame.
    sword.sample({},true,false,false);
    CHECK(!sword.sample({0,1},true,false,false).attack);
    CHECK(!sword.sample({0,1},true,false,true).attack);
    sword.sample({},true,false,false);
    CHECK(!sword.sample({.3f,0},true,false,false).attack);
    CHECK(!sword.sample({1,0},true,false,true).attack);
    sword.sample({},true,false,true);
    CHECK(sword.sample({1,0},true,false,true).attack);
    // Holding LT across the countdown still allows a direction update only.
    const auto countdown_guard=sword.sample({0,-1},true,true,false);
    CHECK(countdown_guard.select_direction&&!countdown_guard.attack&&countdown_guard.direction==TrainingDirection::High);
    // A centered LT guard defends the front against thrusts. The hysteresis
    // band leaves the current guard unchanged until a deliberate direction.
    const auto front_guard=sword.sample({},true,true);
    CHECK(front_guard.select_direction&&!front_guard.attack&&front_guard.direction==TrainingDirection::Thrust);
    const auto guard_band=sword.sample({.3f,0},true,true);
    CHECK(!guard_band.select_direction&&!guard_band.attack);
    const auto front_edge=sword.sample({.25f,0},true,true);
    CHECK(front_edge.select_direction&&!front_edge.attack&&front_edge.direction==TrainingDirection::Thrust);

    // Invalid hardware input disarms rather than looking like a neutral return.
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float infinity=std::numeric_limits<float>::infinity();
    for(const Stick invalid:{Stick{nan,0},Stick{0,nan},Stick{infinity,0},Stick{0,-infinity}}){
        sword.sample({},true,false);
        const auto rejected=sword.sample(invalid,true,false);
        CHECK(!rejected.attack&&!rejected.select_direction);
        CHECK(!sword.sample({1,0},true,false).attack);
        sword.sample({},true,false);
        CHECK(sword.sample({1,0},true,false).attack);
    }

    // Exercise the actual combat consumer. Only one player swing is registered
    // for a held gesture, and guarding never increases that swing count.
    for(unsigned i=0;i<vectors.size();++i){
        WorldTrainingCombat combat;
        CHECK(combat.start_threat());
        while(combat.state==TrainingState::Countdown)
            combat.update(1./60.,combat.enemy_x-2.,combat.enemy_z);
        CHECK(combat.state==TrainingState::Fighting);
        sword.reset();sword.sample({},true,false);
        const auto apply=[&](Stick value,bool guarding){
            const auto action=sword.sample(value,true,guarding,combat.state==TrainingState::Fighting);
            if(action.select_direction)combat.choose(action.direction);
            combat.set_guard(guarding);
            if(action.attack)combat.attack();
            combat.update(1./60.,combat.enemy_x-2.,combat.enemy_z);
        };
        apply(vectors[i],false);
        CHECK(combat.player_phase==TrainingPlayerPhase::Windup&&combat.attack_direction==attack_directions[i]);
        for(unsigned frame=0;frame<120;++frame)apply(vectors[i],false);
        CHECK(combat.player_swings==1);
        for(unsigned direction=0;direction<vectors.size();++direction){
            apply(vectors[direction],true);
            CHECK(combat.guard&&combat.selected==guard_directions[direction]&&combat.player_swings==1);
        }
        apply(vectors[0],false);
        CHECK(combat.player_swings==1);
    }

    // Block and parry all five real telegraphed attacks using only LT plus each
    // cardinal stick direction or the centered front guard. Wait beyond reach,
    // these checks do not assign enemy phases or manufacture combat outcomes.
    const std::array<Stick,5> guard_vectors={vectors[0],vectors[1],vectors[2],vectors[3],Stick{}};
    const std::array<TrainingDirection,5> all_guards={guard_directions[0],guard_directions[1],guard_directions[2],guard_directions[3],TrainingDirection::Thrust};
    for(unsigned i=0;i<guard_vectors.size();++i)for(bool perfect:{false,true}){
        WorldTrainingCombat combat;
        CHECK(combat.start_threat());
        double waited=0;
        while(waited<90&&!(combat.state==TrainingState::Fighting&&
              combat.enemy_phase==TrainingEnemyPhase::Windup&&combat.enemy_direction==all_guards[i])){
            combat.update(1./60.,combat.enemy_x-3.,combat.enemy_z);
            waited+=1./60.;
        }
        CHECK(waited<90&&combat.enemy_hits==0);
        if(perfect)while(combat.enemy_duration-combat.enemy_time>.14)
            combat.update(1./60.,combat.enemy_x-3.,combat.enemy_z);
        sword.reset();
        const auto guard=sword.sample(guard_vectors[i],true,true);
        CHECK(guard.select_direction&&!guard.attack&&guard.direction==all_guards[i]);
        combat.choose(guard.direction);
        combat.set_guard(true);
        unsigned guard_frames=0;
        while(combat.enemy_phase==TrainingEnemyPhase::Windup&&guard_frames++<120)
            combat.update(1./60.,combat.enemy_x-2.,combat.enemy_z);
        CHECK(guard_frames<=120&&combat.enemy_hits==0&&combat.player_swings==0);
        CHECK(perfect?(combat.parries==1&&combat.blocks==0):(combat.blocks==1&&combat.parries==0));
    }

    std::cout<<"world_combat_stick: "<<checks<<" checks passed (four-way flick attacks, continuous directional guard, neutral rearm, context gates, combat consumer)\n";
}
