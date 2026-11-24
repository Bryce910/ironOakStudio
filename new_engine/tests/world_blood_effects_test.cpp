#include "engine/world_blood_effects.h"
#include "engine/world_arena_combat.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace world_blood_effects;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static bool close(double a,double b){return std::abs(a-b)<1e-9;}
static Input fixture(){
    Input input;input.contacts[0]={{-1800,65.1,-10504},{0,0,-2},64,true};
    input.contacts[1]={{-1799.8,65.3,-10501.9},{3,4,0},64,true};return input;
}
static WorldTrainingCombat ready(){
    WorldTrainingCombat combat;combat.start_arena();
    while(combat.state==TrainingState::Countdown)combat.update(1./240.,combat.enemy_x-2.2,combat.enemy_z);
    combat.impact_pause_enabled=false;return combat;
}
int main(){
    auto input=fixture();System system;
    CHECK(system.update(input,0));CHECK(system.stage().mesh_count==0);
    input.outgoing_hits=1;input.incoming_hits=1;input.combat_clock=2;
    CHECK(system.update(input,0));auto stage=system.stage();auto stats=system.stats();
    CHECK(stage.mesh_count==4&&stage.stain_count==2&&stats.mesh_spawned==4&&stats.stains_spawned==2);
    CHECK(stage.meshes[0].defender==1&&stage.meshes[2].defender==0);
    CHECK(close(stage.meshes[0].forward.x,.6)&&close(stage.meshes[0].forward.y,.8));
    CHECK(close(stage.meshes[0].yaw,1.5707963267948966));
    CHECK(close(stage.meshes[0].position.y,65.3)&&close(stage.stains[0].position.y,64));
    CHECK(stage.meshes[0].hit_id==stage.stains[0].hit_id&&stage.meshes[0].hit_id==stage.meshes[1].hit_id&&stage.meshes[0].serial!=stage.meshes[1].serial);
    CHECK(stage.meshes[0].lobe==0&&stage.meshes[1].lobe==1&&stage.meshes[0].delay==0&&stage.meshes[1].delay==.035);
    CHECK(stage.meshes[0].floor_y==64&&stage.meshes[1].floor_y==64);
    CHECK(stats.impacts_spawned==2&&stats.secondary_spawned==2&&stats.finisher_events==0);
    CHECK(system.update(input,.125));stage=system.stage();CHECK(close(stage.meshes[0].age,.125));
    CHECK(system.stats().mesh_spawned==4); // same counters cannot replay effects

    // Pause freezes both visual lifetimes and pending event baselines.
    input.paused=true;input.outgoing_hits=2;input.combat_clock=3;
    CHECK(system.update(input,50));CHECK(close(system.stage().meshes[0].age,.125));
    CHECK(system.stats().mesh_spawned==4&&close(system.stats().elapsed,.125));
    input.paused=false;CHECK(system.update(input,0));CHECK(system.stats().mesh_spawned==6);
    CHECK(system.update(input,0));CHECK(system.stats().mesh_spawned==6);

    // The actual arena resets counters/clock at entry and again at countdown.
    // Living world-space effects must survive both, including actor relocation.
    input.outgoing_hits=input.incoming_hits=0;input.combat_clock=0;
    CHECK(system.update(input,.1));CHECK(system.stage().mesh_count==6&&system.stage().stain_count==3);
    CHECK(system.stats().clock_rebases==1&&system.stats().counter_rebases==1);
    const auto anchored=system.stage().meshes[0].position;
    input.contacts[1].position={200,10,300};input.combat_clock=.2;
    CHECK(system.update(input,0));CHECK(close(system.stage().meshes[0].position.x,anchored.x));
    CHECK(system.rebase(0,0,0));CHECK(system.stage().stain_count==3);
    input.outgoing_hits=1;CHECK(system.update(input,0));CHECK(system.stats().mesh_spawned==8);
    CHECK(system.stage().meshes[6].position.x==200);
    CHECK(system.rebase(50,80,10));input.outgoing_hits=50;input.incoming_hits=80;input.combat_clock=10;
    CHECK(system.update(input,0));CHECK(system.stats().mesh_spawned==8);

    // Explicit retry/unload removes every transient, while retaining diagnostics.
    const auto serial=system.stage().meshes[6].serial;system.reset(50,80);
    CHECK(system.stage().mesh_count==0&&system.stage().stain_count==0&&system.stats().active_meshes==0);
    CHECK(system.stats().mesh_spawned==8&&system.stats().resets==1&&system.stats().elapsed==0);
    CHECK(system.update(input,0));CHECK(system.stage().mesh_count==0);
    ++input.incoming_hits;CHECK(system.update(input,0));CHECK(system.stage().meshes[0].serial>serial);

    // Lifetime configuration is transactional and copied onto each spawn.
    System expiry;input=fixture();CHECK(expiry.configure({.2,.6}));input.outgoing_hits=1;
    CHECK(expiry.update(input,0));CHECK(expiry.configure({1,2}));
    CHECK(!expiry.configure({std::numeric_limits<double>::quiet_NaN(),2}));
    CHECK(!expiry.configure({0,2})&&!expiry.configure({31,2})&&!expiry.configure({1,121}));
    CHECK(expiry.config().mesh_duration==1&&expiry.config().stain_duration==2);
    CHECK(expiry.update(input,.24));CHECK(expiry.stage().mesh_count==0&&expiry.stage().stain_count==1);
    CHECK(expiry.update(input,.2));CHECK(expiry.update(input,.2));CHECK(expiry.stage().stain_count==0);
    CHECK(expiry.configure({1./240.,.1})&&expiry.configure({30,120}));
    input.outgoing_hits=2;CHECK(expiry.update(input,0));CHECK(expiry.stage().meshes[0].duration==30);
    const double before=expiry.stats().elapsed;CHECK(expiry.update(input,20));
    CHECK(close(expiry.stats().elapsed-before,maximum_step)&&close(expiry.stage().meshes[0].age,maximum_step));

    // Invalid time rejects the update without consuming a valid hit. A bad
    // defender contact consumes only that hit, so no delayed phantom appears.
    System invalid;input=fixture();input.outgoing_hits=1;
    CHECK(!invalid.update(input,-1));CHECK(!invalid.update(input,std::numeric_limits<double>::infinity()));
    CHECK(invalid.stats().mesh_spawned==0);CHECK(invalid.update(input,0));CHECK(invalid.stats().mesh_spawned==2);
    ++input.outgoing_hits;input.incoming_hits=1;input.contacts[1].direction={0,0,0};
    CHECK(invalid.update(input,0));CHECK(invalid.stats().invalid_contacts==1&&invalid.stats().mesh_spawned==4);
    input.contacts[1]=fixture().contacts[1];CHECK(invalid.update(input,0));CHECK(invalid.stats().mesh_spawned==4);
    const double bads[]={std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e8};
    for(double bad:bads){input.contacts[1].position.x=bad;++input.outgoing_hits;CHECK(invalid.update(input,0));}
    input.contacts[1]=fixture().contacts[1];input.contacts[1].floor_y=1e7;++input.outgoing_hits;
    CHECK(invalid.update(input,0));CHECK(invalid.stats().invalid_contacts==5);
    input.combat_clock=std::numeric_limits<double>::quiet_NaN();CHECK(!invalid.update(input,0));
    CHECK(!invalid.rebase(0,0,-1));

    // Severity belongs to the actual body event; it cannot turn a guard/miss
    // into blood or double-count a finishing impact across its two lobes.
    System heavy;input=fixture();input.contacts[1].strength=1.5f;input.contacts[1].finisher=true;input.outgoing_hits=1;
    CHECK(heavy.update(input,0));stage=heavy.stage();
    CHECK(stage.mesh_count==2&&stage.stain_count==1&&heavy.stats().finisher_events==1);
    CHECK(stage.meshes[0].strength==1.5f&&stage.meshes[1].strength==1.5f&&stage.stains[0].strength==1.5f);
    CHECK(stage.meshes[0].finisher&&stage.meshes[1].finisher&&stage.stains[0].finisher&&heavy.stats().max_strength==1.5f);
    CHECK(heavy.update(input,.1)&&heavy.stats().finisher_events==1);
    input.contacts[1].strength=std::numeric_limits<float>::quiet_NaN();++input.outgoing_hits;
    CHECK(heavy.update(input,0)&&heavy.stats().invalid_contacts==1&&heavy.stats().impacts_spawned==1);
    input.contacts[1].strength=2.1f;++input.outgoing_hits;CHECK(heavy.update(input,0)&&heavy.stats().invalid_contacts==2);
    input.contacts[1].strength=.1f;++input.outgoing_hits;CHECK(heavy.update(input,0)&&heavy.stats().invalid_contacts==3);

    // Fixed budgets retain the newest effects by serial, without growing storage.
    System bounded;input=fixture();
    for(unsigned n=1;n<=160;++n){input.outgoing_hits=n;CHECK(bounded.update(input,0));}
    stage=bounded.stage();stats=bounded.stats();
    CHECK(stage.mesh_count==mesh_capacity&&stage.stain_count==stain_capacity);
    CHECK(stats.mesh_spawned==320&&stats.mesh_replaced==312&&stats.stains_replaced==32);
    for(unsigned n=0;n<stage.mesh_count;++n)CHECK(stage.meshes[n].serial>=313);
    for(unsigned n=0;n<stage.stain_count;++n)CHECK(stage.stains[n].serial>=33);
    System jump;input.outgoing_hits=std::numeric_limits<std::uint64_t>::max();
    CHECK(jump.update(input,0));CHECK(jump.stats().mesh_spawned==events_per_defender_limit*2);
    CHECK(jump.stats().limited_events==std::numeric_limits<std::uint64_t>::max()-events_per_defender_limit);
    CHECK(jump.update(input,0));CHECK(jump.stats().mesh_spawned==events_per_defender_limit*2);
    input.outgoing_hits=0;CHECK(jump.update(input,0));input.outgoing_hits=1;CHECK(jump.update(input,0));
    CHECK(jump.stats().outgoing_events==std::numeric_limits<std::uint64_t>::max());

    // Integrate actual combat transitions: steel and misses do not change body
    // counters; accepted sword and physical incoming damage emit exactly once.
    for(unsigned scenario=0;scenario<4;++scenario){
        auto combat=ready();System real;input=fixture();
        combat.enemy_phase=scenario==1?TrainingEnemyPhase::Measure:TrainingEnemyPhase::Recovery;
        combat.enemy_duration=100;combat.choose(TrainingDirection::Left);combat.enemy_guard=TrainingDirection::Left;
        CHECK(combat.attack());
        const double distance=scenario==2?4:2.2;
        for(unsigned frame=0;frame<90;++frame){
            if(scenario==3&&frame==35){combat.enemy_phase=TrainingEnemyPhase::Windup;combat.enemy_time=0;combat.enemy_duration=.1;}
            combat.update(1./120.,combat.enemy_x-distance,combat.enemy_z);
            input.outgoing_hits=combat.hits;input.incoming_hits=combat.enemy_hits;input.combat_clock=combat.clock;
            CHECK(real.update(input,1./120.));
        }
        CHECK(real.stats().outgoing_events==combat.hits&&real.stats().incoming_events==combat.enemy_hits);
        CHECK(real.stats().mesh_spawned==2*(combat.hits+combat.enemy_hits));
        if(scenario==1||scenario==2)CHECK(real.stats().mesh_spawned==0);
        else CHECK(combat.hits==1);
        if(scenario==3)CHECK(combat.enemy_hits==1&&real.stats().mesh_spawned==4);
    }
    // Run a complete real arena bout and the following entry. The shared
    // player clock/counters now stay monotonic across opponents; living stains
    // survive the handoff and only an explicit restart clears them.
    {
        world_arena_combat::Arena arena;WorldTrainingCombat fight;System real;
        CHECK(real.configure({30,120}));arena.start(fight);input=fixture();
        bool next_duel=false;unsigned contacts=0;
        for(unsigned frame=0;frame<1800&&!next_duel;++frame){
            if(fight.state==TrainingState::Fighting&&arena.current_index()==0&&arena.active_count()){
                if(fight.enemy_phase==TrainingEnemyPhase::Windup){
                    fight.choose(fight.enemy_direction);
                    fight.set_guard(fight.enemy_duration-fight.enemy_time<.17);
                }else{
                    fight.set_guard(false);
                    if(fight.player_phase==TrainingPlayerPhase::Ready&&fight.stamina>=20){
                        fight.choose(TrainingDirection((unsigned(fight.enemy_guard)+1)%5));CHECK(fight.attack());
                    }
                }
            }
            fight.update(1./120.,fight.enemy_x-2.2,fight.enemy_z);
            const auto enemy=arena.active_position();
            arena.update(1./120.,fight,{enemy.x,0,enemy.z-2.2});
            input.outgoing_hits=fight.hits;input.incoming_hits=fight.enemy_hits;input.combat_clock=fight.clock;
            CHECK(real.update(input,1./120.));
            contacts=unsigned(real.stats().impacts_spawned);
            next_duel=arena.current_index()==1&&fight.state==TrainingState::Fighting;
        }
        CHECK(next_duel&&arena.defeated()==1&&contacts>=4&&contacts<=6);
        CHECK(real.stage().mesh_count<=8&&real.stage().stain_count==contacts);
        CHECK(real.stats().counter_rebases==0&&real.stats().clock_rebases==0);
        for(const auto &p:real.stage().stains)if(p.duration>0)CHECK(p.position.x==fixture().contacts[p.defender].position.x);
        arena.start(fight);real.reset();input.outgoing_hits=input.incoming_hits=0;input.combat_clock=0;
        CHECK(real.update(input,0));CHECK(real.stage().mesh_count==0&&real.stage().stain_count==0);
    }
    // Permanent arena marks survive more than five minutes and all eight duel
    // counter/clock rebases. Mesh effects still finish, and original impact
    // identities/positions remain unchanged instead of following new opponents.
    {
        System permanent;CHECK(permanent.configure({.25,.1,true}));input=fixture();
        std::array<Point,64> anchors={};unsigned count=0;
        for(unsigned duel=0;duel<8;++duel){
            CHECK(permanent.rebase(0,0,0));input.outgoing_hits=input.incoming_hits=0;input.combat_clock=0;
            for(unsigned hit=1;hit<=8;++hit){
                input.contacts[1].position.x=-1799.8+duel*.5+hit*.01;
                anchors[count++]={input.contacts[1].position.x,64,input.contacts[1].position.z};
                input.outgoing_hits=hit;CHECK(permanent.update(input,0));
            }
            for(unsigned frame=0;frame<180;++frame){input.combat_clock+=.25;CHECK(permanent.update(input,.25));}
        }
        stage=permanent.stage();CHECK(permanent.stats().elapsed>300&&stage.mesh_count==0&&stage.stain_count==64);
        for(unsigned n=0;n<64;++n){
            CHECK(stage.stains[n].persistent&&stage.stains[n].serial==n+1);
            CHECK(stage.stains[n].position.x==anchors[n].x&&stage.stains[n].position.y==anchors[n].y&&stage.stains[n].position.z==anchors[n].z);
        }
        CHECK(stage.stains[0].age>=360&&stage.stains[0].age>stage.stains[0].duration);
        const auto before_age=stage.stains[0].age;input.paused=true;++input.outgoing_hits;
        CHECK(permanent.update(input,1000)&&permanent.stage().stains[0].age==before_age&&permanent.stage().stain_count==64);
        input.paused=false;CHECK(permanent.update(input,0)&&permanent.stage().stain_count==65);
        for(unsigned frame=0;frame<15000;++frame)CHECK(permanent.update(input,.25));
        CHECK(permanent.stage().stains[0].age==maximum_persistent_stain_age&&permanent.stage().stain_count==65);
        // Configuration applies to new marks. Changing back to timed mode must
        // neither erase permanent marks nor resurrect already expired ones.
        CHECK(permanent.configure({.25,.1,false}));++input.outgoing_hits;CHECK(permanent.update(input,0));
        CHECK(permanent.stage().stain_count==66&&!permanent.stage().stains[65].persistent);
        CHECK(permanent.update(input,.25)&&permanent.stage().stain_count==65);
        permanent.reset(input.outgoing_hits,input.incoming_hits);
        CHECK(permanent.stage().mesh_count==0&&permanent.stage().stain_count==0);
        CHECK(permanent.update(input,0)&&permanent.stage().stain_count==0);
    }
    // Source lifetimes do not depend on common frame rates or combat clock age.
    for(unsigned fps:{30u,60u,144u}){
        System real;CHECK(real.configure({.7,1.3}));input=fixture();input.outgoing_hits=1;
        CHECK(real.update(input,0));
        for(unsigned frame=0;frame<unsigned(std::ceil(1.4*fps));++frame)CHECK(real.update(input,1./fps));
        CHECK(real.stage().mesh_count==0&&real.stage().stain_count==0);
    }
    // Actual source lengths are compressed at3x. Each deterministic variant
    // expires at its own completed playback, rather than keeping a max-length
    // invisible lobe around. Ground height remains the captured impact surface.
    {
        Config config{1,24,true};config.variant_count=3;
        const std::array<double,3> source={2.,2.5,2.8};
        for(unsigned n=0;n<3;++n)config.variant_durations[n]=(source[n]-source_start_seconds)/playback_rate;
        CHECK(playback_rate==3.&&config.variant_durations[0]>.65&&config.variant_durations[2]<.92);
        System real;CHECK(real.configure(config));
        auto bad=config;bad.variant_count=9;CHECK(!real.configure(bad));
        bad=config;bad.variant_durations[1]=std::numeric_limits<double>::quiet_NaN();CHECK(!real.configure(bad));
        bad=config;bad.variant_durations[1]=0;CHECK(!real.configure(bad));
        for(unsigned hit=0;hit<6;++hit){
            real.reset();input=fixture();input.contacts[1].floor_y=64.032;input.outgoing_hits=1;CHECK(real.update(input,0));
            auto born=real.stage();CHECK(born.mesh_count==2);
            for(unsigned lobe=0;lobe<2;++lobe){
                CHECK(close(born.meshes[lobe].duration,config.variant_durations[(hit+lobe)%3]+born.meshes[lobe].delay));
                CHECK(born.meshes[lobe].floor_y==64.032&&born.meshes[lobe].duration<1);
            }
            double elapsed=0;
            for(unsigned frame=0;frame<1000;++frame){
                CHECK(real.update(input,.001));elapsed+=.001;const auto now=real.stage();
                for(const auto &p:now.meshes)if(p.duration>0)CHECK(elapsed<p.duration+1e-9);
            }
            CHECK(real.stage().mesh_count==0&&real.stage().stain_count==1);
        }
    }
    std::cout<<"PASS "<<checks<<" blood lifecycle checks: real body events, simultaneous defenders, deduplication, wave rebasing, retry, pause, duration expiry, world anchoring, finite inputs and fixed saturation budgets\n";
}
