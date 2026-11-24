#include "engine/world_playtest_walk.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <string>
#include <unistd.h>
using namespace world_playtest_walk;
static void floor(world_physics::Body &b,double x0,double x1,double z0,double z1,double y){
    const auto i=std::uint32_t(b.vertices.size());
    b.vertices.insert(b.vertices.end(),{{x0,y,z0},{x0,y,z1},{x1,y,z1},{x1,y,z0}});
    b.triangles.push_back({i,i+1,i+2});b.triangles.push_back({i,i+2,i+3});
}
int main(){
    namespace fs=std::filesystem;
    const auto dir=fs::temp_directory_path()/("valwood-playtest-walk-"+std::to_string(getpid()));fs::create_directory(dir);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{dir};
    world_physics::Document d;world_physics::Body mesh;mesh.id=1;mesh.shape=world_physics::Shape::Mesh;
    floor(mesh,-4,4,-4,1,0);floor(mesh,-4,-1,1,4,0);floor(mesh,1,4,1,4,0);floor(mesh,-1,1,3,4,0);
    floor(mesh,-3,3,-3,0,4);d.bodies.push_back(mesh);
    world_physics::Body wall;wall.id=2;wall.pose.position={0,1.5,-2};wall.size={.1,1.5,2};d.bodies.push_back(wall);
    world_physics::Body step;step.id=3;step.pose.position={-2,.15,.25};step.size={1,.15,.25};d.bodies.push_back(step);
    world_physics::Body ceiling;ceiling.id=4;ceiling.pose.position={2,1.8,-2};ceiling.size={.5,.15,.5};d.bodies.push_back(ceiling);
    world_physics::Body cross_wall;cross_wall.id=5;cross_wall.pose.position={-.35,1.5,-.9};cross_wall.size={.45,1.5,.1};d.bodies.push_back(cross_wall);
    std::string error;assert(world_physics::save((dir/"test.physics").string().c_str(),d,error));
    auto settings=[&](const std::string &suffix){std::ofstream f(dir/"test.settings");f<<"VALWOOD_PLAYTEST_WALK 1\nphysics \"test.physics\"\nbounds -4 4 -4 4 0 6\n"<<suffix;};
    settings("spawn -2 0 -2 0 -.03\n");Support support;
    assert(support.load((dir/"test.settings").string().c_str(),error));
    float h=123;assert(support.sample(-2,-2,0,h)&&std::abs(h)<1.e-5);
    assert(support.sample(-2,-2,4,h)&&std::abs(h-4)<1.e-5); // sameXZ, separate storeys
    assert(support.sample(-2,-2,1.8,h)&&std::abs(h)<1.e-5); // ordinary jump
    h=123;assert(!support.sample(0,2,0,h)&&h==123); // real void: no atlas fallback
    assert(!support.sample(3.9,0,0,h)); // full capsule remains inside audited bounds
    assert(support.can_move(-2,-2,-1,-2,0,h)&&std::abs(h)<1.e-5);
    assert(!support.can_move(-1,-2,1,-2,0,h)); // actual wall, not just slope
    assert(support.can_move(-.45,-2,-.65,-2,0,h)); // recover out of a small initial wall overlap
    assert(!support.can_move(-.45,-2,-.30,-2,0,h)); // overlap recovery cannot move farther through the wall
    assert(support.can_move(-.45,-2,-.45,-1.8,0,h)); // retain a tangent slide along a doorway jamb
    assert(!support.can_move(-.30,-1.5,-.50,-1.1,0,h)); // recovering from one wall must still see a second wall
    assert(support.can_move(-2,-.4,-2,.4,0,h)&&std::abs(h-.3)<1.e-5); // real stair riser
    // A ledge outside the actual body must not manufacture a higher floor.
    // The former .40m sampling ring raised this clear .39m corridor by .3m
    // and rejected even parallel movement before making a capsule query.
    assert(support.sample(-2,-.39,0,h)&&std::abs(h)<1.e-5);
    assert(support.can_move(-2,-.39,-1.5,-.39,0,h)&&std::abs(h)<1.e-5);
    // Approach and retreat through the rounded stair edge at ordinary frame
    // distances, including diagonal/lateral movement beside the same tread.
    double stair_feet=0;
    for(int i=1;i<=23;i++){
        const double az=-.8+(i-1)*.05,bz=-.8+i*.05;
        assert(support.can_move(-2,az,-2,bz,stair_feet,h));stair_feet=h;
    }
    assert(std::abs(stair_feet-.3)<1.e-5);
    assert(support.can_move(-2,.35,-1.7,.30,stair_feet,h));stair_feet=h;
    for(int i=1;i<=22;i++){
        const double az=.30-(i-1)*.05,bz=.30-i*.05;
        assert(support.can_move(-1.7,az,-1.7,bz,stair_feet,h));stair_feet=h;
    }
    assert(std::abs(stair_feet)<1.e-5);
    assert(!support.can_move(2,-3,2,-2,0,h)); // low overhead clearance
    assert(!support.can_move(-1,.8,0,2,0,h)); // cannot walk into unsupported hole
    assert(support.can_move(-2,-2,-1,-2,1.5,h)&&std::abs(h)<1.e-5); // horizontal while airborne
    const auto original=support.spawn();
    settings("spawn 2 0 -2 0 -.03\n");assert(!support.load((dir/"test.settings").string().c_str(),error));
    assert(support.loaded()&&support.spawn().x==original.x&&support.sample(-2,-2,0,h));
    settings("spawn -2 0 -2 0 -.03\nspawn 0 0 0 0 0\n");assert(!support.load((dir/"test.settings").string().c_str(),error));
    settings("spawn -2 nan -2 0 0\n");assert(!support.load((dir/"test.settings").string().c_str(),error));
    std::ofstream(dir/"bad.settings")<<"VALWOOD_PLAYTEST_WALK 1\nphysics \"../test.physics\"\nbounds -4 4 -4 4 0 6\nspawn -2 0 -2 0 0\n";
    assert(!support.load((dir/"bad.settings").string().c_str(),error));
    assert(support.stats().query_allocations==0);
    // Multiple genuine risers must remain traversable at frame-sized steps,
    // both up and down, while an over-height ledge stays a blocking obstacle.
    world_physics::Document stairs;world_physics::Body stairs_floor;stairs_floor.id=1;
    stairs_floor.shape=world_physics::Shape::Mesh;
    floor(stairs_floor,-4,4,-4,4,0);floor(stairs_floor,-3,-1,2.5,4,.9);
    const auto bevel=std::uint32_t(stairs_floor.vertices.size());
    stairs_floor.vertices.insert(stairs_floor.vertices.end(),{{1,-.1,-2.1},{3,-.1,-2.1},{3,.7,-1.9},{1,.7,-1.9}});
    stairs_floor.triangles.push_back({bevel,bevel+2,bevel+1});stairs_floor.triangles.push_back({bevel,bevel+3,bevel+2});
    stairs.bodies.push_back(stairs_floor);
    for(unsigned i=0;i<5;i++){
        world_physics::Body tread;tread.id=i+2;tread.pose.position={-2,(i+1)*.09,i*.5+.25};
        tread.size={1,(i+1)*.09,.25};stairs.bodies.push_back(tread);
    }
    world_physics::Body high_step;high_step.id=9;high_step.pose.position={2,.35,.5};
    high_step.size={.6,.35,.5};stairs.bodies.push_back(high_step);
    world_physics::Body high_drop;high_drop.id=10;high_drop.pose.position={0,.45,1.5};
    high_drop.size={.6,.45,.5};stairs.bodies.push_back(high_drop);
    assert(world_physics::save((dir/"test.physics").string().c_str(),stairs,error));
    settings("spawn -2 0 -2 0 -.03\n");Support stair_support;
    assert(stair_support.load((dir/"test.settings").string().c_str(),error));
    assert(stair_support.sample(2,-2,0,h)); // steep riser bevel cannot hide the actual source floor beneath it
    stair_feet=0;
    for(int i=1;i<=70;i++){
        const double az=-.75+(i-1)*.05,bz=-.75+i*.05;
        assert(stair_support.can_move(-2,az,-2,bz,stair_feet,h));stair_feet=h;
    }
    assert(std::abs(stair_feet-.9)<1.e-5);
    for(int i=1;i<=70;i++){
        const double az=2.75-(i-1)*.05,bz=2.75-i*.05;
        assert(stair_support.can_move(-2,az,-2,bz,stair_feet,h));stair_feet=h;
    }
    assert(std::abs(stair_feet)<1.e-5);
    assert(!stair_support.can_move(2,-.75,2,.6,0,h));
    assert(stair_support.can_move(2,.6,2,-.75,.7,h)&&std::abs(h)<1.e-5); // a .7m drop stays inside the .8m limit
    assert(!stair_support.can_move(0,1.5,0,.25,.9,h)); // .9m source tread drop must stay blocked
    assert(stair_support.stats().query_allocations==0);
    // Source architecture is a single triangle mesh, including narrow stone
    // joints and both sides of a wall/floor plane, rather than separate boxes.
    world_physics::Document joined;world_physics::Body joined_mesh;joined_mesh.id=1;
    joined_mesh.shape=world_physics::Shape::Mesh;
    floor(joined_mesh,-4,-.01,-4,4,0);floor(joined_mesh,.01,4,-4,4,0);
    floor(joined_mesh,-3,-1,-2,-1,1.6);
    const auto jamb=std::uint32_t(joined_mesh.vertices.size());
    joined_mesh.vertices.insert(joined_mesh.vertices.end(),{{2,0,-4},{2,3,-4},{2,3,4},{2,0,4}});
    joined_mesh.triangles.push_back({jamb,jamb+1,jamb+2});joined_mesh.triangles.push_back({jamb,jamb+2,jamb+3});
    joined.bodies.push_back(joined_mesh);
    assert(world_physics::save((dir/"test.physics").string().c_str(),joined,error));
    settings("spawn -2 0 -3 0 -.03\n");Support joint_support;
    assert(joint_support.load((dir/"test.settings").string().c_str(),error));
    assert(joint_support.sample(0,0,0,h)&&std::abs(h)<.005); // capsule physically bridges a 2cm stone joint
    assert(joint_support.can_move(-.2,0,.2,0,0,h)&&std::abs(h)<.005);
    assert(joint_support.can_move(1.65,0,1.4,0,0,h)); // overlapping the mesh's back-facing jamb can escape
    assert(joint_support.can_move(2.35,0,2.6,0,0,h)); // front-facing jamb can escape too
    assert(!joint_support.can_move(1.65,0,1.8,0,0,h));
    assert(!joint_support.can_move(2.35,0,2.2,0,0,h));
    assert(!joint_support.can_move(-2,-3,-2,-1.5,0,h)); // upper floor underside still blocks the head
    assert(joint_support.can_move(-2,-1.5,-2,-3,0,h)); // an initial underside overlap can escape
    assert(joint_support.stats().query_allocations==0);
    std::puts("PASS playtest walking: storeys/jump reference, rounded stairs/seams/stone joints, mesh jamb escape, climb/drop limits, walls/headroom, void/bounds, atomic settings, zero Jolt query heap fallback");
}
