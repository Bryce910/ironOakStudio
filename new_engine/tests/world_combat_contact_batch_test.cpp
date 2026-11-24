#include "engine/world_blood_effects.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    using namespace world_combat_vfx;
    WorldTrainingCombat fight;fight.state=TrainingState::Fighting;fight.clock=1;
    Input input;input.resolved_events=true;input.feet[0]={0,0,0};input.feet[1]={0,0,2};
    input.contacts[0]={10,0,{-2,1.2,2},{-2,0,2},{1,0,0},1,false};
    input.contacts[1]={11,0,{2,1.2,2},{2,0,2},{-1,0,0},1.5f,true};
    input.contacts[2]={12,1,{0,1.4,1},{0,0,0},{0,0,1},1,false};input.contact_count=3;
    System particles;particles.update(fight,input,0);
    auto stats=particles.stats();assert(stats.hits==2&&stats.parries==1&&stats.body_bursts==2&&stats.metal_bursts==1&&stats.finisher_bursts==1);
    assert(stats.last_body_contacts[1].x==2);
    particles.update(fight,input,0);assert(particles.stats().body_bursts==2&&particles.stats().parries==1);
    input.contacts[2].serial=13;input.paused=true;particles.update(fight,input,.2);assert(particles.stats().parries==1);
    input.paused=false;particles.update(fight,input,0);assert(particles.stats().parries==2);
    // Every attacking enemy has its own ribbon history. A fresh combo swing
    // resets its previous blade rather than bridging two unrelated clips.
    input.contact_count=0;
    for(unsigned i=0;i<9;++i)input.actor_trails[i]={{{double(i),1,0},{double(i),1,1},true},true,1};
    particles.update(fight,input,.01);
    for(auto &t:input.actor_trails)t.blade.tip.x+=.15;
    particles.update(fight,input,.01);assert(particles.stats().active_trails==9);
    const unsigned segments=particles.stats().trail_segments;
    for(auto &t:input.actor_trails){++t.swing;t.blade.tip.x+=.15;}
    particles.update(fight,input,.01);assert(particles.stats().trail_segments==segments);

    world_blood_effects::System blood;world_blood_effects::Config config;config.persistent_stains=true;assert(blood.configure(config));
    world_blood_effects::ResolvedEvent events[3]={
        {10,1,{{-2,1.2,2},{1,0,0},.032,true,1,false}},
        {11,1,{{2,1.2,2},{-1,0,0},.032,true,1.5f,true}},
        {12,0,{{0,1.2,0},{0,0,-1},.032,true,1,false}}
    };
    world_blood_effects::Input body;body.resolved_events=true;body.events=events;body.event_count=3;body.combat_clock=1;
    assert(blood.update(body,0));auto stage=blood.stage();assert(stage.mesh_count==6&&stage.stain_count==3);
    assert(stage.stains[0].position.x==-2&&stage.stains[1].position.x==2&&stage.stains[2].defender==0);
    assert(blood.stats().outgoing_events==2&&blood.stats().incoming_events==1&&blood.stats().finisher_events==1);
    assert(blood.update(body,0)&&blood.stats().impacts_spawned==3);
    body.paused=true;events[2].serial=13;assert(blood.update(body,2)&&blood.stats().impacts_spawned==3);
    body.paused=false;assert(blood.update(body,0)&&blood.stats().impacts_spawned==4);
    body.event_count=0;for(unsigned i=0;i<600;++i)assert(blood.update(body,1./60.));
    stage=blood.stage();assert(stage.mesh_count==0&&stage.stain_count==4);
    assert(stage.stains[0].position.x==-2&&stage.stains[1].position.x==2&&stage.stains[0].position.y==.032);
    // Bound concurrent bursts without allowing one enemy's hit to relocate a
    // surviving stain from another enemy.
    world_blood_effects::ResolvedEvent burst[64];
    for(unsigned i=0;i<64;++i)burst[i]={100+i,1,{{double(i),1,3},{1,0,0},.032,true}};
    body.events=burst;body.event_count=64;assert(blood.update(body,0));
    assert(blood.stats().limited_events==48&&blood.stage().mesh_count<=8&&blood.stage().stain_count==20);
    assert(blood.stage().stains[0].position.x==-2);
    blood.reset();body.events=events;body.event_count=3;assert(blood.update(body,0)&&blood.stage().stain_count==3);
    std::puts("PASS multi-opponent contact batches: distinct contacts, exactly-once serials, pause, nine ribbon histories, combo resets, persistent anchored stains, bounded bursts");
}
