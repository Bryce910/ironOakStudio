#include "engine/world_designer_scene_index.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>
using namespace world_designer_scene;
using namespace world_entity;
namespace {
unsigned checks=0;
#define CHECK(...) do {++checks;assert((__VA_ARGS__));}while(false)
Graph chain(unsigned count){Graph graph;graph.next_id=count+1;for(unsigned i=0;i<count;++i){Node n;n.id=i+1;n.parent=i;n.name="Node "+std::to_string(i);n.local.x=1;graph.nodes.push_back(n);}return graph;}
std::vector<std::string> sources(unsigned count){std::vector<std::string> names;for(unsigned i=0;i<count;++i)names.push_back(i%2?"jungle_oak_"+std::to_string(i):"village_house_"+std::to_string(i));return names;}
void cache_and_mutation(){
    auto graph=chain(1024);auto names=sources(7777);graph.nodes[17].binding={Kind::SourceModel,0};Index index;
    index.sync(graph,names,"source-1",1);index.filter("",{});
    CHECK(index.rows().size()==1024);CHECK(index.sources().size()==7776);CHECK(index.sources().front()==1);CHECK(index.rows().back().depth==1023);
    CHECK(index.rows().front().children&&!index.rows().back().children);CHECK(index.node(graph,1024)==&graph.nodes.back());CHECK(!index.node(graph,9999));
    const auto first=index.counters();const auto *row_memory=index.rows().data();const auto *source_memory=index.sources().data();
    for(unsigned i=0;i<200;++i){index.sync(graph,names,"source-1",1);index.filter("",{});}
    CHECK(index.counters().metadata_compared==first.metadata_compared);CHECK(index.counters().topology_rebuilds==first.topology_rebuilds);CHECK(index.counters().authored_row_rebuilds==first.authored_row_rebuilds);CHECK(index.counters().source_filter_rebuilds==first.source_filter_rebuilds);
    CHECK(row_memory==index.rows().data()&&source_memory==index.sources().data());
    Transform pose;std::string error;CHECK(index.world_pose(graph,1024,pose,error)&&pose.x==1024);CHECK(index.world_pose(graph,500,pose,error)&&pose.x==500);CHECK(index.counters().world_refreshes==1);
    for(unsigned i=0;i<100;++i){graph.nodes[3].local.y=i;index.sync(graph,names,"source-1",2+i);index.filter("",{});CHECK(index.rows().data()==row_memory);CHECK(index.sources().data()==source_memory);}
    CHECK(index.counters().topology_rebuilds==1);CHECK(index.counters().content_refreshes==1);CHECK(index.counters().source_registry_rebuilds==1);CHECK(index.counters().source_filter_rebuilds==1);CHECK(index.counters().authored_row_rebuilds==1);
    CHECK(index.world_pose(graph,1024,pose,error)&&pose.x==1024&&pose.y==99);CHECK(index.counters().world_refreshes==2);
    graph.nodes[3].name="Needle moss";index.sync(graph,names,"source-1",102);index.filter("MOSS needle",{});CHECK(index.rows().size()==4);CHECK(index.rows().back().id==4);CHECK(index.sources().empty());
    index.filter("",{1});CHECK(index.rows().size()==1&&index.rows()[0].collapsed);index.filter("Node 1023",{1});CHECK(index.rows().size()==1024);index.filter("",{});
    graph.nodes[500].locked=true;index.sync(graph,names,"source-1",103);index.filter("",{});CHECK(!index.inherited_locked(500));CHECK(index.inherited_locked(501));CHECK(index.inherited_locked(1024));CHECK(index.local_visible(501));
    graph.nodes[500].visible=false;graph.nodes[500].parent=0;index.sync(graph,names,"source-1",104);index.filter("",{});CHECK(!index.local_visible(501));CHECK(index.rows()[500].depth==0);CHECK(index.rows()[501].depth==1);
    graph.nodes[17].binding={Kind::SourceModel,4};index.sync(graph,names,"source-1",105);index.filter("",{});CHECK(index.sources().front()==0);CHECK(std::find(index.sources().begin(),index.sources().end(),4)==index.sources().end());
    const auto registry_before=index.counters().source_registry_rebuilds;names[0]="same_size_registry_change";index.sync(graph,names,"source-2",105);index.filter("same_size",{});CHECK(index.sources().size()==1&&index.sources()[0]==0);CHECK(index.source_label(0)==names[0]);CHECK(index.counters().source_registry_rebuilds==registry_before+1);
    // A same-size graph replacement and later Undo must not reuse stale indices.
    std::swap(graph.nodes[0],graph.nodes[30]);index.sync(graph,names,"source-2",106);index.filter("",{});CHECK(index.node(graph,1)==&graph.nodes[30]);CHECK(index.rows().front().id==1);
    std::swap(graph.nodes[0],graph.nodes[30]);graph.nodes[500].parent=500;index.sync(graph,names,"source-2",107);index.filter("",{});CHECK(index.node(graph,1)==&graph.nodes[0]);CHECK(index.rows()[500].depth==500);
    CHECK(index.source_label(99999).empty());
}
void selected_union_and_bounds(){
    auto graph=chain(1024);std::vector<std::string> names;Index index;index.sync(graph,names,"empty",1);index.filter("",{});
    std::vector<Id> selected;for(Id i=1;i<=1024;++i)selected.push_back(i);
    auto subset=index.descendants(selected);CHECK(subset.size()==1024);CHECK(index.counters().descendant_visits==1024);
    auto suffix=index.descendants({500,700,9999});CHECK(suffix.size()==525);CHECK(suffix.front()==499&&suffix.back()==1023);CHECK(index.counters().descendant_visits==2048);
    double minimum=1e20,maximum=-1e20;std::string error;
    for(auto i:suffix){Transform pose;CHECK(index.world_pose(graph,graph.nodes[i].id,pose,error));minimum=std::min(minimum,pose.x-2);maximum=std::max(maximum,pose.x+2);}
    CHECK(minimum==498&&maximum==1026);CHECK((minimum+maximum)*.5==762);CHECK((maximum-minimum)*.5==264);CHECK(index.counters().world_refreshes==1);
    graph.nodes[0].parent=1024;index.sync(graph,names,"empty",2);index.filter("",{});CHECK(index.rows().empty());CHECK(index.descendants({1}).empty());Transform out;CHECK(!index.world_pose(graph,1,out,error));CHECK(!error.empty());
}
void nonordered_graph(){
    auto graph=chain(256);for(std::size_t i=0;i<graph.nodes.size();++i){graph.nodes[i].parent=i<7?0:Id(i/3);graph.nodes[i].local={double(i%4),double(i%3),-double(i%5),double(i%7),1};}
    graph.nodes[5].locked=true;std::rotate(graph.nodes.begin(),graph.nodes.begin()+77,graph.nodes.end());Index index;index.sync(graph,{},"none",1);index.filter("",{});CHECK(index.rows().size()==256);
    std::vector<Id> roots={6,17,91};const auto selected=index.descendants(roots);std::vector<bool> membership(graph.nodes.size());for(auto i:selected)membership[i]=true;std::string error;
    for(std::size_t i=0;i<graph.nodes.size();++i){const auto &node=graph.nodes[i];const auto *ancestor=&node;bool wanted=false,locked=false;unsigned depth=0;while(ancestor){wanted|=std::find(roots.begin(),roots.end(),ancestor->id)!=roots.end();locked|=ancestor->locked;ancestor=ancestor->parent?find(graph,ancestor->parent):nullptr;++depth;}CHECK(membership[i]==wanted);CHECK(index.inherited_locked(node.id)==locked);Transform expected,actual;CHECK(world_transform(graph,node.id,expected,error));CHECK(index.world_pose(graph,node.id,actual,error));CHECK(std::abs(expected.x-actual.x)<1e-9&&std::abs(expected.y-actual.y)<1e-9&&std::abs(expected.z-actual.z)<1e-9&&std::abs(expected.yaw-actual.yaw)<1e-9);auto row=std::find_if(index.rows().begin(),index.rows().end(),[&](const auto &r){return r.id==node.id;});CHECK(row!=index.rows().end()&&row->depth==depth-1);}
    CHECK(index.counters().world_refreshes==1);const auto visits=index.counters().descendant_visits;CHECK(index.descendants({}).empty());CHECK(index.counters().descendant_visits==visits);
}
// Exact pre-optimization traversal: every selected root scans every node and
// performs linear ID lookup at each ancestor step. Count work deterministically.
std::uint64_t legacy_frame_work(const Graph &graph,const std::vector<Id> &roots){
    std::uint64_t comparisons=0;for(auto root:roots)for(const auto &node:graph.nodes){const auto *ancestor=&node;unsigned guard=0;while(ancestor&&ancestor->id!=root&&guard++<node_limit){const auto parent=ancestor->parent;ancestor=nullptr;if(parent)for(const auto &candidate:graph.nodes){++comparisons;if(candidate.id==parent){ancestor=&candidate;break;}}}}return comparisons;
}
struct LegacyRows {
    struct Row {Id id;std::string label;unsigned depth;};
    std::uint64_t revision=~std::uint64_t(0),source_tests=0,rebuilds=0;std::vector<unsigned> filtered;std::vector<Row> rows;
    void update(const Graph &graph,const std::vector<std::string> &names,std::uint64_t next){
        if(revision==next){return;}revision=next;++rebuilds;filtered.clear();rows.clear();
        const auto &nodes=graph.nodes;std::vector<bool> registered(names.size());std::vector<std::pair<Id,std::size_t>> indices;indices.reserve(nodes.size());
        for(std::size_t i=0;i<nodes.size();++i){indices.push_back({nodes[i].id,i});}std::sort(indices.begin(),indices.end());
        auto index_of=[&](Id id){auto at=std::lower_bound(indices.begin(),indices.end(),std::pair<Id,std::size_t>{id,0});return at!=indices.end()&&at->first==id?at->second:nodes.size();};
        std::vector<std::size_t> roots;std::vector<std::vector<std::size_t>> children(nodes.size());
        for(std::size_t i=0;i<nodes.size();++i){if(nodes[i].binding.kind==Kind::SourceModel)registered[nodes[i].binding.index]=true;if(nodes[i].parent){const auto parent=index_of(nodes[i].parent);if(parent<nodes.size())children[parent].push_back(i);}else roots.push_back(i);}
        auto visit=[&](auto &&self,std::size_t i,unsigned depth)->void{rows.push_back({nodes[i].id,nodes[i].name,depth});for(auto child:children[i])self(self,child,depth+1);};for(auto root:roots)visit(visit,root,0);
        for(unsigned i=0;i<names.size();++i){++source_tests;if(!registered[i])filtered.push_back(i);}
    }
};
void benchmark(){
    auto graph=chain(1024);auto names=sources(7777);Index index;LegacyRows legacy;index.sync(graph,names,"bench",1);index.filter("",{});legacy.update(graph,names,1);
    using clock=std::chrono::steady_clock;constexpr unsigned updates=1000;auto begin=clock::now();
    for(unsigned i=0;i<updates;++i){graph.nodes[500].local.x=i;legacy.update(graph,names,i+2);}const auto old_us=std::chrono::duration<double,std::micro>(clock::now()-begin).count();
    begin=clock::now();for(unsigned i=0;i<updates;++i){graph.nodes[500].local.x=i;index.sync(graph,names,"bench",i+2);index.filter("",{});}const auto new_us=std::chrono::duration<double,std::micro>(clock::now()-begin).count();
    CHECK(index.counters().source_filter_rebuilds==1);CHECK(index.counters().authored_row_rebuilds==1);CHECK(legacy.source_tests==std::uint64_t(updates+1)*names.size());
    // A 128-node chain avoids making the reference cubic walk dominate CI.
    auto stress=chain(128);const std::vector<Id> roots={1,33,65,97};begin=clock::now();const auto comparisons=legacy_frame_work(stress,roots);const auto old_frame_us=std::chrono::duration<double,std::micro>(clock::now()-begin).count();Index frame;frame.sync(stress,{},"frame",1);begin=clock::now();const auto nodes=frame.descendants(roots);const auto new_frame_us=std::chrono::duration<double,std::micro>(clock::now()-begin).count();CHECK(nodes.size()==128);CHECK(frame.counters().descendant_visits==128);CHECK(comparisons>1000000);
    std::printf("BENCH pose_drag updates=%u sources=%zu nodes=%zu legacy_us=%.3f indexed_us=%.3f legacy_source_tests=%llu indexed_source_filter_rebuilds=%llu indexed_authored_row_rebuilds=%llu\n",updates,names.size(),graph.nodes.size(),old_us,new_us,(unsigned long long)legacy.source_tests,(unsigned long long)index.counters().source_filter_rebuilds,(unsigned long long)index.counters().authored_row_rebuilds);
    std::printf("BENCH subtree_union nodes=128 roots=4 legacy_us=%.3f indexed_us=%.3f legacy_id_comparisons=%llu indexed_node_visits=%llu\n",old_frame_us,new_frame_us,(unsigned long long)comparisons,(unsigned long long)frame.counters().descendant_visits);
}
}
int main(){cache_and_mutation();selected_union_and_bounds();nonordered_graph();benchmark();std::printf("world_designer_scene_index: %u checks passed\n",checks);}
