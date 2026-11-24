#include "engine/world_landscape_protection.h"
#include "engine/world_terrain_authoring.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace world_landscape_protection;
struct Context {std::vector<Footprint> solids;Roads roads;};
static bool blocked(double x,double z,const void *v){const auto &c=*static_cast<const Context*>(v);for(const auto &f:c.solids)if(contains(f,x,z))return true;return c.roads.blocked(x,z);}
static float flat(double,double){return 20;}
int main(){
 using namespace world_studio;std::string error;Document draft;
 Pose original;original.alias="house_fixture";original.root={0,20,0};original.minimum={-1,0,-1};original.maximum={1,20,1};
 std::vector<Pose> sources={original};Override edit;edit.index=0;edit.sx=8;edit.sz=.5f;draft.overrides.push_back(edit);
 Pose source;assert(effective_pose(draft,sources,0,source));const auto wide=footprint(source,3);
 assert(contains(wide,10,0));assert(!contains(wide,14,0));assert(wide.x==0&&wide.z==0);
 Placement house;house.alias="house_fixture";house.x=40;house.scale=.5f;house.pitch=90;house.sy=1.5f;draft.placements.push_back(house);
 Pose added;assert(effective_pose(draft,sources,8192,added));added.minimum=original.minimum;added.maximum=original.maximum;
 const auto tilted=footprint(added,2);assert(contains(tilted,40,-14));assert(!contains(tilted,40,-20));assert(!contains(tilted,80,0));
 // Independent geometric expectations: a tall20m model pitched90deg extends
 // 15m horizontally after1.5Y and.5uniform scale, plus the2m safety margin.
 assert(tilted.radius>17&&tilted.radius<17.1);
 Context context;context.solids={wide,tilted};
 world_surface::Ribbon road;road.name="Draft road";road.width=4;road.shoulder=2;road.smooth=false;road.points={{-20,20,20},{20,20,20}};
 world_surface::Ribbon river=road;river.name="Editable river";river.kind=world_surface::RibbonKind::River;for(auto &p:river.points)p.z=-20;
 world_surface::Data live;assert(context.roads.prepare(live,error)&&!context.roads.blocked(0,20));
 draft.surfaces.ribbons={road,river};assert(context.roads.prepare(draft.surfaces,error));
 assert(context.roads.blocked(0,20)&&context.roads.blocked(0,23.99)&&!context.roads.blocked(0,24.01));
 assert(!context.roads.blocked(0,-20)); // Water never enters the road guard.
 auto bad=draft.surfaces;bad.ribbons[0].points[1].x=std::numeric_limits<double>::quiet_NaN();
 assert(!context.roads.prepare(bad,error)&&context.roads.blocked(0,20));
 world_terrain_authoring::Report report;
 assert(world_terrain_authoring::simple_brush(draft,20,0,64,2,world_terrain_authoring::SimpleBrushMode::Raise,20,1,{flat,blocked,&context},report,error));
 assert(report.changed_nodes>0&&report.protected_nodes>0);
 assert(height_delta(draft,10,0)==0); // nonuniform source footprint survives
 assert(height_delta(draft,40,-14)==0); // tilted added solid footprint survives
 assert(height_delta(draft,0,20)==0); // draft road surface survives
 assert(height_delta(draft,0,-20)>0); // river was not accidentally a road mask
 assert(height_delta(draft,20,0)>0); // ordinary open terrain still changes
 std::puts("Landscape protection: transformed source + tilted/nonuniform added solid, translated root, draft road shoulder, river exclusion, atomic bad-cache rejection, and real brush protected/open samples PASS");
}
