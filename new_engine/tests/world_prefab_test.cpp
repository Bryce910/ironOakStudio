#include "engine/world_prefab.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <unistd.h>
namespace {
bool near(double a,double b){return std::abs(a-b)<1e-4;}
double flat(double,double,const void *p){return *static_cast<const double *>(p);}
std::string read(const char *path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::string doc_bytes(const world_studio::Document &d){const std::string p="/tmp/valwood-prefab-test-document-"+std::to_string(getpid());std::string e;assert(world_studio::save(p.c_str(),d,e));auto s=read(p.c_str());unlink(p.c_str());return s;}
std::string graph_bytes(const world_entity::Graph &g){std::string s,e;assert(world_entity::encode(g,s,e));return s;}
}
int main(){
 using namespace world_studio;using namespace world_entity;using namespace world_prefab;std::string e;
 Document source;Placement tavern;tavern.alias="tavern";tavern.x=100;tavern.z=200;tavern.offset_y=1;tavern.yaw=10;source.placements.push_back(tavern);
 Light lantern;lantern.x=102;lantern.y=12;lantern.z=201;lantern.yaw=30;source.lights.push_back(lantern);
 NavigationArea nav;nav.name="Tavern court";nav.x=103;nav.z=202;nav.span=32;nav.cell=2;source.navigation.push_back(nav);
 Actor actor;actor.name="Innkeeper";actor.source=8;actor.navigation=0;actor.points={{{101,201}},{{104,201}},{{104,204}}};source.actors.push_back(actor);
 Particle fire;fire.source=5;fire.kind=2;fire.x=103;fire.y=11;fire.z=202;fire.ceiling=30;fire.height=2;fire.radius=.4f;fire.light=3;source.particles.push_back(fire);
 Water pond;pond.source=3;pond.x=105;pond.z=206;pond.level=9;pond.radius_x=3;pond.radius_z=5;source.waters.push_back(pond);
 Asset asset;asset.alias="tavern";asset.stem="imports/tavern/revision";source.assets.push_back(asset);Material material;material.alias="tavern";material.normal=.8f;source.materials.push_back(material);source.graphs.push_back(world_graph::preset("tavern",0));
 assert(world_studio::valid(source,e));const auto original=doc_bytes(source);const double source_ground=10,target_ground=20;Ground sg{flat,&source_ground},tg{flat,&target_ground};
 Selection selection;selection.ground=sg;selection.objects={{Kind::Placement,0},{Kind::Light,0},{Kind::Actor,0},{Kind::Particle,0},{Kind::Water,0}};
 Prefab prefab;assert(capture(source,selection,{100,10,200,0,1},"Tavern with courtyard",prefab,e));assert(doc_bytes(source)==original);assert(prefab.content.navigation.size()==1&&prefab.content.assets.size()==1&&prefab.content.materials.size()==1&&prefab.content.graphs.size()==1);assert(prefab.content.actors[0].source==-1&&prefab.content.particles[0].source==-1);assert(prefab.graph.nodes.size()==7);
 const std::string path="/tmp/valwood-prefab-test-"+std::to_string(getpid())+".vwprefab";assert(save(path.c_str(),prefab,e));const auto file=read(path.c_str());Prefab loaded;assert(load(path.c_str(),loaded,e)&&loaded.name==prefab.name&&graph_bytes(loaded.graph)==graph_bytes(prefab.graph));assert(doc_bytes(loaded.content)==doc_bytes(prefab.content));
 Document dest;Graph graph;Instance result;const Transform target{300,20,400,90,1};assert(instantiate(loaded,dest,graph,target,0,result,e,tg));assert(dest.placements.size()==1&&dest.lights.size()==1&&dest.actors.size()==1&&dest.particles.size()==1&&dest.waters.size()==1&&dest.navigation.size()==1);assert(result.objects.size()==6);
 assert(near(dest.placements[0].x,300)&&near(dest.placements[0].z,400)&&near(dest.placements[0].offset_y,1)&&near(dest.placements[0].yaw,100));
 assert(near(dest.lights[0].x,301)&&near(dest.lights[0].z,398)&&near(dest.lights[0].y,22)&&near(dest.lights[0].yaw,120));
 assert(near(dest.actors[0].points[0][0],301)&&near(dest.actors[0].points[0][1],399)&&near(dest.actors[0].points[2][0],304)&&near(dest.actors[0].points[2][1],396)&&dest.actors[0].navigation==0);
 assert(near(dest.particles[0].x,302)&&near(dest.particles[0].z,397)&&near(dest.particles[0].y,21)&&near(dest.particles[0].ceiling,40));
 assert(near(dest.waters[0].x,306)&&near(dest.waters[0].z,395)&&near(dest.waters[0].level,19)&&near(dest.waters[0].radius_x,5)&&near(dest.waters[0].radius_z,3));
 assert(near(dest.navigation[0].x,302)&&near(dest.navigation[0].z,397)&&near(dest.navigation[0].span,32));
 const auto first=doc_bytes(dest);const auto ids_before=graph_bytes(graph);Instance result2;assert(instantiate(loaded,dest,graph,{350,20,400,180,1},0,result2,e,tg));assert(result.root!=result2.root&&dest.actors[1].navigation==1&&dest.assets.size()==1&&dest.materials.size()==1&&dest.graphs.size()==1);std::set<Id> ids;for(const auto &n:graph.nodes)assert(ids.insert(n.id).second);
 assert(transform_instance(dest,graph,result.root,{310,20,420,180,1},e,tg));assert(near(dest.lights[0].x,308)&&near(dest.lights[0].z,419)&&near(dest.particles[0].x,307)&&near(dest.particles[0].z,418)&&near(dest.actors[0].points[0][0],309)&&near(dest.actors[0].points[0][1],419));assert(near(dest.placements[1].x,350));
 assert(transform_instance(dest,graph,result.root,target,e,tg));assert(near(dest.lights[0].x,301)&&near(dest.waters[0].radius_x,5)&&near(dest.navigation[0].span,32));
 auto snapshot=doc_bytes(dest),gs=graph_bytes(graph);const auto previous=result2.root;assert(!instantiate(loaded,dest,graph,{400,20,400,45,1},0,result2,e,tg)&&doc_bytes(dest)==snapshot&&graph_bytes(graph)==gs&&result2.root==previous);assert(!transform_instance(dest,graph,result.root,{310,20,420,45,1},e,tg)&&doc_bytes(dest)==snapshot&&graph_bytes(graph)==gs);
 // Shared aliases are immutable dependencies, never silently replaced.
 auto conflict=dest;conflict.materials[0].normal=.2f;const auto conflict_before=doc_bytes(conflict);assert(!instantiate(loaded,conflict,graph,target,0,result2,e,tg)&&doc_bytes(conflict)==conflict_before&&graph_bytes(graph)==gs);
 // Capacity failures preserve the entire graph/document and output handle.
 Document full;for(unsigned i=0;i<placement_limit;i++)full.placements.push_back({"crate",double(i),0});Graph empty;const auto full_before=doc_bytes(full);assert(!instantiate(loaded,full,empty,target,0,result2,e,tg)&&doc_bytes(full)==full_before&&empty.nodes.empty()&&result2.root==previous);
 // Captured original sources become portable, detached copies.
 Selection resolved;SourceModel sm;sm.index=17;sm.placement=tavern;sm.root_y=11;resolved.source_models.push_back(sm);resolved.objects={{Kind::SourceModel,17}};Prefab from_source;assert(capture(source,resolved,{100,10,200,0,1},"Source tavern",from_source,e)&&from_source.content.placements.size()==1&&from_source.model_heights[0]==11);
 // Missing sources, bad cycles, invalid files and failed save all preserve outputs.
 const auto saved_name=loaded.name;auto invalid_selection=selection;invalid_selection.objects.push_back({Kind::Placement,0});assert(!capture(source,invalid_selection,{},"Bad",loaded,e)&&loaded.name==saved_name);auto broken=loaded;find(broken.graph,broken.root)->parent=broken.root;assert(!world_prefab::validate(broken,e));assert(!save(path.c_str(),broken,e)&&read(path.c_str())==file);
 {std::ofstream f(path,std::ios::binary);f<<file.substr(0,file.size()-1);}assert(!load(path.c_str(),loaded,e)&&loaded.name==saved_name);{std::ofstream f(path,std::ios::binary);f<<file<<"trailing";}assert(!load(path.c_str(),loaded,e)&&loaded.name==saved_name);unlink(path.c_str());
 // Round-trip moves preserve latent navigation bounds rather than growing
 // their axis-aligned envelopes every time the root is rotated.
 Selection square; square.objects={{Kind::Actor,0}};square.ground=sg;Prefab patrol;assert(capture(source,square,{100,10,200,0,1},"Patrol",patrol,e));Document patrol_doc;Graph patrol_graph;Instance patrol_instance;assert(instantiate(patrol,patrol_doc,patrol_graph,{100,10,200,45,1},0,patrol_instance,e,sg));assert(near(patrol_doc.navigation[0].span,32*std::sqrt(2.)));for(unsigned k=0;k<20;k++){assert(transform_instance(patrol_doc,patrol_graph,patrol_instance.root,{100,10,200,0,1},e,sg));assert(near(patrol_doc.navigation[0].span,32));assert(transform_instance(patrol_doc,patrol_graph,patrol_instance.root,{100,10,200,45,1},e,sg));assert(near(patrol_doc.navigation[0].span,32*std::sqrt(2.)));}
 // A locked descendant protects the whole connected bundle from root edits.
 const auto protected_before=doc_bytes(patrol_doc);auto *protected_child=find(patrol_graph,patrol_instance.ids.back().second);assert(protected_child);protected_child->locked=true;
 assert(!transform_instance(patrol_doc,patrol_graph,patrol_instance.root,{105,10,200,0,1},e,sg));assert(doc_bytes(patrol_doc)==protected_before);protected_child->locked=false;
 assert(doc_bytes(source)==original);(void)first;(void)ids_before;
 std::cout<<"Prefab: tavern/model/light/actor route/fire/local pond transforms, dependencies, source detachment, fresh IDs, atomic save/load, malformed input, alias/capacity rollback and repeated navigation rotations passed\n";
}
