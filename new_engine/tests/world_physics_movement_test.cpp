#include "engine/world_physics.h"
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <unistd.h>

static std::atomic<bool> tracking{false};
static std::atomic<unsigned> allocations{0};
[[gnu::noinline]] void *operator new(std::size_t size){if(tracking)++allocations;if(void *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](std::size_t size){return ::operator new(size);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,std::size_t)noexcept{std::free(p);}
using namespace world_physics;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
static Body box(Id id,Vec3 center,Vec3 size){Body b;b.id=id;b.pose.position=center;b.size=size;return b;}
int main(){
    std::string error;Document scene;
    // A capsule overlaps a doorway jamb by 1 mm. Escape and travel along the
    // jamb must work; pressing farther into the jamb must still be blocked.
    scene.bodies={box(1,{-.5,1,0},{.5,2,10})};World world;CHECK(world.configure(scene,error));
    const Vec3 center={.349,.8999,0};Hit hit;
    CHECK(world.cast_capsule(center,.35f,.55f,{.1,0,0},hit)&&hit.hit&&hit.body==1&&hit.fraction==0);
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{.1,0,0},hit)&&!hit.hit);
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{0,0,.1},hit)&&!hit.hit);
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{-.1,0,0},hit)&&hit.hit&&hit.body==1&&hit.fraction==0&&hit.normal.x>.99);
    // A tiny ground penetration is tangent to horizontal motion. Discarding
    // the initial contact must happen before closest-hit pruning, otherwise
    // it hides a second, perpendicular wall farther along the same sweep.
    scene.bodies.push_back(box(2,{0,-1,0},{20,1,20}));
    scene.bodies.push_back(box(3,{0,1,2},{5,2,.1}));CHECK(world.configure(scene,error));
    CHECK(world.cast_capsule(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body!=3&&hit.fraction==0);
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==3&&hit.fraction>.35f&&hit.fraction<.45f&&hit.normal.z<-.99);
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{0,-.1,0},hit)&&hit.hit&&hit.body==2&&hit.fraction==0&&hit.normal.y>.99);
    // Sensor/layer/ignore filters and validation retain the generic contracts.
    CHECK(world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit,3)&&!hit.hit);
    CHECK(!world.cast_capsule_movement(center,-1,.55f,{0,0,4},hit));
    CHECK(!world.cast_capsule_movement(center,.35f,.55f,{std::numeric_limits<double>::infinity(),0,0},hit));
    CHECK(!world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit,0,layer_count));
    bool queries=true;allocations=0;tracking=true;
    for(unsigned i=0;i<1200;++i){queries=queries&&world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==3;}
    tracking=false;CHECK(queries&&allocations==0&&world.stats().query_allocations==0);
    std::cout<<"movement: initial overlap escape, tangent slide, inward contact, ground contact and later wall; 1200 real Jolt queries, zero heap/scratch fallback\n";

    // Grandstone floors and walls share one triangle mesh, so also prove that
    // an initial face in the same mesh cannot mask a future wall face.
    {
        World mesh_world;Document room;Body walls;walls.id=80;walls.shape=Shape::Mesh;
        walls.vertices={{-20,0,-20},{20,0,-20},{20,0,20},{-20,0,20},
                        {0,0,-10},{0,4,-10},{0,4,10},{0,0,10},
                        {-5,0,1.9},{-5,4,1.9},{5,4,1.9},{5,0,1.9}};
        walls.triangles={{{0,2,1}},{{0,3,2}},{{4,5,6}},{{4,6,7}},{{8,9,10}},{{8,10,11}}};
        room.bodies.push_back(std::move(walls));CHECK(mesh_world.configure(room,error));
        const Vec3 jamb_center={.349,1,0};
        CHECK(mesh_world.cast_capsule(jamb_center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.fraction==0);
        CHECK(mesh_world.cast_capsule_movement(jamb_center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==80&&hit.fraction>.35f&&hit.fraction<.45f&&hit.normal.z<-.99);
        CHECK(mesh_world.cast_capsule_movement(jamb_center,.35f,.55f,{.1,0,0},hit)&&!hit.hit);
        CHECK(mesh_world.cast_capsule_movement(jamb_center,.35f,.55f,{-.1,0,0},hit)&&hit.hit&&hit.fraction==0&&hit.normal.x>.99);
        CHECK(mesh_world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==80&&hit.fraction>.35f&&hit.fraction<.45f&&hit.normal.z<-.99);
        const Vec3 back_center={-.349,1,0};
        CHECK(mesh_world.cast_capsule_movement(back_center,.35f,.55f,{-.1,0,0},hit)&&!hit.hit);
        CHECK(mesh_world.cast_capsule_movement(back_center,.35f,.55f,{.1,0,0},hit)&&hit.hit&&hit.fraction==0&&hit.normal.x<-.99);
        CHECK(mesh_world.cast_capsule_movement(back_center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==80&&hit.fraction>.35f&&hit.fraction<.45f&&hit.normal.z<-.99);
        const Vec3 underside={2,-.8999,0};
        CHECK(mesh_world.cast_capsule_movement(underside,.35f,.55f,{0,-.1,0},hit)&&!hit.hit);
        CHECK(mesh_world.cast_capsule_movement(underside,.35f,.55f,{0,.1,0},hit)&&hit.hit&&hit.fraction==0&&hit.normal.y<-.99);
        CHECK(mesh_world.cast_capsule_movement(underside,.35f,.55f,{0,0,1},hit)&&!hit.hit);
        allocations=0;tracking=true;queries=true;
        for(unsigned i=0;i<1200;++i){queries=queries&&mesh_world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.normal.z<-.99;}
        tracking=false;CHECK(queries&&allocations==0&&mesh_world.stats().query_allocations==0);
    }

    // Both geometry limits are aggregate document budgets. The expanded exact
    // limits round-trip through the real codec; overflow leaves prior data and
    // an already configured scene intact. Body/file budgets remain unchanged.
    static_assert(vertex_limit==147456&&triangle_limit==294912);
    static_assert(body_limit==512&&document_byte_limit==16*1024*1024);
    Document mesh;Body floor;floor.id=100;floor.shape=Shape::Mesh;
    floor.vertices.resize(vertex_limit,{0,0,0});floor.vertices[1]={1,0,0};floor.vertices[2]={0,0,1};
    floor.triangles.resize(triangle_limit,{{0,2,1}});mesh.bodies.push_back(std::move(floor));CHECK(validate(mesh,error));
    char folder[]="/tmp/valwood-physics-movement-XXXXXX";CHECK(mkdtemp(folder));
    const auto path=std::string(folder)+"/mesh.physics",badpath=std::string(folder)+"/bad.physics";
    CHECK(save(path.c_str(),mesh,error));Document loaded;CHECK(load(path.c_str(),loaded,error));
    CHECK(loaded.bodies.size()==1&&loaded.bodies[0].vertices.size()==vertex_limit&&loaded.bodies[0].triangles.size()==triangle_limit);
    mesh.bodies[0].vertices.push_back({0,0,0});CHECK(!validate(mesh,error)&&!save(path.c_str(),mesh,error));mesh.bodies[0].vertices.pop_back();
    mesh.bodies[0].triangles.push_back({{0,2,1}});CHECK(!validate(mesh,error));mesh.bodies[0].triangles.pop_back();
    Body extra;extra.id=101;extra.shape=Shape::Mesh;extra.vertices={{0,0,0},{1,0,0},{0,0,1}};extra.triangles={{{0,2,1}}};mesh.bodies.push_back(extra);CHECK(!validate(mesh,error));
    CHECK(!world.configure(mesh,error));CHECK(world.cast_capsule_movement(center,.35f,.55f,{0,0,4},hit)&&hit.hit&&hit.body==3);
    // A codec record beyond the exact vertex or triangle cap is rejected at
    // the parse boundary, with the caller's previous document untouched.
    for(const std::string &record:{std::string("vertex 100 0 0 0\n"),std::string("triangle 100 0 2 1\n")}){
        std::ifstream in(path,std::ios::binary);std::ofstream out(badpath,std::ios::binary);out<<in.rdbuf()<<record;out.close();
        CHECK(!load(badpath.c_str(),loaded,error));CHECK(loaded.bodies.size()==1&&loaded.bodies[0].vertices.size()==vertex_limit&&loaded.bodies[0].triangles.size()==triangle_limit);
    }
    {std::ofstream out(badpath,std::ios::binary|std::ios::trunc);out.seekp(document_byte_limit);out.put('x');}
    CHECK(!load(badpath.c_str(),loaded,error)&&loaded.bodies[0].vertices.size()==vertex_limit);
    Document bodies;for(unsigned i=0;i<=body_limit;++i)bodies.bodies.push_back(box(1000+i,{double(i)*2,0,0},{.5,.5,.5}));CHECK(!validate(bodies,error));
    CHECK(unlink(path.c_str())==0&&unlink(badpath.c_str())==0&&rmdir(folder)==0);CHECK(world.stop());
    std::cout<<"budgets: exact expanded vertex/triangle codec round-trip, aggregate overflow, atomic rejection, unchanged body/file caps; "<<checks<<" checks passed\n";
}
