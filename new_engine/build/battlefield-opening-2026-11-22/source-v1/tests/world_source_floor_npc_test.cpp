#include "engine/world_studio.h"
#include "engine/world_authoring_identity.h"
#include "engine/world_designer.h"
#include "engine/world_prefab.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
static double ground(double,double,const void *){return 10;}
int main(){
    world_studio::Document d;world_studio::Actor a;a.name="Upstairs";a.points={{{1,2}},{{6,2}}};a.floor_heights={104,106};d.actors.push_back(a);
    world_studio::NavigationArea n;n.x=2;n.z=2;n.source_floor=true;n.floor_y=104;d.navigation.push_back(n);d.actors[0].navigation=0;
    std::string text,error;assert(world_studio::encode(d,text,error)&&text.rfind("VALWOOD_STUDIO 11\n",0)==0);
    assert(text.find("actor_floors 0 2 104 106")!=std::string::npos&&text.find("navigation_floor 0 104")!=std::string::npos);
    const auto file=std::filesystem::temp_directory_path()/("valwood-source-floors-"+std::to_string(getpid())+".studio");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove(p,ec);}}cleanup{file};
    assert(world_studio::save(file.string().c_str(),d,error));world_studio::Document loaded;assert(world_studio::load(file.string().c_str(),loaded,error));
    assert(loaded.actors[0].floor_heights==a.floor_heights&&loaded.navigation[0].source_floor&&loaded.navigation[0].floor_y==104);
    std::string roundtrip;assert(world_studio::encode(loaded,roundtrip,error)&&text==roundtrip);
    {std::ofstream out(file);out<<text<<"actor_floors 0 2 104 106\n";}assert(!world_studio::load(file.string().c_str(),loaded,error)&&loaded.actors[0].floor_heights==a.floor_heights);
    auto invalid=d;invalid.actors[0].floor_heights.pop_back();assert(!world_studio::valid(invalid,error));invalid=d;invalid.navigation[0].floor_y=INFINITY;assert(!world_studio::valid(invalid,error));
    auto legacy=d;legacy.actors[0].floor_heights.clear();legacy.navigation[0].source_floor=false;assert(world_studio::encode(legacy,roundtrip,error)&&roundtrip.rfind("VALWOOD_STUDIO 6\n",0)==0&&roundtrip.find("actor_floors")==std::string::npos);
    world_entity::Graph graph;std::vector<world_authoring::Source> sources;assert(world_authoring::synchronize(d,graph,sources,ground,nullptr,error));d.entities=graph;
    world_entity::Id actor_id=0;for(const auto &node:graph.nodes)if(node.binding==world_entity::Binding{world_entity::Kind::Actor,0})actor_id=node.id;
    world_entity::Transform pose;assert(actor_id&&world_entity::world_transform(graph,actor_id,pose,error)&&pose.y==104);
    world_designer::Context context;context.ground={ground,nullptr};assert(world_designer::world_pose(d,context,world_designer::Ref::entity(actor_id),pose,error)&&pose.y==104);
    world_prefab::Prefab prefab;world_prefab::Selection selection;selection.objects={{world_entity::Kind::Actor,0},{world_entity::Kind::Navigation,0}};selection.ground={ground,nullptr};
    assert(world_prefab::capture(d,selection,{1,104,2,0,1},"Upstairs patrol",prefab,error));
    world_studio::Document target;world_entity::Graph target_graph;world_prefab::Instance instance;
    auto at=prefab.origin;at.x+=20;at.y+=3;assert(world_prefab::instantiate(prefab,target,target_graph,at,0,instance,error,{ground,nullptr}));
    assert(target.actors[0].floor_heights[0]==107&&target.actors[0].floor_heights[1]==109&&target.navigation[0].source_floor&&target.navigation[0].floor_y==107);
    assert(world_authoring::synchronize(target,target_graph,sources,ground,nullptr,error));
    std::puts("PASS source-floor persistence: conditional v11, per-point/area storeys, exact save/reopen, duplicate/malformed rejection, legacy bytes, identity, designer pose, prefab copy/translation");
}
