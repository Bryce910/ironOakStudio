#include "engine/world_designer.h"
#include "engine/world_transform_math.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <unistd.h>
static bool close(double a,double b){return std::abs(a-b)<1e-6;}
static void same(const world_entity::Transform &a,const world_entity::Transform &b){auto x=vw_transform::matrix(a),y=vw_transform::matrix(b);for(unsigned i=0;i<3;i++){assert(close(x.p[i],y.p[i]));for(unsigned j=0;j<3;j++)assert(close(x.a[i][j],y.a[i][j]));}}
int main(){
    using namespace world_entity;std::string e;unsigned checks=0;
    for(int yaw=-170;yaw<=170;yaw+=17)for(int pitch=-90;pitch<=90;pitch+=15){Transform t={10,3,-4,double(yaw),1.2,double(pitch),23,.5,1.3,2};auto m=vw_transform::matrix(t);same(t,vw_transform::decompose(m));auto q=vw_transform::point(m,{2,3,4}),v=vw_transform::point(vw_transform::inverse(m),q);assert(close(v[0],2)&&close(v[1],3)&&close(v[2],4));Transform parent={4,2,9,31,1.1,12,-7};same(compose(parent,relative(parent,t)),t);checks+=4;}
    Graph g;Id p=0,c=0;assert(add(g,0,"parent",{}, {},p,e));assert(add(g,p,"child",{},{2,3,4,30,1,12,7},c,e));Transform child;assert(world_transform(g,c,child,e));assert(reparent(g,c,0,true,e));Transform moved;assert(world_transform(g,c,moved,e));same(child,moved);
    Transform anisotropic={0,0,0,0,1,0,0,2,1,1};assert(set_world(g,p,anisotropic,e));auto preserved=g;assert(!reparent(g,c,p,true,e));assert(g.nodes.size()==preserved.nodes.size()&&find(g,c)->parent==0);
    world_studio::Document d;d.placements.push_back({"starting_crate",20,30});assert(world_authoring::synchronize(d,d.entities,{},nullptr,nullptr,e));world_designer::Context context;world_designer::Selection selection;assert(world_designer::select(d,context,selection,world_designer::Ref::entity(d.entities.nodes[0].id),false,e));world_designer::Command command;command.operation=world_designer::Operation::Transform;command.transform={20,2,30,40,1,25,-30,1.5,.7,1.2};world_studio::Document out;world_designer::Result result;assert(world_designer::execute(d,selection,context,command,out,result,e)&&result.changed);assert(close(out.placements[0].pitch,25)&&close(out.placements[0].sx,1.5));
    char path[]="/tmp/valwood-full-transform-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);assert(world_studio::save(path,out,e));world_studio::Document loaded;assert(world_studio::load(path,loaded,e));Transform after;assert(world_designer::world_pose(loaded,context,selection.active,after,e));same(after,command.transform);
    world_prefab::Selection capture;capture.objects={{Kind::Placement,0}};world_prefab::Prefab prefab;assert(world_prefab::capture(out,capture,command.transform,"Tilted crate",prefab,e));assert(world_prefab::save(path,prefab,e));world_prefab::Prefab restored;assert(world_prefab::load(path,restored,e));same(restored.origin,command.transform);assert(close(restored.content.placements[0].roll,-30));unlink(path);
    world_studio::Pose pose;pose.root={20,2,30};pose.yaw=40*.017453292519943295;pose.pitch=25*.017453292519943295;pose.roll=-30*.017453292519943295;pose.sx=1.5;pose.sy=.7;pose.sz=1.2;pose.minimum={-1,-1,-1};pose.maximum={1,1,1};auto m=vw_transform::matrix(vw_transform::pose(pose));auto origin=vw_transform::point(m,{0,0,-5}),direction=vw_transform::vector(m,{0,0,1});double distance=0;assert(world_studio::ray_bounds({{origin[0],origin[1],origin[2]},{direction[0],direction[1],direction[2]}},pose,distance)&&close(distance,4));
    auto invalid=command;invalid.transform.sx=0;assert(!world_designer::execute(d,selection,context,invalid,out,result,e));
    std::cout<<"PASS full transforms: "<<checks<<" matrix/normal-space contracts; pitched/rolled/stretched edit, hierarchy shear rejection, scene+prefab roundtrip, exact bounds picking and invalid-scale rollback\n";
}
