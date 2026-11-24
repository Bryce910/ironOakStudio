#ifndef VALWOOD_POPULATION_RUNTIME_CASES_H
#define VALWOOD_POPULATION_RUNTIME_CASES_H
#include "engine/world_village_population.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
// Shared by native validation and a CPU-stub fixture. No model loads or draw
// calls: all changes use the already resident pose batches.
inline void check_authored_population_runtime(WorldVillagePopulation *p,double ox,double oz){
    std::array<WorldVillageActorConfig,3> config={};
    for(unsigned i=0;i<config.size();i++){config[i].count=1;config[i].points[0]={ox-3+i*3,24,oz-60};std::snprintf(config[i].name.data(),config[i].name.size(),"Runtime actor %u",i);}
    config[0].source=0;config[0].appearance=0;
    config[1].appearance=0;
    config[2].source=8;config[2].appearance=1;
    assert(world_village_population_configure(p,config.data(),config.size()));
    WorldVillageActorRuntime source{ox-3,29.125,oz-60,1.25f,1.2f,true};
    WorldVillageActorRuntime added{ox+3,27.75,oz-60,-.75f,1.2f,true};
    WorldVillageActorRuntime idle{ox+6,31.5,oz-60,0,0,true};
    assert(world_village_population_actor_runtime(p,0,&source));
    assert(world_village_population_actor_runtime(p,1,&added));
    assert(world_village_population_actor_runtime(p,2,&idle));
    assert(!world_village_population_actor_runtime(nullptr,0,&source));
    assert(!world_village_population_actor_runtime(p,3,&source)&&!world_village_population_actor_runtime(p,32,&source));
    auto invalid=source;invalid.y=std::numeric_limits<double>::quiet_NaN();assert(!world_village_population_actor_runtime(p,0,&invalid));invalid=source;invalid.speed=-1;assert(!world_village_population_actor_runtime(p,0,&invalid));
    idle.speed=.1f;assert(!world_village_population_actor_runtime(p,2,&idle));idle.speed=0;
    float time=101;world_village_population_update(p,ox,30,oz-60,time,1.f/60);
    WorldVillageResident a,b,c;assert(world_village_population_resident(p,0,&a)&&world_village_population_resident(p,140,&b)&&world_village_population_resident(p,8,&c));
    assert(a.x==source.x&&a.z==source.z&&a.y==float(source.y)&&a.yaw==source.yaw);
    assert(b.x==added.x&&b.y==float(added.y)&&c.y==float(idle.y)&&!c.walking);
    for(unsigned i=0;i<40;i++){source.x+=1.2/60;added.x+=1.2/60;time+=1.f/60;assert(world_village_population_actor_runtime(p,0,&source)&&world_village_population_actor_runtime(p,1,&added));world_village_population_update(p,ox,30,oz-60,time,1.f/60);}
    assert(world_village_population_resident(p,0,&a)&&a.walking&&!a.transitioning);
    const float phase=a.cycle_phase;
    source.x+=world_village_walk_stride_m[0]*.25;time+=.25f;assert(world_village_population_actor_runtime(p,0,&source));world_village_population_update(p,ox,30,oz-60,time,.25f);
    assert(world_village_population_resident(p,0,&a));assert(std::abs(std::remainder(a.cycle_phase-phase-.25f,1.f))<.00002f);
    const float fixed_phase=a.cycle_phase;time+=50;world_village_population_update(p,ox,30,oz-60,time,1.f/60);assert(world_village_population_resident(p,0,&a)&&a.cycle_phase==fixed_phase);
    source.speed=0;assert(world_village_population_actor_runtime(p,0,&source));world_village_population_update(p,ox,30,oz-60,time,1.f/60);assert(world_village_population_resident(p,0,&a)&&!a.walking&&a.transitioning);
    for(unsigned i=0;i<40;i++){time+=1.f/60;world_village_population_update(p,ox,30,oz-60,time,1.f/60);}
    assert(world_village_population_resident(p,0,&a)&&!a.walking&&!a.transitioning&&a.pose_a>=8&&a.pose_a<12);
    source.speed=1.2f;for(unsigned i=0;i<4;i++){source.x+=1.2/60;time+=1.f/60;assert(world_village_population_actor_runtime(p,0,&source));world_village_population_update(p,ox,30,oz-60,time,1.f/60);}
    assert(world_village_population_resident(p,0,&a)&&a.transitioning);
    const auto pair_a=a.pose_a,pair_b=a.pose_b;const float reverse_weight=a.pose_blend;
    source.speed=0;assert(world_village_population_actor_runtime(p,0,&source));world_village_population_update(p,ox,30,oz-60,time,0);
    assert(world_village_population_resident(p,0,&a)&&a.pose_a==pair_a&&a.pose_b==pair_b&&a.pose_blend==reverse_weight);
    const auto visible=world_village_population_stats(p).visible;
    source.active=added.active=false;assert(world_village_population_actor_runtime(p,0,&source)&&world_village_population_actor_runtime(p,1,&added));world_village_population_update(p,ox,30,oz-60,time,0);
    assert(!world_village_population_resident(p,0,&a)&&!world_village_population_resident(p,140,&b));assert(world_village_population_stats(p).visible+2==visible);
    assert(world_village_population_actor_runtime(p,0,nullptr));world_village_population_clear_runtime(p);world_village_population_update(p,ox,30,oz-60,time,0);
    assert(world_village_population_resident(p,0,&a)&&world_village_population_resident(p,140,&b)&&a.x==config[0].points[0].x&&a.y!=float(source.y));
    source.active=true;assert(world_village_population_actor_runtime(p,0,&source));assert(world_village_population_runtime_active(p,1,false));
    assert(world_village_population_configure(p,config.data(),config.size()));world_village_population_update(p,ox,30,oz-60,time,0);
    assert(world_village_population_resident(p,0,&a)&&a.y!=float(source.y));const auto suppressed=world_village_population_stats(p).visible;
    assert(world_village_population_runtime_active(p,1,true));world_village_population_update(p,ox,30,oz-60,time,0);assert(world_village_population_stats(p).visible==suppressed+1);
    assert(world_village_population_configure(p,nullptr,0));world_village_population_update(p,ox,25.75,oz-65,17,0);
    std::puts("PASS authored runtime: exact source/added/idle roots and feet, distance/stride gait, planted stop and reversible blends, inactive/query suppression, invalid rollback, clear/rebind and base suppression preservation");
}
#endif
