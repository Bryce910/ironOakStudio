// CPU-only integration of the production population configuration/update path.
// Pose uploads and height sampling are explicit inert test adapters; no GL is used.
#include "engine/world_village_population.cpp"
#include <cstdlib>
#include <iostream>
#include <memory>
extern "C" WorldModelCharacterLook world_model_character_default(void){return {1,0,1,0,{1,1,1},0};}
extern "C" bool world_model_pose_blend_upload_character(WorldModelPoseBlend *,const WorldModelInstance *,const float *,const WorldModelCharacterLook *,std::size_t){return true;}
extern "C" float world_atlas_walk_height(double,double){return 10;}
static unsigned checks=0;
static void check(bool okay,const char *name){++checks;if(!okay){std::cerr<<"FAIL "<<checks<<": "<<name<<"\n";std::exit(1);}std::cout<<"PASS "<<checks<<": "<<name<<"\n";}
static WorldVillageActorConfig actor(std::uint64_t id,double first=0){WorldVillageActorConfig c;c.identity=id;c.appearance=3;c.speed=1.35f;c.dwell=1.2f;c.count=3;c.points[0]={first,10,0};c.points[1]={first+60,10,0};c.points[2]={first,10,0};return c;}
static void update(WorldVillagePopulation *p,float time){world_village_population_update(p,0,12,0,time,1.f/60);}
int main(){
 auto p=std::make_unique<WorldVillagePopulation>();world_village_population_stock_enabled(p.get(),false);
 check(world_village_population_stats(p.get()).residents==0,"stock-hidden library has zero stock residents");
 auto a=actor(1001);check(world_village_population_configure(p.get(),&a,1),"configure first authored patrol");
 // Production startup uses a time17 pose-only seed, while deterministic
 // captures use time22. Neither may move a new NPC away from its clicked root.
 world_village_population_update(p.get(),0,12,0,17,0);WorldVillageResident seeded;
 check(world_village_population_resident(p.get(),140,&seeded)&&std::abs(seeded.x)<.00001,"pose-only startup publishes first authored point");
 world_village_population_update(p.get(),0,12,0,22,0);
 check(world_village_population_resident(p.get(),140,&seeded)&&std::abs(seeded.x)<.00001,"time17 warmup to time22 fixed capture with dt0 cannot advance patrol");
 check(!p->route_clocks[0].initialized,"pose-only warmup does not establish the gameplay epoch");
 check(world_village_population_stats(p.get()).residents==1,"authored addition remains active with stock disabled");
 update(p.get(),1234);WorldVillageResident resident;check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x)<.00001,"new patrol starts at first point despite global time 1234");
 update(p.get(),1239);check(world_village_population_resident(p.get(),140,&resident)&&resident.x>5&&resident.x<7,"subsequent supplied visual time advances patrol");const double advanced=resident.x;
 check(world_village_population_configure(p.get(),&a,1),"reapply unchanged route");update(p.get(),1239);check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x-advanced)<.00001,"unrelated Apply preserves unchanged patrol phase");
 world_village_population_update(p.get(),0,12,0,1249,0);
 check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x-advanced)<.00001,"pose-only warm after Apply preserves visible route position");
 update(p.get(),1249);check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x-advanced)<.00001,"resume cannot catch up ten seconds spent in pose-only warmup");
 world_village_population_update(p.get(),0,12,0,1239,0);
 std::snprintf(a.name.data(),a.name.size(),"A renamed traveler");check(world_village_population_configure(p.get(),&a,1),"rename resident without replacing route");update(p.get(),1239);check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x-advanced)<.00001,"renaming preserves patrol phase");
 world_village_population_update(p.get(),0,12,0,1239,0);check(world_village_population_resident(p.get(),140,&resident)&&std::abs(resident.x-advanced)<.00001,"rename reapply pose-only pass preserves phase");
 auto b=actor(1002,20);WorldVillageActorConfig pair[]={a,b};check(world_village_population_configure(p.get(),pair,2),"add a second independent patrol");update(p.get(),1240);WorldVillageResident second;check(world_village_population_resident(p.get(),141,&second)&&std::abs(second.x-20)<.00001,"second patrol starts at its own first point");update(p.get(),1244);check(world_village_population_resident(p.get(),141,&second),"read second resident before compaction");const double second_advanced=second.x;
 check(world_village_population_configure(p.get(),&b,1),"delete earlier patrol and compact slots");update(p.get(),1244);check(world_village_population_resident(p.get(),140,&second)&&std::abs(second.x-second_advanced)<.00001,"durable identity preserves phase through deletion/compaction");
 b.points[1].x=90;check(world_village_population_configure(p.get(),&b,1),"change authored walking route");update(p.get(),1300);check(world_village_population_resident(p.get(),140,&second)&&std::abs(second.x-20)<.00001,"changed patrol restarts at its first point");
 update(p.get(),1305);check(world_village_population_resident(p.get(),140,&second),"read moving changed patrol");const double before_rebase=second.x;update(p.get(),20);check(world_village_population_resident(p.get(),140,&second)&&std::abs(second.x-before_rebase)<.00001,"backwards visual time cannot rewind or teleport patrol");
 b.enabled=false;b.appearance=1;check(world_village_population_configuration_valid(&b,1),"disabled idle-only actor may retain dormant patrol points");check(world_village_population_configure(p.get(),&b,1),"configure dormant idle-only actor");update(p.get(),25);check(!world_village_population_resident(p.get(),140,&second)&&world_village_population_stats(p.get()).residents==0,"disabled actor neither renders nor counts");
 b.enabled=true;check(!world_village_population_configuration_valid(&b,1),"enabled idle-only patrol rejected before mutation");check(world_village_population_stats(p.get()).residents==0,"rejected configuration preserves current population");
 auto override_actor=actor(1003);override_actor.source=0;override_actor.count=1;check(world_village_population_configure(p.get(),&override_actor,1),"configure explicit stock source override in isolated scene");update(p.get(),60);check(world_village_population_resident(p.get(),0,&resident)&&world_village_population_stats(p.get()).residents==1,"explicit source override visible while remaining 139 stock residents hidden");check(!world_village_population_resident(p.get(),1,&resident),"unmodified stock resident stays unavailable");
 world_village_population_stock_enabled(p.get(),true);check(world_village_population_stats(p.get()).residents==140,"stock crowd can be enabled independently");world_village_population_stock_enabled(p.get(),false);for(unsigned i=0;i<140;++i)world_village_population_runtime_active(p.get(),i,true);update(p.get(),61);check(world_village_population_stats(p.get()).visible==1,"live-visual suppression reset cannot reenable stock crowd");
 auto invalid=override_actor;invalid.speed=std::numeric_limits<float>::quiet_NaN();check(!world_village_population_configure(p.get(),&invalid,1),"nonfinite route speed rejected atomically");check(world_village_population_stats(p.get()).residents==1,"invalid route preserves stock visibility and active authored resident");
 auto same=actor(1004);WorldVillageActorConfig duplicate[]={same,same};check(!world_village_population_configuration_valid(duplicate,2),"duplicate durable route identities rejected");
 std::cout<<"PASS total="<<checks<<" production population checks\n";
}
