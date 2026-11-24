#include "engine/world_terrain_authoring.h"
#include "engine/world_surface_field.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unistd.h>
namespace {
unsigned checks=0;
void check(bool ok,const char *what){++checks;if(!ok)throw std::runtime_error(what);}
float flat(double,double){return 20;}
float spike(double x,double z){return float(20+12*std::exp(-((x-64)*(x-64)+(z-64)*(z-64))/8));}
float invalid(double,double){return std::numeric_limits<float>::quiet_NaN();}
bool protected_square(double x,double z,const void *){return std::abs(x)<=8&&std::abs(z)<=8;}
bool protected_river(double x,double z,const void *){return std::hypot(x-64,z-64)<4;}
bool protect_all(double,double,const void *){return true;}
std::string bytes(const world_studio::Document &d){std::string text,error;check(world_studio::encode(d,text,error),"document encode");return text;}
void seams(const world_studio::Document &d){
 std::string error;check(world_studio::valid(d,error),"valid document and all exterior/shared boundaries");
 for(const auto &a:d.terrain)for(const auto &b:d.terrain){
  if(a.x+1==b.x&&a.z==b.z)for(unsigned i=0;i<65;i++)check(a.delta[i*65+64]==b.delta[i*65],"exact X seam");
  if(a.z+1==b.z&&a.x==b.x)for(unsigned i=0;i<65;i++)check(a.delta[64*65+i]==b.delta[i],"exact Z seam");
 }
}
}
int main(){
 try{
 using namespace world_terrain_authoring;using Mode=SimpleBrushMode;
 std::string error;Report report;const Source source{flat};
 world_studio::Document large;
 check(simple_brush(large,1.3,-12.6,256,8,Mode::Raise,20,1,source,report,error),"maximum arbitrary-center brush succeeds");
 check(large.terrain.size()>16&&large.terrain.size()<=25,"large brush supports more than old 16 tiles");
 check(report.changed_nodes>40000&&report.added_tiles==large.terrain.size(),"large brush actionable report");
 check(world_studio::height_delta(large,2,-12)>7.9f,"large raise affects ground");seams(large);
 check(simple_brush(large,1.3,-12.6,256,8,Mode::Lower,20,1,source,report,error),"large lower succeeds");
 check(std::abs(world_studio::height_delta(large,2,-12))<.00001,"raise/lower inverse within float precision");seams(large);
 world_studio::Document flattened;
 check(simple_brush(flattened,64,64,80,8,Mode::Flatten,27,0,source,report,error),"flatten succeeds");
 check(world_studio::height_delta(flattened,64,64)>6.8&&world_studio::height_delta(flattened,64,64)<7,"flatten approaches chosen plane");
 check(world_studio::height_delta(flattened,146,64)==0,"brush exterior unchanged");seams(flattened);
 world_studio::Document smoothed;
 check(simple_brush(smoothed,64,64,64,8,Mode::Smooth,20,0,Source{spike},report,error),"smooth succeeds");
 check(world_studio::height_delta(smoothed,64,64)<-4,"smooth lowers sharp summit");
 check(world_studio::height_delta(smoothed,68,64)>0,"smooth deposits into neighboring low samples");seams(smoothed);
 world_studio::Document hills,hills_repeat,hills_other;
 check(simple_brush(hills,-14.4,18.7,256,8,Mode::RollingHills,20,512,source,report,error),"hills succeed");
 check(simple_brush(hills_repeat,-14.4,18.7,256,8,Mode::RollingHills,20,512,source,report,error),"repeat hills succeed");
 check(bytes(hills)==bytes(hills_repeat),"same seed exact deterministic terrain");
 check(simple_brush(hills_other,-14.4,18.7,256,8,Mode::RollingHills,20,513,source,report,error),"different hills seed succeeds");
 check(bytes(hills)!=bytes(hills_other),"seed alters hills");
 float lowest=0,highest=0;for(const auto &tile:hills.terrain)for(float v:tile.delta){lowest=std::min(lowest,v);highest=std::max(highest,v);}
 check(lowest<-.1f&&highest>.1f,"rolling hills contain hills and valleys");seams(hills);
 world_studio::Document guarded;
 check(simple_brush(guarded,0,0,96,4,Mode::Raise,20,1,source,report,error),"initial protected fixture");
 const auto center=world_studio::height_delta(guarded,0,0),protected_edge=world_studio::height_delta(guarded,8,8);
 check(simple_brush(guarded,0,0,96,8,Mode::Lower,20,1,Source{flat,protected_square},report,error),"brush skips protected samples");
 check(report.protected_nodes==81&&report.changed_nodes>0,"protected sample report count");
 check(world_studio::height_delta(guarded,0,0)==center&&world_studio::height_delta(guarded,8,8)==protected_edge,"protected existing offsets unchanged exactly");seams(guarded);
 world_studio::Document all_protected;
 check(simple_brush(all_protected,0,0,96,4,Mode::Raise,20,1,Source{flat,protect_all},report,error),"all-protected no-op succeeds");
 check(report.changed_nodes==0&&report.added_tiles==0&&report.protected_nodes>0&&all_protected.terrain.empty(),"all-protected brush allocates no tiles");
 world_studio::Document clamp;
 for(int i=0;i<6;i++)check(simple_brush(clamp,64,64,32,8,Mode::Raise,20,1,source,report,error),"repeated raise with clamp");
 check(world_studio::height_delta(clamp,64,64)==32&&report.clamped_nodes>0,"positive offset capped at 32m");
 for(int i=0;i<10;i++)check(simple_brush(clamp,64,64,32,8,Mode::Lower,20,1,source,report,error),"repeated lower with clamp");
 check(world_studio::height_delta(clamp,64,64)==-32&&report.clamped_nodes>0,"negative offset capped at -32m");seams(clamp);
 world_studio::Document full;for(unsigned i=0;i<world_studio::tile_limit;i++){world_studio::TerrainTile tile;tile.x=int(i);tile.z=80;full.terrain.push_back(tile);}
 world_studio::Document exact_capacity;for(unsigned i=0;i<world_studio::tile_limit-25;i++){world_studio::TerrainTile tile;tile.x=int(i);tile.z=80;exact_capacity.terrain.push_back(tile);}
 check(simple_brush(exact_capacity,1.3,-12.6,256,1,Mode::Raise,20,1,source,report,error)&&exact_capacity.terrain.size()==64,"large operation can fill exact 64-tile capacity");seams(exact_capacity);
 const auto full_before=bytes(full);Report sentinel;sentinel.changed_nodes=123;sentinel.added_ribbons=77;
 check(!simple_brush(full,0,0,256,8,Mode::Raise,20,1,source,sentinel,error),"full terrain capacity rejects");
 check(bytes(full)==full_before&&sentinel.changed_nodes==123&&sentinel.added_ribbons==77,"capacity failure preserves document/report");
 for(float radius:{7.f,257.f,std::numeric_limits<float>::quiet_NaN()})check(!simple_brush(full,0,0,radius,1,Mode::Raise,20,1,source,sentinel,error)&&bytes(full)==full_before,"invalid brush radius atomic rejection");
 check(!simple_brush(full,50300,0,256,1,Mode::Raise,20,1,source,sentinel,error)&&bytes(full)==full_before,"world bounds rejected atomically");
 check(!simple_brush(full,0,0,16,1,Mode::Raise,20,1,Source{invalid},sentinel,error)&&bytes(full)==full_before,"invalid sampler rejected atomically");
 check(!simple_brush(full,0,0,16,1,Mode(999),20,1,source,sentinel,error)&&bytes(full)==full_before,"invalid tool rejected atomically");
 world_studio::History history;world_studio::Document undoable;history.checkpoint(undoable);
 check(simple_brush(undoable,64,64,64,2,Mode::Raise,20,1,source,report,error),"undo fixture brush");
 const auto undo_after=bytes(undoable);check(history.undo(undoable)&&undoable.terrain.empty(),"one undo restores whole brush");
 check(history.redo(undoable)&&bytes(undoable)==undo_after,"one redo restores whole brush");
 // This helper creates persisted real water and a carved field, not a dry
 // sculpt-only corridor. A high middle ground pick cannot cause an uphill
 // water segment because the profile interpolates downhill by arc distance.
 std::vector<Point> points{{16,64,20},{64,64,25},{112,64,21}};world_studio::Document river;
 check(simple_river(river,points,6,3,1.5,source,report,error),"real river succeeds");
 check(river.surfaces.ribbons.size()==1&&report.added_ribbons==1&&report.changed_nodes>0,"real river document/report");
 check(river.surface_field&&river.surface_field->ribbons.size()==1,"real river field retained");
 const auto &r=river.surfaces.ribbons.front();check(r.kind==world_surface::RibbonKind::River&&!r.smooth&&r.layer==-1,"linear true-water ribbon");
 for(unsigned i=1;i<r.points.size();i++)check(r.points[i].y<r.points[i-1].y,"strict downhill control heights");
 world_surface::BakedRibbon baked;check(world_surface::bake_ribbon(r,baked,error),"river renderer baking accepted");
 for(unsigned i=1;i<baked.samples.size();i++)check(baked.samples[i].position.y<baked.samples[i-1].position.y,"no spline uphill overshoot");
 float level=0,shore=0;check(world_surface::river(*river.surface_field,64,64,level,shore)&&shore>.9f,"real water query covers channel");
 check(world_surface::terrain_offset(*river.surface_field,64,64)<-1.4f,"real river channel carved");
 const auto river_before=bytes(river);check(!simple_river(river,points,3,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"small river width rejected atomically");
 check(!simple_river(river,points,6,1,1.5,source,sentinel,error)&&bytes(river)==river_before,"small bank rejected atomically");
 check(!simple_river(river,points,6,3,4.1,source,sentinel,error)&&bytes(river)==river_before,"depth overflow rejected atomically");
 auto duplicate=points;duplicate[1]=duplicate[0];check(!simple_river(river,duplicate,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"duplicate river point rejected atomically");
 auto distant=points;distant[1].x=500;check(!simple_river(river,distant,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"river point spacing bounded");
 std::vector<Point> long_river;for(unsigned i=0;i<6;i++)long_river.push_back({double(i*220),64,20});
 check(!simple_river(river,long_river,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"river length bounded");
 auto many=points;many.resize(33,points.back());check(!simple_river(river,many,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"river point count bounded");
 std::vector<Point> bounds{{50398,64,20},{50390,64,20}};check(!simple_river(river,bounds,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"river banks atlas bounds");
 std::vector<Point> deep{{16,64,100},{112,64,100}};check(!simple_river(river,deep,6,3,1.5,source,sentinel,error)&&bytes(river)==river_before,"river earthwork 32m cap");
 world_studio::Document protected_water;const auto empty=bytes(protected_water);
 check(!simple_river(protected_water,points,6,3,1.5,Source{flat,protected_river},sentinel,error),"river crossing protected ground rejected");
 check(bytes(protected_water)==empty&&sentinel.changed_nodes==123&&sentinel.added_ribbons==77,"protected river failure preserves document/report");
 const auto previous_field=river.surface_field;
 check(!simple_river(river,{{16,96,100},{112,96,100}},6,3,1.5,source,sentinel,error)&&river.surface_field==previous_field,"failed river preserves existing derived field identity");
 // The atlas accepts only +/-32m for the SUM of sculpt and spline offsets.
 // Both independently valid 32m components must not silently disappear.
 world_studio::Document sum_fixture;world_studio::TerrainTile sum_tile;sum_tile.x=0;sum_tile.z=0;sum_tile.delta[32*65+32]=32;sum_fixture.terrain.push_back(sum_tile);
 world_surface::TerrainField sum_field;world_surface::TerrainTile field_tile;field_tile.x=0;field_tile.z=0;field_tile.delta[32*65+32]=32;sum_field.tiles.push_back(field_tile);
 check(!combined_height_valid(sum_fixture,sum_field,error),"32m sculpt plus 32m derived offset rejected");
 // Node sums alone are insufficient: triangular sculpt and bilinear field
 // interpolation can exceed the bound along the terrain cell diagonal.
 auto diagonal=sum_fixture;diagonal.terrain[0].delta.fill(0);const unsigned cell=32*65+32;
 diagonal.terrain[0].delta[cell+1]=32;diagonal.terrain[0].delta[cell+65]=32;
 auto diagonal_field=sum_field;diagonal_field.tiles[0].delta.fill(0);diagonal_field.tiles[0].delta[cell]=32;diagonal_field.tiles[0].delta[cell+66]=32;
 check(!combined_height_valid(diagonal,diagonal_field,error)&&error.find("between")!=std::string::npos,"between-node combined overshoot rejected");
 for(float &v:diagonal.terrain[0].delta)v=-v;
 for(float &v:diagonal_field.tiles[0].delta)v=-v;
 check(!combined_height_valid(diagonal,diagonal_field,error),"negative between-node combined overshoot rejected");
 world_studio::Document plateau;world_studio::TerrainTile plateau_tile;plateau_tile.x=0;plateau_tile.z=0;
 for(unsigned z=4;z<61;z++)for(unsigned x=4;x<61;x++)plateau_tile.delta[z*65+x]=32;
 plateau.terrain.push_back(plateau_tile);const auto plateau_before=bytes(plateau);
 check(!simple_river(plateau,{{16,64,52},{112,64,52}},6,3,1.5,source,sentinel,error)&&bytes(plateau)==plateau_before,"river over 32m plateau rejects combined bank displacement atomically");
 world_studio::Document elevated_river;check(simple_river(elevated_river,{{16,64,50},{112,64,50}},6,3,1.5,source,report,error),"valid elevated river fixture");
 const auto elevated_before=bytes(elevated_river);const auto elevated_field=elevated_river.surface_field;
 check(!simple_brush(elevated_river,64,64,96,8,Mode::Lower,20,1,source,sentinel,error),"terrain edit rebakes existing river and rejects excessive cut");
 check(bytes(elevated_river)==elevated_before&&elevated_river.surface_field==elevated_field&&sentinel.changed_nodes==123,"failed terrain plus river operation preserves document/cache/report");
 world_studio::Document full_paths=river;for(unsigned i=1;i<world_surface::ribbon_limit;i++){auto path=r;path.name="Existing "+std::to_string(i);full_paths.surfaces.ribbons.push_back(path);}
 const auto paths_before=bytes(full_paths);check(!simple_river(full_paths,points,6,3,1.5,source,sentinel,error)&&bytes(full_paths)==paths_before,"ribbon capacity atomic rejection");
 const auto save_path="/tmp/valwood-simple-landscape-"+std::to_string(getpid())+".studio";
 world_studio::Document combined=hills;check(simple_river(combined,{{400,64,20},{496,64,20}},6,3,1.5,source,report,error),"combined terrain/river");
 check(world_studio::save(save_path.c_str(),combined,error),"landscape save");world_studio::Document reopened;check(world_studio::load(save_path.c_str(),reopened,error)&&bytes(reopened)==bytes(combined),"persisted terrain and river reopen exactly");unlink(save_path.c_str());
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"terrain_tile_limit\":"<<world_studio::tile_limit<<",\"max_brush_tiles\":"<<large.terrain.size()<<",\"gpu\":false}\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<"Simple landscape failed after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
