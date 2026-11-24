#include "engine/world_studio_tools.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <unistd.h>
using namespace world_studio;
int main(){
 Pose p;p.alias="starting_crate";p.root={100,20,-300};p.minimum={-1,0,-.5};p.maximum={1,2,.5};p.yaw=.7;p.scale=2;
 double distance=0;assert(ray_bounds({{100,22,-310},{0,0,1}},p,distance)&&distance>8&&distance<10);assert(!ray_bounds({{110,22,-310},{0,0,1}},p,distance));assert(ray_bounds({{100,22,-300},{0,1,0}},p,distance)&&distance==0);assert(!ray_bounds({{100,22,-300},{0,0,0}},p,distance));
 Vec hit;assert(ray_plane({{0,10,0},{0,-1,0}},{0,2,0},{0,1,0},hit)&&hit.y==2);assert(!ray_plane({{0,10,0},{1,0,0}},{0,2,0},{0,1,0},hit));
 Document d;d.source_signature="0123456789abcdef";Placement added;added.alias=p.alias;added.x=104;added.z=-300;d.placements.push_back(added);std::vector<Pose> source={p};std::string error;
 const std::vector<unsigned> keys={0,8192};assert(transform_selection(d,source,keys,{100,20,-300},{2,3,4},90,1.5,error));assert(d.overrides.size()==1&&d.overrides[0].x==102&&d.overrides[0].z==-296&&d.overrides[0].offset_y==3);assert(std::abs(d.placements[0].x-102)<1e-5&&std::abs(d.placements[0].z+302)<1e-5);Document before=d;assert(!transform_selection(d,source,keys,{},{},0,10,error));assert(d.placements[0].scale==before.placements[0].scale);
 d.groups.push_back({"Market objects",keys});std::vector<unsigned> created;assert(duplicate_selection(d,source,keys,{5,0,0},created,error)&&created.size()==2&&d.placements.size()==3);d.groups.push_back({"Copies",created});erase_placement(d,0);assert(d.groups[0].members==std::vector<unsigned>{0});assert(d.groups[1].members==std::vector<unsigned>({8192,8193}));assert(delete_selection(d,source,{0,8193},error)&&d.overrides[0].hidden&&d.placements.size()==1);assert(d.groups[1].members==std::vector<unsigned>{8192});
 d.assets.push_back({"user_crate","imports/user_crate/revision/asset",2,1,0,220,60,1});d.environment.path="environments/test.hdr";d.environment.enabled=true;assert(valid(d,error));
 char path[]="/tmp/valwood-studio-tools-XXXXXX";int fd=mkstemp(path);close(fd);assert(save(path,d,error));Document loaded;assert(load(path,loaded,error));assert(loaded.groups.size()==2&&loaded.assets.size()==1&&loaded.environment.enabled&&loaded.overrides[0].offset_y==3);unlink(path);
 d.groups.push_back({"Bad",{8194}});assert(!valid(d,error));d.groups.pop_back();d.assets[0].stem="../../outside";assert(!valid(d,error));
 // Imported city/stronghold source props can be tilted and stretched before
 // any Studio override exists. A viewport translation must not reset that
 // authored shape, and hiding/restoring it must retain the same transform.
 Pose tilted=p;tilted.pitch=.31;tilted.roll=-.22;tilted.sx=1.4;tilted.sy=.7;tilted.sz=1.8;
 const std::vector<Pose> shaped={tilted};Document drag;drag.source_signature="0123456789abcdef";
 assert(transform_selection(drag,shaped,{0},tilted.root,{3,1,2},15,1,error));Pose after;
 assert(effective_pose(drag,shaped,0,after));
 assert(std::abs(after.pitch-tilted.pitch)<1e-6&&std::abs(after.roll-tilted.roll)<1e-6);
 assert(std::abs(after.sx-tilted.sx)<1e-6&&std::abs(after.sy-tilted.sy)<1e-6&&std::abs(after.sz-tilted.sz)<1e-6);
 Document hidden;hidden.source_signature="0123456789abcdef";
 assert(delete_selection(hidden,shaped,{0},error)&&effective_pose(hidden,shaped,0,after)&&after.hidden);
 assert(std::abs(after.pitch-tilted.pitch)<1e-6&&std::abs(after.roll-tilted.roll)<1e-6);
 assert(std::abs(after.sx-tilted.sx)<1e-6&&std::abs(after.sy-tilted.sy)<1e-6&&std::abs(after.sz-tilted.sz)<1e-6);
 std::puts("PASS Studio tools: oriented ray selection, drag-plane math, atomic group transforms, duplication/delete remapping and v2 persistence");
}
