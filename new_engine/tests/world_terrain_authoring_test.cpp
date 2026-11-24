#include "engine/world_terrain_authoring.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <unistd.h>
namespace {
float flat(double,double){return 20;}
float cone(double x,double z){return float(30+12*std::exp(-((x-64)*(x-64)+(z-64)*(z-64))/18));}
float hill(double x,double z){return float(40-.05*x-.03*z+4*std::exp(-((x-60)*(x-60)+(z-60)*(z-60))/180));}
float steep(double x,double){return float(20+x);}
float invalid(double,double){return std::numeric_limits<float>::quiet_NaN();}
bool protect(double x,double z,const void *){return std::hypot(x-64,z-64)<6;}
bool near(double x,double y,double epsilon=1e-4){return std::abs(x-y)<epsilon;}
std::string read(const std::string &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::string bytes(const world_studio::Document &d){auto p="/tmp/valwood-terrain-test-doc-"+std::to_string(getpid());std::string e;assert(world_studio::save(p.c_str(),d,e));auto s=read(p);unlink(p.c_str());return s;}
void equal_edges(const world_studio::Document &d){std::string e;assert(world_studio::valid(d,e));for(const auto &t:d.terrain)for(const auto &n:d.terrain)if(n.x==t.x+1&&n.z==t.z)for(unsigned i=0;i<65;i++)assert(t.delta[i*65+64]==n.delta[i*65]);}
}
int main(){
 using namespace world_terrain_authoring;world_studio::Document original;std::string e;Region region{0,0,65,65};Heightmap m;Source source{flat};Report report;
 assert(sample_heightmap(original,region,0,100,source,m,e)&&m.samples.size()==4225&&m.samples[0]==13107);
 const auto p="/tmp/valwood-terrain-test-"+std::to_string(getpid())+".pgm";assert(save_heightmap(p.c_str(),m,e));auto file=read(p);assert(file.substr(0,3)=="P5\n"&&static_cast<unsigned char>(file[file.size()-2])==0x33&&static_cast<unsigned char>(file.back())==0x33);Heightmap loaded;assert(load_heightmap(p.c_str(),loaded,e)&&loaded.samples==m.samples&&loaded.region.x==m.region.x&&loaded.maximum==100);
 auto bad=m;bad.maximum=bad.minimum;assert(!save_heightmap(p.c_str(),bad,e)&&read(p)==file);{std::ofstream f(p,std::ios::binary);f<<file.substr(0,file.size()-1);}loaded.maximum=42;assert(!load_heightmap(p.c_str(),loaded,e)&&loaded.maximum==42);{std::ofstream f(p,std::ios::binary);f<<file<<"trailing";}assert(!load_heightmap(p.c_str(),loaded,e)&&loaded.maximum==42);unlink(p.c_str());
 std::fill(m.samples.begin(),m.samples.end(),std::uint16_t(std::llround(.3*65535)));world_studio::Document imported;assert(import_heightmap(imported,m,{},source,report,e)&&report.changed_nodes>3000&&report.added_tiles==1&&report.largest_change>9.99);assert(near(world_studio::height_delta(imported,64,64),10,.001));assert(world_studio::height_delta(imported,0,64)==0&&world_studio::height_delta(imported,128,64)==0);equal_edges(imported);
 Heightmap exported;assert(sample_heightmap(imported,region,0,100,source,exported,e));assert(std::abs(double(exported.samples[32*65+32])*100/65535.-30)<.002);
 const auto snapshot=bytes(imported);auto protected_map=m;std::fill(protected_map.samples.begin(),protected_map.samples.end(),std::uint16_t(26214));assert(!import_heightmap(imported,protected_map,{},Source{flat,protect},report,e)&&bytes(imported)==snapshot);assert(!import_heightmap(imported,m,{},Source{invalid},report,e)&&bytes(imported)==snapshot);
 // A region crossing a128m boundary writes the exact same shared sample into
 // both tiles, including existing neighbors, with its exterior held fixed.
 Heightmap cross;assert(sample_heightmap(imported,{120,0,17,65},0,100,source,cross,e));std::fill(cross.samples.begin(),cross.samples.end(),std::uint16_t(26214));assert(import_heightmap(imported,cross,{},source,report,e));equal_edges(imported);assert(world_studio::height_delta(imported,128,64)>19.9f);
 // Thermal relaxation lowers the summit and deposits nearby, deterministically.
 Erosion thermal;thermal.region=region;thermal.iterations=24;thermal.rain=0;thermal.thermal=1;thermal.talus=0;world_studio::Document eroded,repeat;assert(erode(eroded,thermal,Source{cone},report,e)&&report.changed_nodes>100);assert(cone(64,64)+world_studio::height_delta(eroded,64,64)<cone(64,64)-1);assert(erode(repeat,thermal,Source{cone},report,e)&&bytes(eroded)==bytes(repeat));equal_edges(eroded);
 // Hydraulic-only flow transfers sediment down an uneven incline and responds
 // reproducibly to the rain seed; this exercises actual erosion, not a blur.
 Erosion hydraulic;hydraulic.region=region;hydraulic.iterations=64;hydraulic.rain=.12f;hydraulic.capacity=8;hydraulic.erosion=.7f;hydraulic.deposition=.2f;hydraulic.thermal=0;hydraulic.seed=451;world_studio::Document hydro,hydro2;assert(erode(hydro,hydraulic,Source{hill},report,e)&&report.changed_nodes>100&&report.largest_change>.005);assert(erode(hydro2,hydraulic,Source{hill},report,e)&&bytes(hydro)==bytes(hydro2));hydraulic.seed++;world_studio::Document other;assert(erode(other,hydraulic,Source{hill},report,e)&&bytes(other)!=bytes(hydro));equal_edges(hydro);
 auto erosion_before=bytes(hydro);hydraulic.iterations=257;assert(!erode(hydro,hydraulic,Source{hill},report,e)&&bytes(hydro)==erosion_before);hydraulic.iterations=12;assert(!erode(hydro,hydraulic,Source{hill,protect},report,e)&&bytes(hydro)==erosion_before);
 // Biome paint is a real persisted per-layer density mask with conservatively
 // sampled height/slope constraints, rather than a preview-only raster.
 Biome b;b.x=64;b.z=64;b.radius=12;b.layer=1;b.density=0;b.minimum_height=19;b.maximum_height=21;b.maximum_slope=5;world_studio::Document biome;assert(paint_biome(biome,b,source,report,e)&&report.added_masks>0&&report.added_masks<=9);assert(world_studio::vegetation(biome,1,64,64)<.01f&&world_studio::vegetation(biome,0,64,64)==1);auto before=bytes(biome);assert(!paint_biome(biome,b,Source{steep},report,e)&&bytes(biome)==before);b.minimum_height=50;b.maximum_height=60;assert(!paint_biome(biome,b,source,report,e)&&bytes(biome)==before);
 // Road cut/fill follows control heights; shoulders fade back to undisturbed
 // terrain. River controls descend and create an actual depressed channel.
 Corridor road;road.points={{32,64,22},{96,64,24}};road.vegetation_layers=3;world_studio::Document roads;assert(sculpt_corridor(roads,road,source,report,e)&&report.added_masks>0&&report.added_masks<=32);assert(near(flat(64,64)+world_studio::height_delta(roads,64,64),23)&&near(world_studio::height_delta(roads,64,80),0));equal_edges(roads);
 Corridor river;river.kind=CorridorKind::River;river.points={{32,96,22},{96,96,20}};river.depth=3;river.vegetation_layers=0;world_studio::Document rivers;assert(sculpt_corridor(rivers,river,source,report,e));assert(near(flat(64,96)+world_studio::height_delta(rivers,64,96),18)&&near(world_studio::height_delta(rivers,64,110),0));auto river_before=bytes(rivers);std::swap(river.points[0],river.points[1]);assert(!sculpt_corridor(rivers,river,source,report,e)&&bytes(rivers)==river_before);
 auto roads_before=bytes(roads);assert(!sculpt_corridor(roads,road,Source{flat,protect},report,e)&&bytes(roads)==roads_before);
 // Capacity, invalid inputs and protected edits never publish partial tiles,
 // masks, reports or broken shared boundaries.
 world_studio::Document full;for(int i=0;i<16;i++){world_studio::TerrainTile t;t.x=i;t.z=10;full.terrain.push_back(t);}auto full_before=bytes(full);Report sentinel;sentinel.changed_nodes=123;assert(!import_heightmap(full,m,{},source,sentinel,e)&&bytes(full)==full_before&&sentinel.changed_nodes==123);
 auto invalid_map=m;invalid_map.region.x=1;assert(!import_heightmap(full,invalid_map,{},source,sentinel,e)&&bytes(full)==full_before);Heightmap too_small;assert(!sample_heightmap(original,region,0,10,source,too_small,e));
 world_studio::Document masks;for(unsigned i=0;i<32;i++)masks.stamps.push_back({double(i),0,1,0,0});auto masks_before=bytes(masks);b.minimum_height=19;b.maximum_height=21;assert(!paint_biome(masks,b,source,sentinel,e)&&bytes(masks)==masks_before);assert(!sculpt_corridor(masks,road,source,sentinel,e)&&bytes(masks)==masks_before);
 // Existing manual undo stores these same Documents and restores exactly.
 world_studio::History history;world_studio::Document undoable;history.checkpoint(undoable);assert(import_heightmap(undoable,m,{},source,report,e));assert(history.undo(undoable)&&undoable.terrain.empty());assert(history.redo(undoable)&&undoable.terrain.size()==1);
 std::cout<<"Terrain authoring:16-bit PGM roundtrip, shared-edge height import, deterministic hydraulic/thermal erosion, constrained biome masks, graded roads/downhill rivers, protected areas, budgets, atomic failures and undo passed\n";
}
