#include "engine/world_arena_camera.h"
#include "engine/world_follow_camera.h"
#include <cstdio>
#include <limits>
using namespace world_arena_combat;
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(){
    Arena arena;WorldTrainingCombat fight;WorldArenaTargetLock lock;const auto player=arena.player_spawn();
    const float pi=3.14159265359f;
    CHECK(!lock.locked()&&!lock.toggle(arena,player,pi));
    CHECK(arena.start(fight,{8,1}));
    CHECK(!lock.toggle(arena,player,0)); // Never spin toward enemies behind the camera.
    CHECK(lock.toggle(arena,player,pi)&&lock.target()==0); // Waiting knights cannot steal the lock.
    CHECK(!lock.toggle(arena,player,pi)&&!lock.locked());
    CHECK(arena.start(fight,{8,8}));CHECK(lock.toggle(arena,player,pi));
    const auto selected=lock.target();const auto serial=fight.attack_start_serial;
    const auto closer=arena.challengers()[(selected+3)%8].position+Vec3{0,0,-.6};
    arena.update(0.,fight,closer,pi,false,selected);
    CHECK(arena.current_index()==selected); // HUD follows explicit target rather than nearest knight.
    CHECK(lock.maintain(arena,closer)&&lock.target()==selected);
    arena.update(0.,fight,closer,pi,false);
    CHECK(arena.current_index()!=selected); // The previous test genuinely crossed a nearest-target change.
    CHECK(lock.maintain(arena,closer)&&lock.target()==selected);
    CHECK(fight.attack_start_serial==serial); // Targeting never starts a paid attack.
    float yaw=pi;
    for(unsigned i=0;i<120;++i){
        const float before=yaw;yaw=lock.steer(arena,closer,yaw,1.f/60.f);
        CHECK(std::isfinite(yaw)&&std::abs(std::remainder(double(yaw-before),6.28318530718))<=4.5/60.+1e-5);
        const auto follow=world_follow_camera(closer.x,1.75,closer.z,yaw,-.04f,3.6f,1.f/60.f,true,
            [](double,double){return 0.;},[](double,double,double,double,float,float,float){return true;});
        CHECK(follow.distance>3.59&&std::abs((closer.x-follow.x)*std::cos(double(yaw))+(closer.z-follow.z)*std::sin(double(yaw)))<1e-6);
    }
    CHECK(!lock.maintain(arena,{100,0,100})&&!lock.locked());
    CHECK(!lock.toggle(arena,player,std::numeric_limits<float>::quiet_NaN()));
    CHECK(lock.toggle(arena,player,pi));arena.cancel(fight);
    CHECK(!lock.maintain(arena,player)&&!lock.locked());
    CHECK(arena.start(fight,{1,1}));CHECK(lock.toggle(arena,player,pi));
    for(unsigned i=0;i<2400&&arena.state()!=State::Lost;++i)arena.update(1./120.,fight,player,pi,false,lock.target());
    CHECK(arena.state()==State::Lost&&!lock.maintain(arena,player)&&!lock.locked());
    CHECK(arena.start(fight,{2,1},true));CHECK(lock.toggle(arena,player,pi));
    for(unsigned i=0;i<120*90&&arena.defeated()==0&&arena.state()!=State::Lost;++i){
        if(fight.state==TrainingState::Fighting&&arena.active_count()){
            const auto *opponent=arena.opponent(lock.target());
            if(opponent->phase==TrainingEnemyPhase::Windup){fight.choose(opponent->direction);fight.set_guard(opponent->duration-opponent->time<.17);}
            else{fight.set_guard(false);if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=35){fight.choose(TrainingDirection((unsigned(opponent->guard)+1)%4));fight.attack();}}
        }
        yaw=lock.steer(arena,player,yaw,1.f/120.f);
        arena.update(1./120.,fight,player,pi-yaw,false,lock.target());
    }
    CHECK(arena.defeated()==1&&arena.state()!=State::Lost&&arena.opponent(0)->health==0);
    CHECK(!lock.maintain(arena,player)&&!lock.locked());
    for(unsigned i=0;i<240;++i)arena.update(1./120.,fight,player,pi,false,lock.target());
    CHECK(!lock.maintain(arena,player)&&!lock.locked()); // A replacement never silently steals the camera.
    std::printf("PASS arena camera: %u checks, stable explicit target, normal player boom, bounded yaw, release and no attack side effects\n",checks);
}
