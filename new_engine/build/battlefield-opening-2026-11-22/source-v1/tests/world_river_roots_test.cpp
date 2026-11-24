#include "engine/world_landscape_protection.h"
#include "engine/world_terrain_authoring.h"
#include "engine/world_surface_field.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace world_landscape_protection;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);return 1;}}while(0)
static float flat(double,double){return 176;}
static bool protected_root(double x,double z,const void *context){return static_cast<const DryRoots*>(context)->blocked(x,z);}
static std::string bytes(const world_studio::Document &d){std::string out,error;assert(world_studio::encode(d,out,error));return out;}
int main(int argc,char **argv){
 CHECK(argc==2);std::string error;world_studio::Document jungle;
 CHECK(world_studio::load(argv[1],jungle,error));
 CHECK(jungle.placements.size()==960);
 const auto plant=jungle.placements[938];
 CHECK(plant.alias=="explore_thicket"&&std::abs(plant.x+26611)<1e-8&&std::abs(plant.z-9732.496705929)<1e-7);
 DryRoots roots;CHECK(roots.prepare(jungle,{},error));CHECK(roots.roots.size()==960);
 CHECK(roots.blocked(plant.x,plant.z));
 for(double dx:{-1.,0.,1.})for(double dz:{-1.,0.,1.})CHECK(roots.blocked(plant.x+dx,plant.z+dz));
 // Old nine-point FindOpen sampling can miss a shrub in the brush interior.
 CHECK(!roots.blocked(plant.x+2.1,plant.z));
 CHECK(roots.overlaps(plant.x+2.1,plant.z,16));
 world_studio::Document candidate;candidate.placements.push_back(plant);
 const std::vector<world_terrain_authoring::Point> route={{plant.x-8,plant.z,176},{plant.x+8,plant.z,175.98}};
 // First establish that the old generic source admits this actual jungle
 // shrub position and creates water there; root-specific protection matters.
 auto old=candidate;world_terrain_authoring::Report old_report;
 CHECK(world_terrain_authoring::simple_river(old,route,8,4,1.2f,{flat,nullptr,nullptr},old_report,error));
 float water=0,shore=0;CHECK(old.surface_field&&world_surface::river(*old.surface_field,plant.x,plant.z,water,shore));
 CHECK(water+.1f>=176);
 const auto before=bytes(candidate);world_terrain_authoring::Report report;report.changed_nodes=777;report.added_ribbons=99;
 CHECK(!world_terrain_authoring::simple_river(candidate,route,8,4,1.2f,{flat,protected_root,&roots},report,error));
 CHECK(error.find("protected")!=std::string::npos);
 CHECK(bytes(candidate)==before&&candidate.surface_field==nullptr);
 CHECK(report.changed_nodes==777&&report.added_ribbons==99);
 // The same existing plant remains terrain-brushable with the ordinary source.
 auto brush=candidate;world_terrain_authoring::Report brush_report;
 CHECK(world_terrain_authoring::simple_brush(brush,plant.x,plant.z,8,1,world_terrain_authoring::SimpleBrushMode::Raise,176,1,{flat,nullptr,nullptr},brush_report,error));
 CHECK(world_studio::height_delta(brush,plant.x,plant.z)>.5);
 // Search a genuinely nearby clear route against ALL 960 real jungle roots.
 bool found=false;double clear_distance=0;
 for(double distance:{24.,32.,48.,64.,96.,128.,192.,256.})for(unsigned direction=0;direction<16&&!found;++direction){
  const double angle=direction*6.283185307179586/16,x=plant.x+distance*std::cos(angle),z=plant.z+distance*std::sin(angle);
  if(roots.overlaps(x,z,16))continue;
  auto clear=candidate;world_terrain_authoring::Report clear_report;
  const std::vector<world_terrain_authoring::Point> alternative={{x-8,z,176},{x+8,z,175.98}};
  if(world_terrain_authoring::simple_river(clear,alternative,8,4,1.2f,{flat,protected_root,&roots},clear_report,error)){
   CHECK(clear.surfaces.ribbons.size()==1&&clear_report.added_ribbons==1);clear_distance=distance;found=true;
  }
 }
 CHECK(found&&clear_distance<=256);
 // Effective source overrides, full transforms, and inherited visibility are
 // used; no baseline-root or collision/vegetation classification shortcut.
 world_studio::Pose source;source.alias="decorative_plant";source.root={10,176,20};
 world_studio::Document edits;world_studio::Override move;move.index=0;move.x=90;move.z=120;move.pitch=45;move.roll=30;move.sx=2;move.sy=3;move.sz=.5f;edits.overrides.push_back(move);
 world_studio::Placement added;added.alias="nonsolid_fern";added.x=150;added.z=160;edits.placements.push_back(added);
 DryRoots transformed;CHECK(transformed.prepare(edits,{source},error));
 CHECK(transformed.blocked(90,120)&&!transformed.blocked(10,20)&&transformed.blocked(150,160));
 edits.overrides[0].hidden=true;CHECK(transformed.prepare(edits,{source},error));CHECK(!transformed.blocked(90,120));
 world_entity::Id parent=0,child=0;CHECK(world_entity::add(edits.entities,0,"Hidden parent",{}, {},parent,error));
 CHECK(world_entity::add(edits.entities,parent,"Decorative fern",{world_entity::Kind::Placement,0},{150,176,160},child,error));
 world_entity::find(edits.entities,parent)->visible=false;
 CHECK(transformed.prepare(edits,{source},error)&&transformed.roots.empty());
 auto invalid=source;invalid.root.x=std::numeric_limits<double>::quiet_NaN();edits.overrides.clear();
 CHECK(!transformed.prepare(edits,{invalid},error)&&transformed.roots.empty());
 CHECK(transformed.blocked(std::numeric_limits<double>::quiet_NaN(),0));
 std::printf("PASS %u river dry-root checks; actual jungle shrub old-admits/new-rejects, 2m lattice safety, atomic document/report failure, nearby %.0fm clear route, effective source transforms/hidden hierarchy, and ordinary terrain brush preserved\n",checks,clear_distance);
}
