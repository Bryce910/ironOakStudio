#include "engine/world_designer.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace world_designer;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<" | "<<error<<'\n';return 1;}}while(0)
static bool near(double a,double b){return std::abs(a-b)<1e-5;}
static double ground(double x,double z,const void *){return 10+.2*x+.1*z;}
static bool bounds(const std::string &alias,world_studio::Vec &lo,world_studio::Vec &hi,const void *){
    if(alias=="missing")return false;
    lo={-2,0,-3};hi={2,6,3};return true;
}
static bool reject(const world_studio::Document &,const void *,std::string &error){error="Host rejected paste";return false;}
int main(){
    std::string error;Context context;context.ground={ground,nullptr};ModelBounds meshes{bounds,nullptr};
    Clipboard clipboard;PasteOptions options;Result result;world_studio::Document d,next;
    next.placements={{"sentinel",99,99}};result.created=777;
    CHECK(!clipboard.paste(d,context,options,next,result,error)&&next.placements[0].alias=="sentinel"&&result.created==777);
    Selection selection;std::vector<world_studio::Pose> sources(1);sources[0].alias="tavern";sources[0].root={100,ground(100,200,nullptr),200};sources[0].minimum={-2,0,-3};sources[0].maximum={2,6,3};
    std::vector<world_authoring::Source> identities={{"tavern","village.txt",100,200,sources[0].root.y,0,1}};
    context.source_poses=&sources;context.source_identities=&identities;
    CHECK(select(d,context,selection,Ref::source_model(0),false,error));
    CHECK(clipboard.copy(d,selection,context,meshes,error)&&clipboard.ready());
    CHECK(d.placements.empty()&&d.overrides.empty()&&d.entities.nodes.empty());
    CHECK(clipboard.paste(d,context,options,next,result,error));
    CHECK(next.placements.size()==1&&near(next.placements[0].x,104)&&near(next.placements[0].z,200));
    CHECK(near(next.placements[0].offset_y,0)&&d.entities.nodes.empty());
    CHECK(result.geometry&&result.selection.active.id==result.created);
    CHECK(next.entities.nodes.size()==1&&world_entity::find(next.entities,result.created)->binding.kind==world_entity::Kind::Placement);
    CHECK(project(next,context,result.selection).keys==std::vector<unsigned>{8192});
    world_studio::History history;history.checkpoint(d);d=next;
    const auto first=result.created;
    CHECK(clipboard.paste(d,context,options,next,result,error));history.checkpoint(d);d=next;
    CHECK(d.placements.size()==2&&near(d.placements[1].x,108)&&result.created!=first&&history.back.size()==2);
    // Copying the original again must skip both already occupied slots.
    Clipboard recopied;CHECK(recopied.copy(d,selection,context,meshes,error));
    CHECK(recopied.paste(d,context,options,next,result,error)&&near(next.placements.back().x,112));
    auto reversed=options;reversed.direction=1;
    world_studio::Document back;
    CHECK(recopied.paste(next,context,reversed,back,result,error)&&near(back.placements.back().x,96));
    // The selected copy is the visible model; moving it updates the next anchor.
    Selection moved=result.selection;Command move;move.operation=Operation::Transform;
    CHECK(world_pose(back,context,moved.active,move.transform,error));move.transform.x=90;move.transform.y=ground(90,move.transform.z,nullptr);
    world_studio::Document relocated;CHECK(execute(back,moved,context,move,relocated,result,error));
    CHECK(recopied.paste(relocated,context,reversed,back,result,error)&&near(back.placements.back().x,86));
    CHECK(history.undo(d)&&d.placements.size()==1);
    CHECK(history.redo(d)&&d.placements.size()==2);
    CHECK(history.undo(d));
    CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements.back().x,108));
    // A failed host validation cannot advance the row or publish either output.
    Context rejected=context;rejected.validate_candidate=reject;world_studio::Document untouched;untouched.placements={{"sentinel",99,99}};Result sentinel;sentinel.created=888;
    CHECK(!clipboard.paste(next,rejected,options,untouched,sentinel,error)&&untouched.placements[0].alias=="sentinel"&&sentinel.created==888);
    d=next;CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements.back().x,112));
    // Copy is a snapshot, independent of later edits/removal of its source.
    sources[0].alias="barrel";sources[0].scale=4;
    d={};CHECK(clipboard.paste(d,context,options,next,result,error)&&next.placements[0].alias=="tavern"&&near(next.placements[0].scale,1));
    // Bounds and row axes include yaw and nonuniform scale (4 * 2 * 1.5).
    d={};world_studio::Placement house;house.alias="tavern";house.x=100;house.z=200;house.yaw=90;house.scale=2;house.sx=1.5;d.placements={house};
    CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));selection.clear();
    CHECK(select(d,context,selection,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(clipboard.copy(d,selection,context,meshes,error));
    options.gap=1;CHECK(clipboard.paste(d,context,options,next,result,error));
    CHECK(near(next.placements[1].x,100)&&near(next.placements[1].z,187)&&near(next.placements[1].offset_y,0));
    CHECK(near(next.placements[1].yaw,90)&&near(next.placements[1].sx,1.5));
    // Forward follows the copied yaw; left/back are their exact opposites.
    CHECK(clipboard.copy(d,selection,context,meshes,error));options.direction=2;options.gap=0;
    CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements[1].x,112)&&near(next.placements[1].z,200));
    CHECK(clipboard.copy(d,selection,context,meshes,error));options.direction=1;
    CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements[1].z,212));
    CHECK(clipboard.copy(d,selection,context,meshes,error));options.direction=3;
    CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements[1].x,88));
    // A 90-degree roll puts the six-metre height along the row: step is 12.
    house.yaw=0;house.roll=90;house.sx=1;d={};d.placements={house};
    CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));selection.clear();
    CHECK(select(d,context,selection,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(clipboard.copy(d,selection,context,meshes,error));options={};
    CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements[1].x,112)&&near(next.placements[1].roll,90));
    // A connected group copies attachments/dependencies once, even when its
    // child is also selected, and snaps the combined 14-metre footprint.
    d={};d.placements={{"tavern",0,0},{"barrel",10,0}};world_studio::Light light;light.x=1;light.y=12;d.lights={light};
    world_studio::Material material;material.alias="tavern";material.roughness=.4;d.materials={material};
    CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));
    Id root=0;CHECK(world_entity::add(d.entities,0,"Street section",{},{0,10,0,0,1},root,error));
    const auto children=d.entities.nodes;for(const auto &node:children)if(node.id!=root)CHECK(world_entity::reparent(d.entities,node.id,root,true,error));
    selection.clear();CHECK(select(d,context,selection,Ref::entity(root),false,error));
    CHECK(select(d,context,selection,Ref::entity(children[0].id),true,error));
    // Keep the group as the active origin for a stable row axis.
    selection.active=Ref::entity(root);
    CHECK(clipboard.copy(d,selection,context,meshes,error));CHECK(clipboard.paste(d,context,options,next,result,error));
    CHECK(next.placements.size()==4&&next.lights.size()==2&&next.materials.size()==1);
    CHECK(near(next.placements[2].x,14)&&near(next.placements[3].x,24)&&near(next.lights[1].x,15));
    CHECK(world_entity::find(next.entities,result.created)->name=="Street section copy");
    // Copy failure retains the previous useful clipboard.
    Selection empty;CHECK(!clipboard.copy(d,empty,context,meshes,error)&&clipboard.label()=="Street section copy");
    world_entity::find(d.entities,root)->locked=true;
    CHECK(!clipboard.copy(d,selection,context,meshes,error)&&clipboard.label()=="Street section copy");
    // Unknown geometry never silently uses a one-metre cube for snapping.
    d={};d.placements={{"missing",0,0}};CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));selection.clear();
    CHECK(select(d,context,selection,from_binding(d,{world_entity::Kind::Placement,0}),false,error));
    CHECK(clipboard.copy(d,selection,context,meshes,error));
    CHECK(!clipboard.paste(d,context,options,next,result,error)&&error.find("bounds")!=std::string::npos);
    options.snap=false;CHECK(clipboard.paste(d,context,options,next,result,error)&&near(next.placements[1].x,2));
    options.gap=std::numeric_limits<double>::quiet_NaN();CHECK(!clipboard.paste(d,context,options,next,result,error));
    options={};options.direction=4;CHECK(!clipboard.paste(d,context,options,next,result,error));
    // Repair the rejected scene shape reported in the user's recovery: two
    // identical authored roots, including an old single-model prefab wrapper.
    d={};d.placements={{"rock0",0,0},{"rock0",0,0},{"rock0",4.25,0}};
    CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));
    const Id duplicate=from_binding(d,{world_entity::Kind::Placement,1}).id;
    Id wrapper=0;CHECK(world_entity::add(d.entities,0,"rock0 copy",{},{0,10,0,0,1},wrapper,error));
    CHECK(world_entity::reparent(d.entities,duplicate,wrapper,true,error));
    CHECK(separate_overlapping_models(d,context,meshes,next,result,error)&&result.changed);
    CHECK(near(next.placements[0].x,0)&&near(next.placements[1].x,8.5)&&near(next.placements[2].x,4.25));
    CHECK(near(next.placements[1].offset_y,0)&&near(world_entity::find(next.entities,wrapper)->local.x,8.5));
    CHECK(from_binding(next,{world_entity::Kind::Placement,1}).id==duplicate&&next.placements.size()==3);
    CHECK(!result.selection.active.empty()&&project(next,context,result.selection).selected_added==1);
    CHECK(separate_overlapping_models(next,context,meshes,back,result,error)&&!result.changed);
    world_entity::find(d.entities,wrapper)->locked=true;
    CHECK(!separate_overlapping_models(d,context,meshes,back,result,error)&&near(back.placements[1].x,8.5));
    options={};house={};house.alias="tavern";d={};d.placements={house};CHECK(world_authoring::synchronize(d,d.entities,identities,ground,nullptr,error));selection.clear();
    CHECK(select(d,context,selection,from_binding(d,{world_entity::Kind::Placement,0}),false,error));CHECK(clipboard.copy(d,selection,context,meshes,error));
    while(d.placements.size()<world_studio::placement_limit)d.placements.push_back(house);
    CHECK(!clipboard.paste(d,context,options,next,result,error)&&error.find("capacity")!=std::string::npos);
    std::cout<<"world_designer_clipboard: "<<checks<<" checks passed\n";
}
