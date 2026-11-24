#include "engine/world_studio.h"
#include "engine/world_torch_flames.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <unistd.h>
using namespace world_studio;
static float base(double x,double z){return float(20+x*.01+z*.02);}
int main(){
 Document d;std::string error;
 Document triangles;triangles.terrain.push_back({});triangles.terrain[0].delta[2*65+2]=4;
 assert(valid(triangles,error));assert(height_delta(triangles,5,5)==0);assert(height_delta(triangles,4.5,4.5)==2);
 auto bad_edge=triangles;bad_edge.terrain[0].delta[64]=1;assert(!valid(bad_edge,error));
 // A stroke crossing both128m tile seams remains continuous and reversible.
 assert(brush(d,128,128,12,2,0,0,base,error));
 assert(d.terrain.size()==4);assert(std::abs(height_delta(d,128,128)-2)<1e-6);
 for(double z=117;z<140;z+=.37)assert(std::abs(height_delta(d,128-1e-5,z)-height_delta(d,128+1e-5,z))<1e-4);
 assert(height_delta(d,1e300,0)==0);
 Document raised=d;assert(brush(d,128,128,12,2,1,0,base,error));assert(std::abs(height_delta(d,128,128))<1e-6);
 History history;history.checkpoint(d);d=raised;assert(history.undo(d));assert(std::abs(height_delta(d,128,128))<1e-6);assert(history.redo(d));assert(std::abs(height_delta(d,128,128)-2)<1e-6);
 assert(brush(d,128,128,10,1,2,30,base,error));assert(std::abs(base(128,128)+height_delta(d,128,128)-30)<1e-5);
 assert(brush(d,128,128,10,.5,3,0,base,error));
 d.stamps.push_back({128,128,8,0,1});assert(vegetation(d,1,128,128)==0);assert(vegetation(d,0,128,128)==1);assert(vegetation(d,1,500,500)==1);
 std::array<float,32> scores={};scores.fill(1);auto weights=light_weights(scores,24);for(float value:weights)assert(value==1);scores.fill(0);weights=light_weights(scores,24);for(float value:weights)assert(value==0);
 auto points=scatter("starting_barrel",30,40,8,16,1,17);assert(points.size()==16);auto repeat=scatter("starting_barrel",30,40,8,16,1,17);
 for(size_t i=0;i<points.size();i++){assert(points[i].x==repeat[i].x&&points[i].z==repeat[i].z);assert(std::hypot(points[i].x-30,points[i].z-40)<=8);}
 auto alternate=scatter("starting_barrel",30,40,8,16,1,25);assert(std::hypot(points[0].x-alternate[0].x,points[0].z-alternate[0].z)>.05);
 assert(scatter("../escape",30,40,8,4,1,0).empty());
 Spline spline;spline.alias="bep_prop_fence";spline.points={{{0,0}},{{10,0}},{{10,10}}};spline.spacing=2;
 std::vector<Placement> instances;assert(build_spline(spline,instances,error));assert(instances.size()==10);assert(instances[5].x==10&&instances[5].z==0);
 spline.kind=1;assert(build_spline(spline,instances,error));assert(instances.size()>=10);for(const auto &p:instances)assert(std::isfinite(p.x)&&std::isfinite(p.z));
 d.source_signature="0123456789abcdef";d.placements=points;d.splines.push_back(spline);d.lights.push_back({});d.lights.back().cone=40;d.materials.push_back({"starting_barrel"});d.decals.push_back({});d.overrides.push_back({2,3,4,30,1,true});
 char path[]="/tmp/valwood-studio-test-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 assert(save(path,d,error));Document loaded;assert(load(path,loaded,error));assert(loaded.source_signature==d.source_signature);assert(loaded.placements.size()==16&&loaded.terrain.size()==4&&loaded.lights[0].cone==40&&loaded.overrides[0].hidden);assert(height_delta(loaded,128,128)==height_delta(d,128,128));
 // Invalid saves and malformed loads cannot clobber the last valid state.
 Document invalid=d;invalid.source_signature="not-a-scene-id";assert(!save(path,invalid,error));invalid=d;invalid.look.sun=std::numeric_limits<float>::quiet_NaN();assert(!save(path,invalid,error));assert(load(path,loaded,error));
 {std::ofstream f(path);f<<"VALWOOD_STUDIO 1\nlook 0.2 1 1 1 17 0 0 0 0 0\nprop ../../bad 0 0 0 1 0 0\n";}
 const auto old=loaded.placements.size();assert(!load(path,loaded,error));assert(loaded.placements.size()==old);
 {std::ofstream f(path);f<<"VALWOOD_STUDIO 1\nlook 0.2 1 1 1 17 0 0 0 0 0\nlight 0 0 0 8 4 1 1 1 0 0 180 2\n";}
 assert(!load(path,loaded,error));unlink(path);
 Document full;for(int i=0;i<64;i++){TerrainTile t;t.x=i*2;full.terrain.push_back(t);}auto n=full.terrain.size();assert(!brush(full,30000,0,8,1,0,0,base,error));assert(full.terrain.size()==n);
 Document elevated;elevated.placements.push_back({"fixture",0,0,0,1,0,0,100});assert(valid(elevated,error));elevated.placements[0].offset_y=WORLD_OBJECT_OFFSET_LIMIT+1;assert(!valid(elevated,error));
 Document palette;
 // Existing base flames remain individually editable beyond the old56-slot
 // pool, while the authored overlay document still has its16-emitter limit.
 Document fixtures;fixtures.particles.push_back({});fixtures.particles[0].source=64+164;
 assert(valid(fixtures,error));fixtures.particles[0].source=64+int(WORLD_TORCH_FLAME_CAPACITY)-1;assert(valid(fixtures,error));
 fixtures.particles[0].source++;assert(!valid(fixtures,error));
 fixtures.particles[0].source=-1;for(unsigned i=1;i<17;i++)fixtures.particles.push_back({});assert(!valid(fixtures,error));
 for(unsigned i=0;i<WORLD_CUSTOM_ASSET_LIMIT;i++){Asset a;a.alias="fixture_"+std::to_string(i);a.stem="imports/fixture/asset";palette.assets.push_back(a);}
 assert(valid(palette,error));char palette_path[]="/tmp/valwood-studio-palette-XXXXXX";fd=mkstemp(palette_path);assert(fd>=0);close(fd);
 assert(save(palette_path,palette,error));Document palette_loaded;assert(load(palette_path,palette_loaded,error));assert(palette_loaded.assets.size()==WORLD_CUSTOM_ASSET_LIMIT);
 palette.assets.push_back(palette.assets.back());assert(!save(palette_path,palette,error));assert(load(palette_path,palette_loaded,error));assert(palette_loaded.assets.size()==WORLD_CUSTOM_ASSET_LIMIT);unlink(palette_path);
 std::puts("world_studio: seams, brushes, masks, scatter, splines, history, atomic persistence, imported palette capacity and malformed records passed");
}
