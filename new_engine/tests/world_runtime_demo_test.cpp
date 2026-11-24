#include "engine/world_runtime_demo.h"
#include <cmath>
#include <iostream>
using namespace world_runtime_demo;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
int main(){
    std::string error;world_living::World living;Sites sites;sites.castle_x=1000;sites.castle_z=2000;sites.jungle_x=-10000;sites.jungle_z=8000;sites.jungle_floor=120;Seed seed;
    CHECK(build(sites,{},living,true,1000,seed,error));CHECK(seed.initialized_living&&seed.visuals.size()==8&&seed.markers.nodes.size()==11&&living.definition().actors.size()==6);CHECK(seed.configuration.components.living.size()==6&&seed.configuration.components.physics.size()==2);CHECK(seed.visuals[0].living_actor==920001&&seed.visuals[0].entity==castle_actor_first&&seed.visuals[4].entity==jungle_actor_first);CHECK(seed.visuals[5].asset=="knight");
    world_runtime::Runtime runtime;CHECK(runtime.configure(seed.configuration,error));world_runtime::Frame frame;CHECK(runtime.tick(.016,1000,{sites.castle_x-42.5,24,sites.castle_z-68},frame));world_runtime::Id selected=0;CHECK(runtime.interact_nearest({sites.castle_x-42.5,24,sites.castle_z-68},3.25,selected)&&selected==instructor_id);CHECK(runtime.tick(.016,1000,{sites.castle_x-42.5,24,sites.castle_z-68},frame));CHECK(runtime.current_line()&&runtime.choices().count==2);CHECK(runtime.choose(0));CHECK(runtime.tick(.016,1000,{sites.castle_x-42.5,24,sites.castle_z-68},frame));bool training=false;for(unsigned i=0;i<frame.requests.count;i++)training|=frame.requests.values[i].kind==world_gameplay::CommandKind::StartTraining&&frame.requests.values[i].key==training_encounter_id&&frame.requests.values[i].entity==arena_id;CHECK(training);CHECK(frame.motion_count==6);
    CHECK(runtime.interact_nearest({sites.jungle_x-5,120,sites.jungle_z+4},3.25,selected)&&selected==jungle_actor_first);CHECK(runtime.tick(.016,1000,{sites.jungle_x-5,120,sites.jungle_z+4},frame));CHECK(runtime.current_line()&&runtime.current_line()->text.find("wayfarers")!=std::string::npos);
    world_runtime::Entity crate;for(unsigned i=0;i<200;i++)CHECK(runtime.tick(1./60.,1000+i/60.,{},frame));CHECK(runtime.entity(jungle_crate_id,crate)&&std::abs(crate.pose.position.y-sites.jungle_floor)<.02);CHECK(runtime.entity(castle_crate_id,crate)&&std::abs(crate.pose.position.y-sites.castle_floor)<.02);
    // Reusing a configured world preserves every resource, actor and clock.
    const auto original=living.state();Seed reused;CHECK(build(sites,{},living,false,9000,reused,error));CHECK(!reused.initialized_living&&living.state().tick==original.tick&&living.state().last_wall_milliseconds==original.last_wall_milliseconds&&living.state().settlements[0].stock==original.settlements[0].stock);CHECK(reused.visuals[0].living_actor==living.definition().actors[0].id);CHECK(!build(sites,{},living,true,9000,reused,error));
    // Full authoring scenes must leave room for the built-in playable journey.
    std::vector<world_runtime::Entity> full(world_entity::node_limit);for(unsigned i=0;i<full.size();i++)full[i].id=i+1;
    Seed capacity_seed;CHECK(build(sites,full,living,false,9000,capacity_seed,error));CHECK(capacity_seed.configuration.entities.size()==full.size()+11);
    CHECK(presentation(sites,capacity_seed,120,error));world_runtime::Runtime full_runtime;CHECK(full_runtime.configure(capacity_seed.configuration,error));
    world_runtime::Frame full_frame;full_runtime.read_frame(full_frame);CHECK(full_frame.entity_count==full.size()+13);
    full.resize(world_runtime::entity_limit);for(unsigned i=0;i<full.size();i++)full[i].id=i+1;
    CHECK(!build(sites,full,living,false,9000,capacity_seed,error));CHECK(error.find("capacity")!=std::string::npos);
    auto definition=living.definition();definition.actors[1].name=definition.actors[0].name;CHECK(living.configure(definition,1000,error));CHECK(build(sites,{},living,false,1000,reused,error));CHECK(reused.configuration.components.living.size()==4);for(const auto &v:reused.visuals)if(v.living_actor)CHECK(v.name!=definition.actors[0].name);
    world_runtime::Entity conflict;conflict.id=instructor_id;CHECK(!build(sites,{conflict},living,false,1000,reused,error));CHECK(living.state().tick==0);std::cout<<"world_runtime_demo: "<<checks<<" fixed-ID scene/real crate support/instructor training/jungle dialogue/persistent world preservation contracts\n";
}
