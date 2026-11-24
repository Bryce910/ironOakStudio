#include "engine/world_designer.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace world_designer;
struct Studio {
    world_studio::Document draft;world_studio::History history;
    std::vector<world_studio::Pose> source_poses;std::vector<world_authoring::Source> identity_sources;
    std::string signature="0123456789abcdef",message;bool components_ready=false,dirty=false,rebuild=false;
    struct Reference {Id entity=0;};struct {std::vector<Reference> physics,living;}components_active_document;
    struct {struct Entity {Id id=0;};std::vector<Entity> entities;}gameplay_document;
    std::vector<unsigned> selection_keys;Id entity_selected=0,production_id_highwater=1;std::uint64_t entity_world_revision=0,revision=0;
    int selected=-1,selected_added=-1,light=-1,actor_selection=-1,area_selection=-1,particle_selection=-1,water_selection=-1,tab=2;
    static double production_ground(double,double,const void *){return 10;}
    bool production_freeze()const{return false;}
    bool components_scene_prevalidate(const world_studio::Document &,std::string &)const{return true;}
    void changed(bool geometry=false){dirty=true;rebuild|=geometry;revision++;}
    void designer_preview_object_edit(){}
    #include "engine/world_studio_designer_selection.inc"
};
template<class F>static double ms(unsigned count,F &&f){const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<count;i++)f(i);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/count;}
static void dense_components(world_studio::Document &d){
    d.source_signature="0123456789abcdef";
    for(unsigned i=0;i<512;i++)d.overrides.push_back({i,double(i),0,0,1,false,0});
    d.lights.resize(24);d.stamps.resize(32);d.decals.resize(32);d.particles.resize(16);d.navigation.resize(4);d.actors.resize(32);d.waters.resize(8);
    for(auto &n:d.navigation)n.span=8;
    for(auto &a:d.actors)a.points={{{0,0}}};
    for(auto &w:d.waters)w.source=3;
    for(unsigned i=0;i<64;i++){d.materials.push_back({"material_"+std::to_string(i)});d.assets.push_back({"asset_"+std::to_string(i),"models/stem_"+std::to_string(i)});}
    for(unsigned i=0;i<32;i++)d.groups.push_back({"Selection group "+std::to_string(i),{i}});
    for(unsigned i=0;i<16;i++){world_studio::Spline s;s.alias="starting_crate";s.points={{{0,0}},{{3,0}}};d.splines.push_back(s);}
    for(unsigned i=0;i<8;i++){auto graph=world_graph::preset("graph_"+std::to_string(i),0);graph.nodes.clear();graph.outputs.fill(-1);for(unsigned n=0;n<32;n++){world_graph::Node node;node.id=n;graph.nodes.push_back(node);}d.graphs.push_back(std::move(graph));}
    auto &s=d.surfaces;
    for(unsigned i=0;i<16;i++)s.images.push_back({"Image "+std::to_string(i),"surfaces/fixture_"+std::to_string(i)+".png"});
    for(unsigned i=0;i<8;i++){world_surface::Layer layer;layer.name="Layer "+std::to_string(i);s.layers.push_back(layer);}
    for(unsigned i=0;i<64;i++){world_surface::Binding b;b.alias="surface_"+std::to_string(i);b.first=0;s.bindings.push_back(b);}
    s.paint.resize(256);s.projections.resize(64);s.winds.resize(16);
    for(unsigned i=0;i<16;i++){world_surface::Ribbon r;r.points={{0,0,0},{3,0,0}};s.ribbons.push_back(r);}
}
int main(int argc,char **){
    Studio studio;studio.source_poses.resize(7777);studio.identity_sources.resize(7777);
    for(unsigned i=0;i<7777;i++){auto &p=studio.source_poses[i];p.alias="starting_crate";p.root={double(i)*3,10,0};p.minimum={-.5,0,-.5};p.maximum={.5,1,.5};studio.identity_sources[i]={p.alias,"village.txt",p.root.x,0,10,0,1};}
    auto &d=studio.draft;for(unsigned i=0;i<16;i++){d.terrain.push_back({});d.terrain.back().x=int(i);}
    for(unsigned i=0;i<512;i++)d.placements.push_back({"starting_crate",double(i),0});
    for(unsigned i=0;i<1024;i++)d.entities.nodes.push_back({Id(i+1),0,"Marker "+std::to_string(i),{}, {}});
    if(argc>1)dense_components(d);
    d.entities.next_id=1025;std::string error;if(!world_studio::valid(d,error)){std::cerr<<error;return 1;}
    if(!studio.designer_select(Ref::source_model(0))){std::cerr<<studio.message;return 1;}
    volatile double sink=0;
    const auto lazy=ms(400,[&](unsigned){const auto context=studio.designer_context();auto n=name(d,context,studio.designer_selection.active);world_entity::Transform p;if(!world_pose(d,context,studio.designer_selection.active,p,error))std::abort();auto projected=project(d,context,studio.designer_selection);sink+=p.x+n.size()+projected.selected;});
    if(!studio.designer_select(Ref::entity(1024)))return 1;
    const auto entity=ms(400,[&](unsigned){const auto context=studio.designer_context();auto n=name(d,context,studio.designer_selection.active);world_entity::Transform p;if(!world_pose(d,context,studio.designer_selection.active,p,error))std::abort();auto projected=project(d,context,studio.designer_selection);sink+=p.x+n.size()+projected.entity;});
    studio.designer_begin_edit();
    const auto gesture=ms(200,[&](unsigned i){Command command;command.operation=Operation::Rename;command.name="Marker edited "+std::to_string(i);if(!studio.designer_update_edit(command)){std::cerr<<studio.message;std::abort();}});
    if(!studio.designer_finish_edit()||studio.history.back.size()!=1)return 1;
    // Keep one spare graph slot so the old implementation must take its full
    // temporary-prefab path, rather than its special graph-full leaf fallback.
    studio.draft.entities.nodes.pop_back();studio.draft.entities.nodes[0].binding={world_entity::Kind::Placement,0};studio.draft.entities.nodes[0].local={0,10,0,0,1};
    if(!studio.designer_select(Ref::entity(1)))return 1;
    studio.designer_begin_edit();
    const auto leaf=ms(100,[&](unsigned i){Command command;command.operation=Operation::Transform;command.transform={double(i)*.02,10,0,0,1};if(!studio.designer_update_edit(command)){std::cerr<<studio.message;std::abort();}});
    if(!studio.designer_finish_edit()||studio.history.back.size()!=2)return 1;
    std::cout<<"{\"sources\":7777,\"entities\":1024,\"placements\":512,\"terrain_tiles\":16,\"dense_components\":"<<(argc>1?"true":"false")<<",\"lazy_inspector_ms\":"<<lazy<<",\"entity_inspector_ms\":"<<entity<<",\"gesture_update_ms\":"<<gesture<<",\"leaf_transform_ms\":"<<leaf<<",\"undo_steps\":"<<studio.history.back.size()<<",\"sink\":"<<sink<<"}\n";
}
