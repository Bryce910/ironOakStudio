#include "engine/world_npc_authoring.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <limits>
struct Fixture {bool water=false;};
static bool ground(void *data,double x,double z,double &y){y=20;return !static_cast<Fixture *>(data)->water&&std::isfinite(x)&&std::isfinite(z);}
static bool clear(void *,const world_navigation::Point &a,const world_navigation::Point &b,float r){
    // Solid wall x=-1..1, z=-3..3. Navigation must go around either end.
    for(unsigned i=0;i<=20;i++){double t=i/20.;double x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;if(std::abs(x)<1+r&&std::abs(z)<3+r)return false;}return true;
}
int main(){
    Fixture fixture;world_navigation::Query query{&fixture,ground,clear};std::vector<world_navigation::Point> result;std::string error;
    world_studio::Actor actor;actor.points={{{-7,0}}};
    assert(world_npc_authoring::route(actor,true,false,query,nullptr,result,error)&&result.size()==1);
    actor.points.push_back({{-7,5}});assert(!world_npc_authoring::route(actor,true,false,query,nullptr,result,error));
    assert(world_npc_authoring::route(actor,true,true,query,nullptr,result,error)&&result.size()==3); // outbound and return
    actor.points.push_back(actor.points.back());assert(!world_npc_authoring::route(actor,true,true,query,nullptr,result,error));actor.points.pop_back();
    actor.loop=true;actor.points.push_back(actor.points.front());assert(!world_npc_authoring::route(actor,true,true,query,nullptr,result,error));actor.points.pop_back();
    fixture.water=true;assert(!world_npc_authoring::route(actor,true,true,query,nullptr,result,error));
    assert(world_npc_authoring::route(actor,false,false,query,nullptr,result,error)&&result.size()==1);fixture.water=false;
    actor.loop=false;actor.points={{{-7,0}},{{7,0}}};assert(!world_npc_authoring::route(actor,true,true,query,nullptr,result,error));
    world_studio::NavigationArea area;area.x=area.z=0;area.span=24;area.cell=1;world_navigation::Grid grid;assert(world_navigation::bake(area,query,grid,error));
    assert(world_npc_authoring::route(actor,true,true,query,&grid,result,error)&&result.size()>3);
    for(unsigned i=1;i<result.size();i++)assert(clear(nullptr,result[i-1],result[i],.32f));
    actor.points[0]={0,0};assert(!world_npc_authoring::route(actor,true,true,query,&grid,result,error));
    actor.points[0]={std::numeric_limits<double>::quiet_NaN(),0};assert(!world_npc_authoring::route(actor,false,true,query,nullptr,result,error));
    std::puts("NPC authoring: stationary/walking, return/loop, duplicate endpoints, disabled repair, water, solid endpoint and obstacle detour passed");
}
