#include "engine/world_training_combat.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

using D=TrainingDirection;using E=TrainingEnemyPhase;using P=TrainingPlayerPhase;using S=TrainingState;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static WorldTrainingCombat ready(){
    WorldTrainingCombat c;c.start_arena();
    while(c.state==S::Countdown)c.update(1./240.,c.enemy_x-2.2,c.enemy_z);
    c.impact_pause_enabled=false;return c;
}
static void tick(WorldTrainingCombat &c,double dt,double distance=2.2){c.update(dt,c.enemy_x-distance,c.enemy_z);}

int main(){
    // Every attack still has a real anticipation, one contact and a committed
    // recovery. Verify the measured response, rather than its configured value.
    for(unsigned d=0;d<5;++d){
        auto c=ready();c.enemy_phase=E::Recovery;c.enemy_duration=100;c.choose(D(d));
        CHECK(c.attack()&&c.hits==0&&c.enemy_health==90);
        double elapsed=0,contact=-1;
        for(unsigned frame=0;frame<240&&c.player_phase!=P::Ready;++frame){
            CHECK(!c.attack());
            tick(c,1./240.);elapsed+=1./240.;
            if(c.hits&&contact<0)contact=elapsed;
            CHECK(c.hits<=1&&c.guarded==0&&c.enemy_hits==0);
        }
        const double expected=d==unsigned(D::Thrust)?.17:.20;
        CHECK(contact>=expected-.0001&&contact<expected+.009);
        CHECK(elapsed>=expected+.35-.0001&&elapsed<expected+.35+.009);
        CHECK(c.player_phase==P::Ready&&c.hits==1&&c.enemy_health==75);
    }

    // Body contact, steel contact and air swings are separate source events.
    // A blocked blade cannot damage a body or emit a body-hit counter.
    for(unsigned d=0;d<5;++d){
        auto body=ready();body.enemy_phase=E::Recovery;body.enemy_duration=100;
        body.enemy_guard=D(d);body.choose(D(d));CHECK(body.attack());tick(body,.25);
        CHECK(body.hits==1&&body.guarded==0&&body.misses==0&&body.enemy_health==75);
        CHECK(std::strstr(body.feedback,"BODY HIT")&&!std::strstr(body.feedback,"STEEL"));
        auto clash=ready();clash.enemy_phase=E::Measure;clash.enemy_duration=100;
        clash.enemy_guard=D(d);clash.choose(D(d));CHECK(clash.attack());tick(clash,.25);
        CHECK(clash.guarded==1&&clash.misses==1&&clash.hits==0&&clash.enemy_health==90);
        CHECK(std::strstr(clash.feedback,"STEEL CLASH")&&!std::strstr(clash.feedback,"BODY HIT"));
        auto air=ready();air.enemy_phase=E::Measure;air.enemy_duration=100;
        air.enemy_guard=D(d);air.choose(D(d));CHECK(air.attack());tick(air,.25,4);
        CHECK(air.misses==1&&air.hits==0&&air.guarded==0&&air.enemy_health==90);

        auto parry=ready();parry.enemy_phase=E::Windup;parry.enemy_duration=.12;
        parry.enemy_direction=D(d);parry.choose(D(d));parry.set_guard(true);tick(parry,.14);
        CHECK(parry.parries==1&&parry.blocks==0&&parry.enemy_hits==0&&parry.health==100);
        CHECK(std::strstr(parry.feedback,"STEEL PARRY")&&!std::strstr(parry.feedback,"BODY HIT"));
        auto block=ready();block.enemy_phase=E::Windup;block.enemy_duration=.12;
        block.enemy_direction=D(d);block.choose(D(d));block.set_guard(true);block.guard_age=1;tick(block,.14);
        CHECK(block.blocks==1&&block.parries==0&&block.enemy_hits==0&&block.health==100);
        CHECK(std::strstr(block.feedback,"STEEL BLOCK")&&!std::strstr(block.feedback,"BODY HIT"));
        auto wrong=ready();wrong.enemy_phase=E::Windup;wrong.enemy_duration=.12;
        wrong.enemy_direction=D(d);wrong.choose(D((d+1)%5));wrong.set_guard(true);tick(wrong,.14);
        CHECK(wrong.enemy_hits==1&&wrong.blocks==0&&wrong.parries==0&&wrong.health==82);
        CHECK(std::strstr(wrong.feedback,"BODY HIT")&&!std::strstr(wrong.feedback,"STEEL"));
        auto broken=ready();broken.enemy_phase=E::Windup;broken.enemy_duration=.12;
        broken.enemy_direction=D(d);broken.choose(D(d));broken.stamina=15;broken.set_guard(true);broken.guard_age=1;tick(broken,.14);
        CHECK(broken.enemy_hits==1&&broken.blocks==0&&broken.parries==0&&broken.health==82&&!broken.guard);
        CHECK(std::strstr(broken.feedback,"GUARD BROKEN - BODY HIT"));
    }

    // The .24-second late guard window remains. Moving a held guard does not
    // rearm it or manufacture a new perfect-parry opportunity.
    CHECK(WorldTrainingCombat::parry_window_seconds==.24);
    auto late=ready();late.enemy_phase=E::Windup;late.enemy_duration=.22;
    late.enemy_direction=D::Right;late.choose(D::Right);late.set_guard(true);tick(late,.24);
    CHECK(late.parries==1&&late.enemy_hits==0);
    auto held=ready();held.enemy_phase=E::Windup;held.enemy_duration=.28;
    held.enemy_direction=D::Right;held.choose(D::Left);held.set_guard(true);tick(held,.10);
    held.choose(D::Right);held.set_guard(true);tick(held,.20);
    CHECK(held.parries==0&&held.blocks==1&&held.enemy_hits==0);

    // Arena enemies close distance before starting a telegraph. Ordinary story
    // encounters retain their original ranged miss behavior.
    auto ranged=ready();
    for(unsigned frame=0;frame<240*4;++frame){tick(ranged,1./240.,8);CHECK(ranged.enemy_phase==E::Measure&&ranged.enemy_swings==0&&ranged.enemy_hits==0);}
    double first_telegraph=-1;
    for(unsigned frame=0;frame<120;++frame){tick(ranged,1./240.,2.2);if(ranged.enemy_phase==E::Windup){first_telegraph=(frame+1)/240.;break;}}
    CHECK(first_telegraph>0&&first_telegraph<=.09);
    CHECK(ranged.enemy_duration>=.70&&ranged.enemy_duration<=.73);
    const auto locked_direction=ranged.enemy_direction,locked_guard=ranged.enemy_guard;
    while(ranged.enemy_phase==E::Windup){
        ranged.choose(D((unsigned(ranged.selected)+1)%5));tick(ranged,1./240.);
        CHECK(ranged.enemy_direction==locked_direction&&ranged.enemy_guard==locked_guard);
    }
    CHECK(ranged.enemy_hits==1&&ranged.health==82);

    // Faster enemies retain a readable windup and a useful counter window.
    auto cadence=ready();std::array<double,3> contacts={};unsigned prior=0;double time=0;
    for(unsigned frame=0;frame<240*10&&prior<3;++frame){
        tick(cadence,1./240.);time+=1./240.;
        if(cadence.enemy_hits>prior)contacts[prior++]=time;
    }
    CHECK(prior==3&&contacts[0]>1.3&&contacts[0]<1.42);
    for(unsigned i=1;i<3;++i)CHECK(contacts[i]-contacts[i-1]>=1.65&&contacts[i]-contacts[i-1]<1.94);

    // Measure a sustained real-hit loop: six responsive attacks are available,
    // followed by deliberate recovery; stamina never becomes an infinite chain.
    auto loop=ready();loop.enemy_phase=E::Recovery;loop.enemy_duration=100;
    loop.enemy_health=loop.enemy_max_health=10000;std::vector<double> starts;
    for(unsigned frame=0;frame<240*10;++frame){
        if(loop.player_phase==P::Ready&&loop.stamina>=16){loop.choose(D::Left);CHECK(loop.attack());starts.push_back(frame/240.);}
        tick(loop,1./240.);CHECK(loop.stamina>=0&&loop.stamina<=100);
    }
    CHECK(starts.size()>=11&&starts.size()<=13&&loop.hits>=11);
    CHECK(starts[5]<2.9&&starts[6]-starts[5]>.85);
    auto stopped=ready();stopped.impact_pause_enabled=true;stopped.enemy_phase=E::Recovery;stopped.enemy_duration=100;
    stopped.enemy_health=stopped.enemy_max_health=10000;std::vector<double> stopped_starts;
    for(unsigned frame=0;frame<240*10;++frame){
        if(stopped.player_phase==P::Ready&&stopped.stamina>=16){stopped.choose(D::Left);CHECK(stopped.attack());stopped_starts.push_back(frame/240.);}
        tick(stopped,1./240.);CHECK(stopped.stamina>=0&&stopped.stamina<=100);
    }
    CHECK(stopped_starts.size()>=11&&stopped_starts.size()<=13&&stopped.hits>=11);
    CHECK(stopped_starts[5]<3.1&&stopped_starts[6]-stopped_starts[5]>.85);
    auto resting=ready();resting.enemy_phase=E::Recovery;resting.enemy_duration=100;
    resting.stamina=0;resting.regen_delay=0;tick(resting,.25);tick(resting,.25);tick(resting,.12);
    CHECK(resting.stamina>=16&&resting.stamina<16.3);
    auto guarding=ready();guarding.enemy_phase=E::Recovery;guarding.enemy_duration=100;
    guarding.guard=true;guarding.stamina=0;guarding.regen_delay=0;tick(guarding,.25);tick(guarding,.25);tick(guarding,.12);
    CHECK(guarding.stamina>3.6&&guarding.stamina<3.9&&resting.stamina>4*guarding.stamina);
    std::cout<<std::fixed<<std::setprecision(3)<<"PASS combat response: cut .200/.100/.250 s; thrust .170/.100/.250 s; parry .240 s; enemy first contact "<<contacts[0]<<" s\n";
    std::cout<<"PASS stamina measurement: "<<starts.size()<<" attacks / 10 s; sixth starts "<<starts[5]<<" s; first recharge gap "<<starts[6]-starts[5]<<" s; rest restores16 in .620 s\n";
    std::cout<<"PASS with ordinary impact pauses: "<<stopped.hits<<" body contacts / 10 s; sixth starts "<<stopped_starts[5]<<" s; first recharge gap "<<stopped_starts[6]-stopped_starts[5]<<" s\n";
    std::cout<<"world_combat_feel: "<<checks<<" checks passed (all5 body/steel/air outcomes, readable telegraph, faster recovery, strict guard age, finite stamina)\n";
}
