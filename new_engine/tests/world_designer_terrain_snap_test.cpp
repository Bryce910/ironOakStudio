#include "engine/world_designer.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace world_designer;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static bool near(double a,double b){return std::abs(a-b)<1e-5;}
static double flat(double,double,const void *){return 10;}
static double slope(double x,double z,const void *){return 10+.5*x+.25*z;}
static double invalid(double x,double,const void *){return x>9?std::numeric_limits<double>::quiet_NaN():10;}
static bool bounds(const std::string &alias,world_studio::Vec &lo,world_studio::Vec &hi,const void *){if(alias=="missing")return false;lo={-1,-2,-1};hi={1,2,1};return true;}
static bool support(const std::string &,const world_entity::Transform &t,double burial,double elevation,world_prefab::Ground g,double &out,const void *){out=g.height(t.x,t.z,g.context)-burial+elevation-.05;return true;}
static bool reject(const world_studio::Document &,const void *,std::string &e){e="Host rejected grounded model";return false;}
static std::string bytes(const world_studio::Document &d){std::string text,error;return world_studio::encode(d,text,error)?text:error;}
int main(){
    std::string error;Context context;context.ground={flat,nullptr};ModelBounds meshes{bounds,nullptr,support};
    world_studio::Document d,next;Selection selected;Result result;
    world_studio::Placement model;model.alias="offpivot";model.offset_y=5;d.placements={model};
    std::vector<world_authoring::Source> identities;
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    const auto before=bytes(d);
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&result.changed&&result.geometry);
    CHECK(near(next.placements[0].offset_y,2.05)&&near(next.placements[0].x,0)&&near(next.placements[0].z,0));
    CHECK(bytes(d)==before&&result.selection.active==selected.active);
    // A nonzero mesh minimum and the renderer's 5 cm support adjustment must
    // both be included: visible bottom = (10-.05)+2.05-2 = 10.
    CHECK(near(10-.05+next.placements[0].offset_y-2,10));
    world_studio::History history;history.checkpoint(d);d=next;
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&!result.changed&&bytes(d)==bytes(next));
    CHECK(history.undo(d)&&near(d.placements[0].offset_y,5));
    CHECK(history.redo(d)&&near(d.placements[0].offset_y,2.05));
    // The same action raises a buried object to the terrain.
    d.placements[0].offset_y=-6;
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&near(next.placements[0].offset_y,2.05));
    // Scales and rotations stay unchanged. With roll=90 the scaled X extent
    // becomes the vertical extent, requiring a 13-metre explicit world pivot.
    d={};model.roll=90;model.scale=2;model.sx=1.5;model.offset_y=9;d.placements={model};
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));selected.clear();
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error));
    world_entity::Transform pose;CHECK(world_pose(next,context,result.selection.active,pose,error)&&near(pose.y,13));
    CHECK(next.placements[0].roll==90&&next.placements[0].scale==2&&near(next.placements[0].sx,1.5));
    // Uneven terrain uses footprint support, not just the pivot's terrain.
    model={};model.alias="offpivot";model.offset_y=10;model.yaw=90;d={};d.placements={model};context.ground={slope,nullptr};
    CHECK(world_authoring::synchronize(d,d.entities,identities,slope,nullptr,error));selected.clear();
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&near(next.placements[0].offset_y,2.8));
    CHECK(near(10-.05+next.placements[0].offset_y-2,10.75));
    // Source objects remain independent overrides; no source geometry or added
    // placement is modified, and a second snap issues no additional entity.
    d={};selected.clear();context.ground={flat,nullptr};
    std::vector<world_studio::Pose> sources(1);sources[0].alias="offpivot";sources[0].root={0,14.95,0};sources[0].offset_y=5;
    identities={{"offpivot","scene.txt",0,0,14.95,0,1}};context.source_poses=&sources;context.source_identities=&identities;context.source_signature="0123456789abcdef";
    CHECK(select(d,context,selected,Ref::source_model(0),false,error));
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&next.placements.empty()&&next.overrides.size()==1&&near(next.overrides[0].offset_y,2.05));
    CHECK(sources[0].offset_y==5&&near(sources[0].root.y,14.95)&&d.entities.nodes.empty());
    d=next;selected=result.selection;
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error)&&!result.changed&&next.entities.nodes.size()==1);
    // A selected group retains its internal height differences and attachments.
    d={};model={};model.alias="offpivot";model.offset_y=5;d.placements={model,model};d.placements[1].x=5;d.placements[1].offset_y=9;
    world_studio::Light light;light.x=1;light.y=21;d.lights={light};
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));
    Id group=0;CHECK(world_entity::add(d.entities,0,"House and lamp",{},{0,10,0,0,1},group,error));
    const auto nodes=d.entities.nodes;for(const auto &node:nodes)if(node.id!=group)CHECK(world_entity::reparent(d.entities,node.id,group,true,error));
    selected.clear();CHECK(select(d,context,selected,Ref::entity(group),false,error));
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,0}),true,error));
    CHECK(snap_to_terrain(d,selected,context,meshes,next,result,error));
    CHECK(near(next.placements[0].offset_y,2.05)&&near(next.placements[1].offset_y,6.05)&&near(next.lights[0].y,18.05));
    CHECK(next.entities.nodes.size()==d.entities.nodes.size()&&near(next.placements[1].x,5));
    // Multi-selection is atomic, including locks, bad ground, missing geometry
    // and the host's live-component guard. Output sentinels remain untouched.
    world_studio::Document untouched;untouched.placements={{"sentinel",99,99}};Result sentinel;sentinel.created=777;
    world_entity::find(d.entities,group)->locked=true;
    CHECK(!snap_to_terrain(d,selected,context,meshes,untouched,sentinel,error)&&untouched.placements[0].alias=="sentinel"&&sentinel.created==777);
    world_entity::find(d.entities,group)->locked=false;auto guarded=context;guarded.validate_candidate=reject;
    CHECK(!snap_to_terrain(d,selected,guarded,meshes,untouched,sentinel,error)&&sentinel.created==777);
    d={};model.offset_y=7;d.placements={model,model};d.placements[1].x=10;
    CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));selected.clear();
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Placement,1}),true,error));
    auto bad_ground=context;bad_ground.ground={invalid,nullptr};
    CHECK(!snap_to_terrain(d,selected,bad_ground,meshes,untouched,sentinel,error)&&untouched.placements[0].alias=="sentinel");
    d.placements[1].alias="missing";
    CHECK(!snap_to_terrain(d,selected,context,meshes,untouched,sentinel,error)&&sentinel.created==777);
    selected.clear();CHECK(!snap_to_terrain(d,selected,context,meshes,untouched,sentinel,error));
    d={};d.lights={light};CHECK(world_authoring::synchronize(d,d.entities,identities,flat,nullptr,error));
    CHECK(select(d,context,selected,from_binding(d,{world_entity::Kind::Light,0}),false,error));
    CHECK(!snap_to_terrain(d,selected,context,meshes,untouched,sentinel,error)&&error.find("model")!=std::string::npos);
    std::cout<<"world_designer_terrain_snap: "<<checks<<" checks passed\n";
}
