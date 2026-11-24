#include "engine/world_entity_graph.h"
#include "engine/world_studio.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
namespace {
bool near(double a,double b){return std::abs(a-b)<1e-7;}
std::string bytes(const world_entity::Graph &g){std::string s,e;assert(world_entity::encode(g,s,e));return s;}
}
int main(){
 using namespace world_entity;std::string e;Graph g;Id root=0,model=0,light=0,other=0;
 assert(add(g,0,"Tavern",{}, {100,10,50,90,2},root,e));
 assert(add(g,root,"Building",{Kind::Placement,0},{2,0,3,15,.5},model,e));
 assert(add(g,model,"Lantern",{Kind::Light,0},{0,2,1,-15,1},light,e));
 Transform w;assert(world_transform(g,model,w,e));assert(near(w.x,106)&&near(w.y,10)&&near(w.z,46)&&near(w.yaw,105)&&near(w.scale,1));
 Transform l;assert(world_transform(g,light,l,e));const auto recovered=compose(w,relative(w,l));assert(near(recovered.x,l.x)&&near(recovered.y,l.y)&&near(recovered.z,l.z));
 std::vector<std::pair<Id,Transform>> all;assert(world_transforms(g,all,e)&&all.size()==g.nodes.size());for(const auto &v:all){Transform one;assert(world_transform(g,v.first,one,e));assert(near(one.x,v.second.x)&&near(one.y,v.second.y)&&near(one.z,v.second.z));}
 assert(add(g,0,"Other district",{}, {-50,3,-20,-45,1},other,e));
 assert(reparent(g,model,other,true,e));Transform moved;assert(world_transform(g,model,moved,e));assert(near(w.x,moved.x)&&near(w.y,moved.y)&&near(w.z,moved.z)&&near(w.yaw,moved.yaw)&&near(w.scale,moved.scale));
 assert(world_transform(g,light,moved,e));assert(near(l.x,moved.x)&&near(l.y,moved.y)&&near(l.z,moved.z));
 auto unchanged=bytes(g);assert(!reparent(g,other,light,true,e)&&bytes(g)==unchanged);assert(!reparent(g,model,98765,true,e)&&bytes(g)==unchanged);
 Id created=123;assert(!add(g,0,"Duplicate binding",{Kind::Placement,0},{},created,e)&&created==123&&bytes(g)==unchanged);
 assert(!add(g,0,"Missing object",{Kind::Actor,32},{},created,e)&&bytes(g)==unchanged);
 world_studio::Document doc;assert(!validate(g,&doc,e));doc.placements.push_back({"tavern"});doc.lights.push_back({});assert(validate(g,&doc,e));
 std::vector<std::pair<Id,Id>> ids={{7,8}};assert(!clone_subtree(g,other,0,{},created,ids,e)&&created==123&&ids.size()==1&&bytes(g)==unchanged);
 assert(clone_subtree(g,other,0,{{{Kind::Placement,0},{Kind::Placement,1}},{{Kind::Light,0},{Kind::Light,1}}},created,ids,e));
 assert(ids.size()==3&&created!=other);std::set<Id> unique;for(const auto &n:g.nodes)assert(unique.insert(n.id).second);for(auto pair:ids)assert(pair.first!=pair.second);
 const auto cloned_model=ids[1].second;assert(find(g,cloned_model)->binding==Binding({Kind::Placement,1}));assert(world_transform(g,cloned_model,moved,e));assert(near(moved.x,w.x)&&near(moved.z,w.z));
 assert(set_world(g,created,{1000,30,600,90,2},e));assert(world_transform(g,created,moved,e)&&near(moved.x,1000));
 Graph roundtrip;assert(decode(bytes(g),roundtrip,e)&&bytes(roundtrip)==bytes(g));
 unchanged=bytes(roundtrip);assert(!decode("VALWOOD_ENTITIES 1 2 1\n1 2 \"bad\" 0 0 0 0 0 0 1\n",roundtrip,e)&&bytes(roundtrip)==unchanged);
 assert(!decode(bytes(g)+"trailing",roundtrip,e)&&bytes(roundtrip)==unchanged);
 assert(!decode("VALWOOD_ENTITIES 2 -1 0\n",roundtrip,e)&&bytes(roundtrip)==unchanged);
 assert(!decode("VALWOOD_ENTITIES 2 18446744073709551615 1\n-2 0 \"negative ID\" 0 0 0 0 0 0 1 \"\"\n",roundtrip,e)&&bytes(roundtrip)==unchanged);
 auto bad=g;find(bad,root)->parent=root;assert(!validate(bad,nullptr,e));bad=g;bad.nodes[0].parent=123456;assert(!validate(bad,nullptr,e));bad=g;bad.nodes[0].local.scale=std::numeric_limits<double>::quiet_NaN();assert(!validate(bad,nullptr,e));
 Graph full;full.nodes.reserve(node_limit);for(unsigned i=0;i<node_limit;i++)full.nodes.push_back({Id(i)+1,0,"Root",{}, {}});full.next_id=node_limit+1;unchanged=bytes(full);assert(!add(full,0,"Overflow",{},{},created,e)&&bytes(full)==unchanged);
 Graph high;high.next_id=(Id(1)<<60)+17;assert(add(high,0,"64-bit persistent ID",{},{},created,e)&&created==(Id(1)<<60)+17);assert(decode(bytes(high),roundtrip,e)&&roundtrip.nodes[0].id==created);high.next_id=std::numeric_limits<Id>::max();unchanged=bytes(high);assert(!add(high,0,"Wrap",{},{},created,e)&&bytes(high)==unchanged);
 // Direct numeric edits synchronize listed objects exactly once, preserving
 // unedited attached children even when their parent's world pose changes.
 assert(world_transform(g,model,w,e)&&world_transform(g,light,l,e));
 auto edited=w;edited.x+=50;edited.yaw+=20;
 assert(synchronize(g,{{{Kind::Placement,0},edited}},e));assert(world_transform(g,model,moved,e)&&near(moved.x,edited.x));assert(world_transform(g,light,moved,e)&&near(moved.x,l.x)&&near(moved.y,l.y)&&near(moved.z,l.z));
 unchanged=bytes(g);assert(!synchronize(g,{{{Kind::Placement,3},{}}},e)&&bytes(g)==unchanged);
 // Array compaction preserves stable IDs and promotes live attachments.
 auto compact=g;std::vector<Binding> removed;assert(erase_binding(compact,{Kind::Placement,0},ChildPolicy::PreserveWorld,removed,e));assert(removed.size()==1&&!find(compact,model)&&find(compact,light)->parent==other);assert(world_transform(compact,light,moved,e)&&near(moved.x,l.x)&&near(moved.y,l.y)&&near(moved.z,l.z));assert(find(compact,cloned_model)->binding==Binding({Kind::Placement,0}));
 Graph missing;Id later=0;assert(add(missing,0,"Later model",{Kind::Placement,3},{},later,e));assert(erase_binding(missing,{Kind::Placement,1},ChildPolicy::PreserveWorld,removed,e)&&find(missing,later)->binding.index==2&&removed.empty());
 auto subtree=g;assert(erase_binding(subtree,{Kind::Placement,0},ChildPolicy::EraseSubtree,removed,e)&&removed.size()==2&&!find(subtree,light));
 // Canonical source metadata survives generated-file reorder while retaining
 // persistent entity IDs. Missing or ambiguous identities fail closed.
 Graph sources;Id source_a=0,source_b=0;assert(add(sources,0,"Tavern source",{Kind::SourceModel,0},{},source_a,e));assert(add(sources,0,"Sign source",{Kind::SourceModel,2},{},source_b,e));
 const std::vector<std::string> old_keys={"tavern|village.txt|100,200|0|1|0|0","crate|market.txt|1,2|0|1|0|0","sign|village.txt|101,200|0|1|0|0"};
 const std::vector<std::string> new_keys={old_keys[2],old_keys[1],old_keys[0]};std::vector<std::pair<unsigned,unsigned>> index_map;
 assert(remap_source_bindings(sources,old_keys,new_keys,index_map,e)&&find(sources,source_a)->binding.index==2&&find(sources,source_b)->binding.index==0&&find(sources,source_a)->source_key==old_keys[0]);
 assert(decode(bytes(sources),roundtrip,e)&&bytes(roundtrip)==bytes(sources));unchanged=bytes(sources);auto missing_keys=new_keys;missing_keys[2]="changed-position";assert(!remap_source_bindings(sources,new_keys,missing_keys,index_map,e)&&bytes(sources)==unchanged);auto ambiguous_keys=new_keys;ambiguous_keys[1]=new_keys[0];assert(!remap_source_bindings(sources,new_keys,ambiguous_keys,index_map,e)&&bytes(sources)==unchanged);
 auto wrong_previous=new_keys;wrong_previous[2]="different tavern";assert(!remap_source_bindings(sources,wrong_previous,new_keys,index_map,e)&&bytes(sources)==unchanged);
 assert(source_index_map(old_keys,new_keys,{0,2,0},index_map,e)&&index_map.size()==2);const auto map_before=index_map;auto ambiguous_old=old_keys;ambiguous_old[1]=old_keys[0];assert(!source_index_map(ambiguous_old,new_keys,{0},index_map,e)&&index_map==map_before);
 Graph legacy;assert(decode("VALWOOD_ENTITIES 1 2 1\n1 0 \"legacy\" 2 0 0 0 0 0 1\n",legacy,e)&&legacy.nodes[0].source_key.empty());assert(remap_source_bindings(legacy,old_keys,new_keys,index_map,e)&&legacy.nodes[0].binding.index==2&&legacy.nodes[0].source_key==old_keys[0]);
 const Id previous_next=g.next_id;assert(erase_subtree(g,other,removed,e)&&removed.size()==2&&g.next_id==previous_next&&!find(g,model));assert(validate(g,nullptr,e));
 std::cout<<"Entity graph: transforms, preserve-world reparent, cycles/dangling/duplicate bindings, fresh clone IDs, 64-bit persistence, atomic failures and budgets passed\n";
}
