#include "engine/world_arena_combat.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace world_arena_combat;
static unsigned shots_at_head(bool use_orc){
    Arena arena;WorldTrainingCombat fight;arena.set_weapon(Weapon::Bow);
    // Measured four-idle PowerOrc profile, rounded up to centimetres.
    if(use_orc)assert(arena.set_opponent_body_profile(.33,2.11));
    assert(arena.start(fight,{1,1,use_orc?world_arena_opponents::Kind::Orc:world_arena_opponents::Kind::Human}));
    const auto player=arena.player_spawn();
    for(unsigned i=0;i<3000&&(arena.state()!=State::Fighting||fight.state!=TrainingState::Fighting);++i)arena.update(1./120.,fight,player,0,false);
    assert(arena.state()==State::Fighting&&fight.state==TrainingState::Fighting&&arena.active_count()==1);
    // A geometry-only fixture holds the real opponent for a deterministic shot;
    // health, damage, projectile integration and victory logic stay ordinary.
    assert(arena.fixture_geometry_pose(0,{0,0,-3.4},3.14159265359f,TrainingDirection::High,fight.attack_start_serial));
    assert(arena.begin_draw(fight));for(unsigned i=0;i<4;++i)arena.update(.25,fight,player,0,false);
    assert(arena.release_draw(fight,{0,2.04,-4.4},{0,0,1}));
    for(unsigned i=0;i<120;++i)arena.update(1./120.,fight,player,0,false);
    assert(arena.shots()==1);return arena.arrow_hits();
}
int main(){
    Arena arena;assert(arena.opponent_body_radius()==.34&&arena.opponent_body_height()==1.8);
    assert(arena.set_opponent_body_profile(.33,2.11));
    for(const auto invalid:std::array<std::array<double,2>,7>{{{{.1,2.11}},{{.8,2.11}},{{.33,.9}},{{.33,3.6}},{{.75,1.}},{{std::numeric_limits<double>::quiet_NaN(),2.11}},{{.33,std::numeric_limits<double>::infinity()}}}}){
        assert(!arena.set_opponent_body_profile(invalid[0],invalid[1]));assert(arena.opponent_body_radius()==.33&&arena.opponent_body_height()==2.11);
    }
    assert(shots_at_head(false)==0&&shots_at_head(true)==1);
    std::puts("Opponent profile: human defaults unchanged;7 invalid profiles rejected; real2.04m bow shot misses human and hits measured2.11m Orc capsule");
}
