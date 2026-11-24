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
    CHECK(!lock.locked()&&!lock.cycle(arena,player,pi));
    CHECK(arena.start(fight,{8,1}));
    CHECK(lock.cycle(arena,player,0)&&lock.target()==0); // A sole approaching enemy can be behind the hero.
    CHECK(lock.cycle(arena,player,pi)&&lock.target()==0); // Waiting knights cannot steal the lock.
    CHECK(lock.cycle(arena,player,pi)&&lock.target()==0); // Tapping with one enemy keeps focus.
    lock.clear();CHECK(!lock.maintain(arena,player)&&!lock.locked()); // Free camera never auto-acquires.
    CHECK(arena.start(fight,{2,2}));CHECK(lock.cycle(arena,player,pi));
    unsigned first=lock.target();const auto serial=fight.attack_start_serial;
    for(unsigned i=0;i<40;++i){
        CHECK(lock.cycle(arena,player,float(i)*.8f));
        CHECK(lock.target()==(first+1)%2);
        arena.update(1./240.,fight,player,pi,false,lock.target());
        CHECK(arena.current_index()==lock.target()&&fight.attack_start_serial==serial);
        first=lock.target();
    }
    lock.clear();CHECK(arena.start(fight,{8,8}));CHECK(lock.cycle(arena,player,pi));
    const auto selected=lock.target();unsigned visited=1u<<selected;
    for(unsigned i=1;i<=8;++i){
        CHECK(lock.cycle(arena,player,float(i)*1.4f));
        CHECK(lock.target()==(selected+i)%8);visited|=1u<<lock.target();
    }
    CHECK(visited==255&&lock.target()==selected);
    const auto closer=arena.challengers()[(selected+3)%8].position+Vec3{0,0,-.6};
    arena.update(1./240.,fight,closer,pi,false,selected);
    CHECK(arena.current_index()==selected); // HUD follows explicit target rather than nearest knight.
    CHECK(lock.maintain(arena,closer)&&lock.target()==selected);
    arena.update(1./240.,fight,closer,pi,false);
    CHECK(arena.current_index()!=selected);
    CHECK(lock.maintain(arena,closer)&&lock.target()==selected);
    CHECK(fight.attack_start_serial==serial);
    float yaw=pi;
    for(unsigned i=0;i<120;++i){
        const float before=yaw;yaw=lock.steer(arena,closer,yaw,1.f/60.f);
        CHECK(std::isfinite(yaw)&&std::abs(std::remainder(double(yaw-before),6.28318530718))<=4.5/60.+1e-5);
        const auto follow=world_follow_camera(closer.x,1.75,closer.z,yaw,-.04f,3.6f,1.f/60.f,true,
            [](double,double){return 0.;},[](double,double,double,double,float,float,float){return true;});
        CHECK(follow.distance>3.59&&std::abs((closer.x-follow.x)*std::cos(double(yaw))+(closer.z-follow.z)*std::sin(double(yaw)))<1e-6);
    }
    CHECK(!lock.maintain(arena,{100,0,100})&&!lock.locked());
    CHECK(!lock.cycle(arena,player,std::numeric_limits<float>::quiet_NaN()));
    CHECK(lock.cycle(arena,player,pi));
    CHECK(!lock.maintain(arena,{std::numeric_limits<double>::quiet_NaN(),0,0})&&!lock.locked());
    CHECK(lock.cycle(arena,player,pi));arena.cancel(fight);
    CHECK(!lock.maintain(arena,player)&&!lock.locked());
    CHECK(arena.start(fight,{1,1}));CHECK(lock.cycle(arena,player,pi));
    for(unsigned i=0;i<2400&&arena.state()!=State::Lost;++i)arena.update(1./120.,fight,player,pi,false,lock.target());
    CHECK(arena.state()==State::Lost&&!lock.maintain(arena,player)&&!lock.locked());
    CHECK(arena.start(fight,{2,2},true));CHECK(lock.cycle(arena,player,pi));
    const auto killed=lock.target();yaw=pi;
    for(unsigned i=0;i<120*90&&arena.defeated()==0&&arena.state()!=State::Lost;++i){
        if(fight.state==TrainingState::Fighting&&arena.active_count()){
            const auto *opponent=arena.opponent(lock.target());
            if(opponent->phase==TrainingEnemyPhase::Windup){fight.choose(opponent->direction);fight.set_guard(opponent->duration-opponent->time<.17);}
            else{fight.set_guard(false);if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=35){fight.choose(TrainingDirection((unsigned(opponent->guard)+1)%4));fight.attack();}}
        }
        yaw=lock.steer(arena,player,yaw,1.f/120.f);
        arena.update(1./120.,fight,player,pi-yaw,false,lock.target());
    }
    CHECK(arena.defeated()==1&&arena.state()!=State::Lost&&arena.opponent(killed)->health==0);
    CHECK(lock.maintain(arena,player)&&lock.locked()&&lock.target()!=killed);
    arena.update(1./240.,fight,player,pi,false,lock.target());CHECK(arena.current_index()==lock.target());
    const auto survivor=lock.target();
    CHECK(lock.cycle(arena,player,pi)&&lock.target()==survivor); // Dead target is skipped.
    // Range loss falls through to another genuinely present enemy, with no pose/health edits.
    CHECK(arena.start(fight,{8,8}));lock.clear();CHECK(lock.cycle(arena,player,pi));
    const unsigned old=lock.target();const auto pos=arena.challengers()[old].position;
    Vec3 edge={pos.x-27.99,0,pos.z};
    CHECK(lock.maintain(arena,edge)&&lock.target()==old);
    edge.x-=.1;
    CHECK(lock.maintain(arena,edge)&&lock.target()!=old);
    WorldArenaFocusGesture gesture;
    CHECK(gesture.press()&&!gesture.press());
    for(unsigned i=0;i<26;++i)CHECK(!gesture.update(1.f/60.f));
    CHECK(gesture.update(1.f/60.f));
    for(unsigned i=0;i<120;++i)CHECK(!gesture.update(1.f/60.f));
    CHECK(!gesture.press());gesture.reset();CHECK(gesture.press());
    CHECK(!gesture.update(std::numeric_limits<float>::quiet_NaN()));
    CHECK(!gesture.update(-1));CHECK(!gesture.update(0));
    gesture.reset();CHECK(!gesture.update(1));
    for(unsigned i=0;i<40;++i){CHECK(gesture.press());CHECK(!gesture.update(.1f));gesture.reset();}
    CHECK(gesture.press());CHECK(!gesture.update(100)); // Frame stalls cannot count as holding through a menu.
    std::printf("PASS arena focus: %u checks; 2/8 opponent cycles, genuine death retarget, range fallback, bounded yaw/player boom, tap/hold debouncing, no attack side effects\n",checks);
}
