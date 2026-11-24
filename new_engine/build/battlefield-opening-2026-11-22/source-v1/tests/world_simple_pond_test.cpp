#include "engine/world_terrain_authoring.h"
#include "engine/world_surface_field.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>
namespace {
unsigned checks=0;
void check(bool ok,const char *what){++checks;if(!ok)throw std::runtime_error(what);}
float flat(double,double){return 30;}
float slope(double x,double z){return float(30+x*.18-z*.11);}
float cliff(double x,double){return float(300+x*2);}
float invalid(double,double){return std::numeric_limits<float>::quiet_NaN();}
struct Root {double x,z;};
bool root(double x,double z,const void *data){const auto &r=*static_cast<const Root *>(data);return std::hypot(x-r.x,z-r.z)<2;}
std::string bytes(const world_studio::Document &d){std::string text,error;check(world_studio::encode(d,text,error),"encode scene");return text;}
double height(const world_studio::Document &d,float(*base)(double,double),double x,double z){return base(x,z)+world_studio::height_delta(d,x,z)+(d.surface_field?world_surface::terrain_offset(*d.surface_field,x,z):0);}
void enclosed(const world_studio::Document &d,float(*base)(double,double)){
 const auto &w=d.waters.back();
 check(w.source==3&&w.enabled,"real enabled custom water body");
 check(w.level-height(d,base,w.x,w.z)>=.08,"actual triangular center submerged");
 // Denser than Apply's 32 cardinal/diagonal rim probes: no clipped water
 // edge may escape between those probes or across a 128m terrain seam.
 for(unsigned i=0;i<2048;i++){const double a=i*6.283185307179586/2048;
  check(height(d,base,w.x+std::cos(a)*w.radius_x,w.z+std::sin(a)*w.radius_z)>=w.level+.035,"entire water mesh cutoff has a dry rim");}
 const double outer=w.radius_x+std::max(4.,double(w.radius_x)*.25)+2.9;
 for(unsigned i=0;i<128;i++){const double a=i*6.283185307179586/128,x=w.x+std::cos(a)*outer,z=w.z+std::sin(a)*outer;
  check(std::abs(height(d,base,x,z)-base(x,z))<.0001,"ground beyond grading and interpolation fringe stays unchanged");}
 std::string error;check(world_studio::valid(d,error),"shared terrain seam and document invariants");
}
}
int main(){try{
 using namespace world_terrain_authoring;std::string error;Report report;
 world_studio::Document pond;
 check(simple_pond(pond,127.3,128.8,6,1.5,Source{flat},report,error),"default arbitrary-center pond across tile seams");
 check(report.changed_nodes>0&&report.added_tiles==4&&report.added_ribbons==0,"pond reports real sculpt without adding a river");enclosed(pond,flat);
 const auto &w=pond.waters.front();check(w.radius_x==6&&w.radius_z==6,"water keeps selected dimensions");
 for(float radius:{6.f,24.f,32.f})for(float depth:{.5f,3.f,6.f})for(double offset:{.01,.49,.99,1.51}){
  world_studio::Document lake;
  check(simple_pond(lake,128+offset,128-offset,radius,depth,Source{slope},report,error),"pond/lake grades a closed bank on a slope at arbitrary grid phase");enclosed(lake,slope);
 }
 // An existing sculpt changes both the chosen water level and the resulting
 // bowl; it must not use the base terrain as if the user's hill were absent.
 world_studio::Document sculpted;
 check(simple_brush(sculpted,128,128,64,4,SimpleBrushMode::Raise,30,1,Source{flat},report,error),"existing hill fixture");
 const double old_center=height(sculpted,flat,128,128);
 check(simple_pond(sculpted,128,128,12,2,Source{flat},report,error),"pond on previously sculpted ground");
 check(std::abs(sculpted.waters.back().level-(old_center-.3))<.0001,"pond respects existing hill height");
 world_studio::Document guarded;const auto before=bytes(guarded);Report sentinel;sentinel.changed_nodes=123;sentinel.added_ribbons=77;
 Root dry_bank{140,128};
 check(!simple_pond(guarded,128,128,12,2,Source{flat,root,&dry_bank},sentinel,error),"dry bank object is protected even when its terrain sample need not move");
 check(bytes(guarded)==before&&sentinel.changed_nodes==123&&sentinel.added_ribbons==77,"protected pond leaves document and report unchanged");
 Root between{128.9,128.9};check(!simple_pond(guarded,128,128,6,2,Source{flat,root,&between},sentinel,error),"between-grid root protection checked");
 check(!simple_pond(guarded,128,128,6,2,Source{invalid},sentinel,error)&&bytes(guarded)==before,"nonfinite base is rejected atomically");
 check(!simple_pond(guarded,50395,0,6,2,Source{flat},sentinel,error)&&bytes(guarded)==before,"full banks and fringe must fit map bounds");
 check(!simple_pond(guarded,128,128,32,6,Source{cliff},sentinel,error)&&bytes(guarded)==before,"excessive lake slope rejects rather than clipping its bank");
 for(float radius:{0.f,5.9f,32.1f,std::numeric_limits<float>::quiet_NaN()})check(!simple_pond(guarded,128,128,radius,2,Source{flat},sentinel,error)&&bytes(guarded)==before,"invalid pond dimensions are atomic");
 for(float depth:{.49f,6.1f,std::numeric_limits<float>::infinity()})check(!simple_pond(guarded,128,128,6,depth,Source{flat},sentinel,error)&&bytes(guarded)==before,"invalid pond depth is atomic");
 const auto pond_before=bytes(pond);const auto old_field=pond.surface_field;
 check(!simple_pond(pond,128,128,6,2,Source{flat},sentinel,error)&&bytes(pond)==pond_before&&pond.surface_field==old_field,"overlapping pond rejected with original field identity intact");
 world_studio::Document full;for(unsigned i=0;i<8;i++){world_studio::Water water;water.source=3;water.x=double(i)*100;water.z=400;water.level=30;full.waters.push_back(water);}
 const auto full_before=bytes(full);check(!simple_pond(full,128,128,6,2,Source{flat},sentinel,error)&&bytes(full)==full_before,"water budget failure preserves scene");
 world_studio::Document paths;
 check(simple_river(paths,{{384,400,30},{480,400,30}},6,3,1.5,Source{flat},report,error),"existing distant river fixture");
 const float offset=world_surface::terrain_offset(*paths.surface_field,432,400);const auto existing_path=paths.surfaces.ribbons.front();
 check(simple_pond(paths,128,128,24,3,Source{flat},report,error),"pond coexists with distant authored river");
 check(paths.surfaces.ribbons.size()==1&&paths.surfaces.ribbons[0].points.size()==existing_path.points.size()&&world_surface::terrain_offset(*paths.surface_field,432,400)==offset,"rebaking preserves unrelated river profile");enclosed(paths,flat);
 const auto paths_before=bytes(paths);const auto path_field=paths.surface_field;
 check(!simple_pond(paths,432,410,6,2,Source{flat},sentinel,error)&&bytes(paths)==paths_before&&paths.surface_field==path_field,"river and banks protected without relying on UI sampler");
 auto road=paths.surfaces.ribbons[0];road.kind=world_surface::RibbonKind::Road;paths.surfaces.ribbons[0]=road;
 const auto road_before=bytes(paths);check(!simple_pond(paths,432,410,6,2,Source{flat},sentinel,error)&&bytes(paths)==road_before,"road grade is protected too");
 world_studio::History history;world_studio::Document undoable;history.checkpoint(undoable);
 check(simple_pond(undoable,64.7,64.3,12,2,Source{flat},report,error),"undo fixture");const auto after=bytes(undoable);
 check(history.undo(undoable)&&undoable.terrain.empty()&&undoable.waters.empty(),"one undo restores terrain and water together");
 check(history.redo(undoable)&&bytes(undoable)==after,"one redo restores both");
 const auto path="/tmp/valwood-simple-pond-"+std::to_string(getpid())+".studio";
 check(world_studio::save(path.c_str(),paths,error),"water/terrain/river persistence");world_studio::Document reopened;
 check(world_studio::load(path.c_str(),reopened,error)&&bytes(reopened)==bytes(paths),"pond and lake dimensions, terrain and paths reopen exactly");unlink(path.c_str());
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"water_radius_min\":6,\"water_radius_max\":32,\"gpu\":false}\n";return 0;
 }catch(const std::exception &e){std::cerr<<"Simple pond failed after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
