#include "engine/world_designer.h"
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <iostream>
using namespace world_designer;
static double ground(double,double,const void *){return 10;}
int main(){
    std::vector<world_studio::Pose> poses(7777);std::vector<world_authoring::Source> sources(7777);
    for(unsigned i=0;i<poses.size();i++){poses[i].alias="starting_crate";poses[i].root={double(i)*3,10,0};sources[i]={"starting_crate","village.txt",double(i)*3,0,10,0,1};}
    Context context;context.source_poses=&poses;context.source_identities=&sources;context.source_signature="0123456789abcdef";context.ground={ground,nullptr};
    world_studio::Document d;d.entities.nodes.push_back({1,0,"Source bundle",{}, {}});
    for(unsigned i=0;i<512;i++)d.entities.nodes.push_back({Id(i+2),1,"Base model",{world_entity::Kind::SourceModel,i},{double(i)*3,10,0,0,1},world_authoring::source_key(sources[i])});
    for(unsigned i=513;i<1024;i++)d.entities.nodes.push_back({Id(i+1),0,"Marker",{}, {}});
    d.entities.next_id=1025;std::string error;if(!world_studio::valid(d,error)){std::cerr<<error;return 1;}
    Selection selection;selection.items={Ref::entity(1)};selection.active=selection.items[0];EditScope scope;world_studio::History history;scope.begin(d,selection);
    const auto started=std::chrono::steady_clock::now();
    for(unsigned i=0;i<20;i++){Command command;command.operation=Operation::Transform;command.transform={.1+double(i)*.01,0,0,0,1};Result result;if(!scope.update(context,command,d,selection,result,error)){std::cerr<<error;return 1;}}
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()/20;
    if(!scope.commit(history)||history.back.size()!=1||d.overrides.size()!=512)return 1;
    for(const auto &o:d.overrides)if(std::abs(o.x-(double(o.index)*3+.29))>1e-6||std::abs(o.offset_y)>1e-6)return 1;
    std::cout<<"{\"sources\":7777,\"entities\":1024,\"bound_sources\":512,\"samples\":20,\"group_transform_ms\":"<<elapsed<<",\"undo_steps\":"<<history.back.size()<<"}\n";
}
