#include "engine/world_studio.h"
#include "engine/world_navigation.h"
#include "engine/world_uniform_cache.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unistd.h>
namespace {
struct Scene {bool wall=false;};
bool ground(void *,double x,double z,double &h){h=10;return std::abs(x)<30&&std::abs(z)<13;}
float slope(double x,double z){return float(12+x*.04+z*.03);}
bool clear(void *state,const world_navigation::Point &a,const world_navigation::Point &b,float radius){bool wall=static_cast<Scene *>(state)->wall;unsigned count=unsigned(std::max(1.,std::ceil(std::hypot(a.x-b.x,a.z-b.z)/.05)));for(unsigned i=0;i<=count;i++){double t=double(i)/count,x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t;if(std::abs(x)<1+radius&&(wall||std::abs(z)<4+radius))return false;}return true;}
}
int main(){
 Scene scene;world_navigation::Query q{&scene,ground,clear};world_studio::NavigationArea area;area.span=32;area.cell=1;world_navigation::Grid grid;std::string error;assert(world_navigation::bake(area,q,grid,error));assert(grid.open>300&&grid.open<1024);std::vector<world_navigation::Point> route;assert(world_navigation::path(&grid,q,{{{-8,0}},{{8,0}}},false,.32,route,error));assert(route.size()>3&&route.front().x==route.back().x);for(unsigned i=1;i<route.size();i++)assert(clear(&scene,route[i-1],route[i],.32));assert(!world_navigation::path(nullptr,q,{{{-8,0}},{{8,0}}},false,.32,route,error));assert(!world_navigation::path(&grid,q,{{{-8,0}},{{8,14}}},false,.32,route,error));scene.wall=true;assert(world_navigation::bake(area,q,grid,error));assert(!world_navigation::path(&grid,q,{{{-8,0}},{{8,0}}},false,.32,route,error));auto old=grid.cells.size();area.cell=0;assert(!world_navigation::bake(area,q,grid,error)&&grid.cells.size()==old);
 auto tint=world_graph::preset("bep_prop_barrel",0),motion=world_graph::preset("bep_prop_bucket",2);assert(world_graph::validate(tint,error)&&world_graph::validate(motion,error));world_graph::Code code;assert(world_graph::compile({tint,motion},code,error));assert(code.surface.find("bool studioGraphSurface")!=std::string::npos&&code.vertex.find("vec3(-.15)")!=std::string::npos);assert(!world_graph::compile({tint,tint},code,error));auto cycle=tint;cycle.nodes[0].op=world_graph::Op::Multiply;cycle.nodes[0].inputs={2,2,-1};assert(!world_graph::validate(cycle,error));auto bad=tint;bad.outputs[5]=0;assert(!world_graph::validate(bad,error));bad=tint;bad.nodes[1].value[0]=std::numeric_limits<float>::infinity();assert(!world_graph::validate(bad,error));
 world_studio::Document doc;area.cell=1;doc.navigation.push_back(area);world_studio::Actor actor;actor.name="Authored traveler";actor.navigation=0;actor.points={{{-8,0}},{{8,0}}};doc.actors.push_back(actor);world_studio::Particle p;p.x=4;p.y=10;p.z=4;p.ceiling=16;p.rate=1.5;p.lifetime=2;doc.particles.push_back(p);world_studio::Water water;water.source=3;water.x=12;water.level=10.5;doc.waters.push_back(water);doc.graphs={tint,motion};assert(world_studio::valid(doc,error));char path[]="/tmp/valwood-runtime-editors-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);assert(world_studio::save(path,doc,error));world_studio::Document loaded;assert(world_studio::load(path,loaded,error));assert(loaded.navigation.size()==1&&loaded.actors[0].name==actor.name&&loaded.actors[0].points.size()==2&&loaded.particles[0].rate==1.5&&loaded.waters[0].source==3&&loaded.graphs.size()==2&&loaded.graphs[1].outputs[5]==2);unlink(path);doc.actors[0].navigation=7;assert(!world_studio::valid(doc,error));
 for(double x:{0.,1.1,127.5}){world_studio::Document pond;assert(world_studio::carve_pond(pond,x,127.3,slope,error));auto w=pond.waters.back();assert(w.level-slope(w.x,w.z)-world_studio::height_delta(pond,w.x,w.z)>.5);for(unsigned i=0;i<64;i++){double a=i*6.283185307/64,px=w.x+std::cos(a)*w.radius_x,pz=w.z+std::sin(a)*w.radius_z;assert(slope(px,pz)+world_studio::height_delta(pond,px,pz)>w.level+.035);}}
 WorldUniformCache cache;unsigned calls=0;auto lookup=[&](const char *){return int(++calls);};assert(cache.get(99,"same_name",lookup)==1&&cache.get(99,"same_name",lookup)==1);++world_program_epoch;assert(cache.get(99,"same_name",lookup)==2);
 std::puts("PASS runtime editors: obstacle detour/disconnected/water rejection, bounded grids, typed graph cycles/stages/SSA, v3 roundtrip and recycled-program invalidation");
}
